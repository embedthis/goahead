/*
    route.c -- Route Management

    This module implements the loading of a route configuration file
    and the routing of requests.

    The route configuration is loaded from a text file of space-separated key=value directives
    (see route.txt). The supported directives are:
        route uri=URI [protocol=PROTO] [methods=METHOD,...] [handler=NAME] [extensions=EXT,...]
              [abilities=ABILITY,...] [redirect=STATUS@URI] [dir=DIR] [auth=TYPE]
        user name=NAME password=PASSWORD roles=ROLE,...
        role name=NAME abilities=ABILITY,...

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static WebsRoute **routes = 0;
static WebsHash  handlers = -1;
static int       routeCount = 0;
static int       routeMax = 0;

#define WEBS_MAX_ROUTE 16               /* Maximum passes over route set */

/********************************** Forwards **********************************/

static bool continueHandler(Webs *wp);
static void freeRoute(WebsRoute *route);
static void growRoutes(void);
static int lookupRoute(cchar *uri);
static bool redirectHandler(Webs *wp);

/************************************ Code ************************************/
/*
    Compare a request path against a route prefix.

    On a case-insensitive file system the kernel resolves a case-variant path to the same file, so
    the route gate must fold case too. Otherwise /AUTH/ selects the unauthenticated catch-all route
    while the subsequent open() serves the protected /auth/ document, and the authorization decision
    and the file open are made on two different strings naming one file. Folding here rather than in
    wp->path keeps the client's spelling for open(), PATH_INFO and REQUEST_URI.

    This folds ASCII case only. The other folding axes that the file system applies - NFD/NFC and
    Win32 trailing dot, space, ADS and 8.3 aliases - are rejected as non-canonical when the request
    path is created. See validCanonicalPath in http.c.
 */
static int matchRoutePrefix(cchar *path, cchar *prefix, ssize len)
{
#if ME_GOAHEAD_CASE_INSENSITIVE_FS
    return sncaselesscmp(path, prefix, len);
#else
    return sncmp(path, prefix, len);
#endif
}


/*
    Route the request. If wp->route is already set, test routes after that route
 */

PUBLIC void websRouteRequest(Webs *wp)
{
    WebsRoute   *route;
    WebsHandler *handler;
    ssize       plen, len;
    bool        safeMethod;
    int         i;

    assert(wp);
    assert(wp->path);
    assert(wp->method);
    assert(wp->protocol);

    /*
        Entering with a route set means this is a re-route: a handler declined and websPump routed again.
        Every re-entry counts against the loop limit, however it was reached. Without this, a handler that
        declines forever spins the event loop and the single threaded server never services another
        connection.
     */
    if (wp->route && ++wp->routeCount >= WEBS_MAX_ROUTE) {
        wp->route = 0;
        error("Route loop for %s", wp->url);
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot find suitable route for request.");
        return;
    }
    safeMethod = smatch(wp->method, "POST") || smatch(wp->method, "GET") || smatch(wp->method, "HEAD");
    plen = slen(wp->path);

    /*
        Resume routing from after the last matched route. This permits the legacy service() callbacks to
        return false and continue routing. A resumed scan that runs off the end means no further route
        matched, so the scan below does not run and the request fails with a 404. It must never wrap back
        to the start: that re-offers routes the request has already been given.
     */
    if (wp->route && !(wp->flags & WEBS_REROUTE)) {
        for (i = 0; i < routeCount; i++) {
            if (wp->route == routes[i]) {
                i++;
                break;
            }
        }
    } else {
        i = 0;
    }
    wp->route = 0;

    for (; i < routeCount; i++) {
        route = routes[i];
        assert(route->prefix && route->prefixLen > 0);

        if (plen < route->prefixLen) continue;
        len = min(route->prefixLen, plen);
        trace(5, "Examine route %s", route->prefix);
        /*
            Match route
         */
        if (matchRoutePrefix(wp->path, route->prefix, len) != 0) {
            continue;
        }
        if (route->protocol && !smatch(route->protocol, wp->protocol)) {
            trace(5, "Route %s does not match protocol %s", route->prefix, wp->protocol);
            continue;
        }
        if (route->methods >= 0) {
            if (!hashLookup(route->methods, wp->method)) {
                trace(5, "Route %s does not match method %s", route->prefix, wp->method);
                continue;
            }
        } else if (!safeMethod) {
            continue;
        }
        if (route->extensions >= 0 && (wp->ext == 0 || !hashLookup(route->extensions, &wp->ext[1]))) {
            trace(5, "Route %s doesn match extension %s", route->prefix, wp->ext ? wp->ext : "");
            continue;
        }

        wp->route = route;
#if ME_GOAHEAD_AUTH
        /*
            Clear wp->route on a deny so that "a route was selected" can never be read as "the
            request was authorized". Any caller that resumes from wp->route after this point must
            find nothing to dispatch.
         */
        if (route->authType && !websAuthenticate(wp)) {
            wp->route = 0;
            return;
        }
        if (route->abilities >= 0 && !websCan(wp, route->abilities)) {
            wp->route = 0;
            return;
        }
#endif
        if ((handler = route->handler) == 0) {
            continue;
        }
        if (!handler->match || (*handler->match)(wp)) {
            /* Handler matches */
            return;
        }
        wp->route = 0;
        if (wp->flags & WEBS_REROUTE) {
            wp->flags &= ~WEBS_REROUTE;
            if (++wp->routeCount >= WEBS_MAX_ROUTE) {
                break;
            }
            i = 0;
        }
    }
    if (wp->routeCount >= WEBS_MAX_ROUTE) {
        error("Route loop for %s", wp->url);
    }
    websError(wp, HTTP_CODE_NOT_FOUND, "Cannot find suitable route for request.");
    assert(wp->route == 0);
}


PUBLIC bool websRunRequest(Webs *wp)
{
    WebsRoute *route;

    assert(wp);
    assert(wp->path);
    assert(wp->method);
    assert(wp->protocol);
    assert(wp->route);

    /*
        Never dispatch a handler for a request whose response has already been written. The state
        machine is the primary guard; this is defence in depth for any future caller.
     */
    if (wp->finalized) {
        return 1;
    }
    if ((route = wp->route) == 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no route for request");
        return 1;
    }
    if (!route->handler) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no handler for route");
        return 1;
    }
    if (!wp->filename || route->dir) {
        wfreeset(wp->filename);
        wp->filename = sfmt("%s%s", route->dir ? route->dir : websGetDocuments(), wp->path);
    }
    if (!(wp->flags & WEBS_VARS_ADDED)) {
        if (wp->query && *wp->query) {
            websSetQueryVars(wp);
        }
        if (wp->flags & WEBS_FORM) {
            websSetFormVars(wp);
        }
        wp->flags |= WEBS_VARS_ADDED;
    }
    wp->state = WEBS_RUNNING;
    trace(5, "Route %s calls handler %s", route->prefix, route->handler->name);

#if ME_GOAHEAD_LEGACY
    if (route->handler->flags & WEBS_LEGACY_HANDLER) {
        return (*(WebsLegacyHandlerProc) route->handler->service)(wp, route->prefix, route->dir, route->flags) == 0;
    } else
#endif
    if (!route->handler->service) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no handler service callback");
        return 1;
    }
    return (*route->handler->service)(wp);
}


#if ME_GOAHEAD_AUTH
static bool can(Webs *wp, char *ability)
{
    assert(wp);
    assert(ability && *ability);

    if (wp->user && hashLookup(wp->user->abilities, ability)) {
        return 1;
    }
    return 0;
}


PUBLIC bool websCan(Webs *wp, WebsHash abilities)
{
    WebsKey *key;
    char    *ability, *cp, *start, abuf[ME_GOAHEAD_LIMIT_STRING];

    assert(wp);
    assert(abilities >= 0);

    if (!wp->user) {
        if (wp->authType) {
            if (!wp->username) {
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. User not logged in.");
                return 0;
            }
            if ((wp->user = websLookupUser(wp->username)) == 0) {
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Unknown user.");
                return 0;
            }
        }
    }
    if (abilities >= 0) {
        if (!wp->user && wp->username) {
            wp->user = websLookupUser(wp->username);
        }
        for (key = hashFirst(abilities); key; key = hashNext(abilities, key)) {
            ability = key->name.value.string;
            if ((cp = strchr(ability, '|')) != 0) {
                /*
                    Examine a set of alternative abilities. Need only one to match
                 */
                start = ability;
                do {
                    sncopy(abuf, sizeof(abuf), start, cp - start);
                    if (can(wp, abuf)) {
                        break;
                    }
                    start = &cp[1];
                } while ((cp = strchr(start, '|')) != 0);
                if (!cp) {
                    websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Insufficient capabilities.");
                    return 0;
                }
            } else if (!can(wp, ability)) {
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Insufficient capabilities.");
                return 0;
            }
        }
    }
    return 1;
}
#endif


#if KEEP
PUBLIC bool websCanString(Webs *wp, char *abilities)
{
    WebsUser *user;
    char     *ability, *tok;

    if (!wp->user) {
        if (!wp->username) {
            return 0;
        }
        if ((user = websLookupUser(wp->username)) == 0) {
            trace(2, "Cannot find user %s", wp->username);
            return 0;
        }
    }
    abilities = sclone(abilities);
    for (ability = stok(abilities, " \t,", &tok); ability; ability = stok(NULL, " \t,", &tok)) {
        if (hashLookup(wp->user->abilities, ability) == 0) {
            wfree(abilities);
            return 0;
        }
    }
    wfree(abilities);
    return 1;
}
#endif


/*
    If pos is < 0, then add to the end. Otherwise insert at specified position
 */
PUBLIC WebsRoute *websAddRoute(cchar *uri, cchar *handler, int pos)
{
    WebsRoute *route;
    WebsKey   *key;

    if (uri == 0 || *uri == '\0') {
        error("Route has bad URI");
        return 0;
    }
    if ((route = walloc(sizeof(WebsRoute))) == 0) {
        return 0;
    }
    memset(route, 0, sizeof(WebsRoute));
    route->prefix = sclone(uri);
    route->prefixLen = slen(uri);
    route->abilities = route->extensions = route->methods = route->redirects = -1;
    if (!handler) {
        handler = "file";
    }
    if ((key = hashLookup(handlers, handler)) == 0) {
        error("Cannot find route handler %s", handler);
        wfreeset(route->prefix);
        wfree(route);
        return 0;
    }
    route->handler = key->content.value.symbol;

#if ME_GOAHEAD_AUTH
    route->verify = websGetPasswordStoreVerify();
#endif
    growRoutes();
    if (pos < 0) {
        pos = routeCount;
    }
    if (pos < routeCount) {
        memmove(&routes[pos + 1], &routes[pos], sizeof(WebsRoute*) * routeCount - pos);
    }
    routes[pos] = route;
    routeCount++;
    return route;
}


PUBLIC int websSetRouteMatch(WebsRoute *route, cchar *dir, cchar *protocol, WebsHash methods, WebsHash extensions,
                             WebsHash abilities, WebsHash redirects)
{
    assert(route);

    if (dir) {
        route->dir = sclone(dir);
    }
    route->protocol = protocol ? sclone(protocol) : 0;
    route->abilities = abilities;
    route->extensions = extensions;
    route->methods = methods;
    route->redirects = redirects;

#if !ME_GOAHEAD_AUTH
    /*
        This build compiles out the authorization gate, so an ability requirement can only be
        recorded on the route and never consulted. Refuse it here - the single choke point through
        which every ability set reaches a route - rather than let the route table describe a policy
        the binary cannot enforce. The hashes are attached to the route above before failing so that
        websCloseRoute still owns and frees them.
     */
    if (abilities >= 0) {
        error("Route \"%s\" requires abilities, but this build has ME_GOAHEAD_AUTH=0 and cannot "
              "enforce them. Rebuild with ME_GOAHEAD_AUTH=1 or remove the abilities requirement.",
              route->prefix);
        return -1;
    }
#endif
    return 0;
}


static void growRoutes(void)
{
    if (routeCount >= routeMax) {
        routeMax += 16;
        if ((routes = wrealloc(routes, sizeof(WebsRoute*) * routeMax)) == 0) {
            error("Cannot grow routes");
        }
    }
}


static int lookupRoute(cchar *uri)
{
    WebsRoute *route;
    int       i;

    assert(uri && *uri);

    for (i = 0; i < routeCount; i++) {
        route = routes[i];
        if (smatch(route->prefix, uri)) {
            return i;
        }
    }
    return -1;
}


static void freeRoute(WebsRoute *route)
{
    assert(route);

    if (route->abilities >= 0) {
        hashFree(route->abilities);
    }
    if (route->extensions >= 0) {
        hashFree(route->extensions);
    }
    if (route->methods >= 0) {
        hashFree(route->methods);
    }
    if (route->redirects >= 0) {
        hashFree(route->redirects);
    }
    wfreeset(route->prefix);
    wfreeset(route->dir);
    wfreeset(route->protocol);
    wfreeset(route->authType);
    wfree(route);
}


PUBLIC int websRemoveRoute(cchar *uri)
{
    int i;

    assert(uri && *uri);

    if ((i = lookupRoute(uri)) < 0) {
        return -1;
    }
    freeRoute(routes[i]);
    for (; i < routeCount; i++) {
        routes[i] = routes[i + 1];
    }
    routeCount--;
    return 0;
}


#if ME_GOAHEAD_AUTH
/*
    Does this route gate access behind authentication or an ability check?
 */
static int routeRequiresAuth(WebsRoute *route)
{
    return route->authType || route->abilities >= 0;
}


/*
    Does this route narrow itself with a selector? A qualified route claims only a subset of the requests
    its prefix covers, so it shadows less than a bare prefix route does.
 */
static int routeIsQualified(WebsRoute *route)
{
    return route->extensions >= 0 || route->methods >= 0 || route->protocol != 0;
}


/*
    Can two routes serve the same files? Routes without an explicit dir= share the document root, so the
    common configuration overlaps by default. Compares the configured directories rather than their
    realpath, so two paths that reach one directory through a symlink are not detected - that yields a
    missed warning, never a false one.
 */
static int regionsOverlap(WebsRoute *r1, WebsRoute *r2)
{
    cchar *d1, *d2, *shorter, *longer;
    ssize len;

    d1 = r1->dir ? r1->dir : websGetDocuments();
    d2 = r2->dir ? r2->dir : websGetDocuments();
    if (d1 == 0 || d2 == 0) {
        return 1;
    }
    if (slen(d1) <= slen(d2)) {
        shorter = d1;
        longer = d2;
    } else {
        shorter = d2;
        longer = d1;
    }
    len = slen(shorter);
    if (sncmp(shorter, longer, len) != 0) {
        return 0;
    }
    return longer[len] == '\0' || longer[len] == '/';
}
#endif /* ME_GOAHEAD_AUTH */


/*
    Audit the loaded route table and warn about configurations that defeat the authentication gate.
    Returns the number of problems found.

    Two checks. First, no route requires authentication at all. Second, an unauthenticated route later in
    the table shadows an authenticated route earlier in it. The second is the structural cause of the
    route-prefix authorization bypass family: routes are matched in order and a request that misses its
    protected route falls through to the next match rather than being denied, so any spelling the file
    system folds to a protected name - case, Unicode normalization, Unicode case, Win32 trailing dot or
    space - misses the strict route and is served by the permissive one. Rejecting the shadowing
    configuration closes every such axis at once, including axes not yet enumerated, because the check
    never reasons about folding.

    Uses error() rather than logmsg() so warnings are visible at the default log level.
 */
PUBLIC int websCheckRoutes(void)
{
#if ME_GOAHEAD_AUTH
    WebsRoute *strict, *loose, *candidate;
    int       i, j, authRoutes, problems;

    if (routeCount == 0) {
        return 0;
    }
    problems = 0;
    authRoutes = 0;
    for (i = 0; i < routeCount; i++) {
        if (routeRequiresAuth(routes[i])) {
            authRoutes++;
        }
    }
    if (authRoutes == 0) {
        error("Security warning: no authenticated routes configured. All content is served to "
              "unauthenticated clients. See route.txt for how to protect content.");
        return 1;
    }
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!routeRequiresAuth(strict)) {
            continue;
        }
        /*
            Report one warning per protected route, naming the worst shadowing route rather than every
            one. A route carrying extensions=, methods= or protocol= only claims a subset of requests, so
            an unqualified route is the more serious exposure and is the one worth naming.
         */
        loose = 0;
        for (j = i + 1; j < routeCount; j++) {
            candidate = routes[j];
            if (routeRequiresAuth(candidate)) {
                continue;
            }
            if (!regionsOverlap(strict, candidate)) {
                continue;
            }
            if (matchRoutePrefix(strict->prefix, candidate->prefix, candidate->prefixLen) != 0) {
                continue;
            }
            if (loose == 0 || (routeIsQualified(loose) && !routeIsQualified(candidate))) {
                loose = candidate;
            }
        }
        if (loose) {
            /*
                Several routes may share one prefix - shipped route.txt has three "/" routes - so name
                the shadowing route's handler too, or the operator cannot tell which one is meant.
             */
            error("Security warning: route \"%s\" requires authentication, but later route \"%s\" "
                  "(handler=%s) does not and can serve the same directory. A case or Unicode folded "
                  "request will miss \"%s\" and be served without credentials. Reorder the routes, "
                  "narrow \"%s\", or give \"%s\" its own dir=.",
                  strict->prefix, loose->prefix, loose->handler ? loose->handler->name : "?",
                  strict->prefix, loose->prefix, strict->prefix);
            problems++;
        }
    }
    return problems;
#else
    return 0;
#endif
}


/*
    Legacy entry point. Retained so existing callers keep working; websCheckRoutes performs this check
    and the route shadowing check.
 */
PUBLIC void websCheckRouteAuth(void)
{
    (void) websCheckRoutes();
}


/*
    Return an allocated absolute form of path with '\' folded to '/' and any trailing separator removed,
    so two directory names can be compared as strings. A relative path is resolved against the current
    directory, which is where every relative configuration directory is resolved at runtime.
 */
static char *absolutePath(cchar *path)
{
    char cwd[ME_GOAHEAD_LIMIT_FILENAME], *result, *cp;
    int  absolute;

    absolute = (*path == '/' || *path == '\\');
#if ME_WIN_LIKE
    if (isalpha((uchar) path[0]) && path[1] == ':') {
        absolute = 1;
    }
#endif
    if (absolute) {
        result = sclone(path);
    } else if (getcwd(cwd, sizeof(cwd)) == NULL) {
        result = sclone(path);
    } else {
        result = sfmt("%s/%s", cwd, path);
    }
    for (cp = result; *cp; cp++) {
        if (*cp == '\\') {
            *cp = '/';
        }
    }
    /*
        "/" is its own trailing separator and must keep it, or every path compares as inside it.
     */
    for (cp = &result[slen(result) - 1]; cp > result && *cp == '/'; cp--) {
        *cp = '\0';
    }
    return result;
}


/*
    Test if child names dir or a location inside it. Compares the configured directories rather than
    their realpath, so a path that reaches one directory through a symlink is not detected - that yields
    a missed warning, never a false one.
 */
static int dirContains(cchar *dir, cchar *child)
{
    char  *parent, *sub;
    ssize len;
    int   rc;

    parent = absolutePath(dir);
    sub = absolutePath(child);
    len = slen(parent);
    rc = 0;
    if (len > 0 && (ME_WIN_LIKE ? sncaselesscmp(parent, sub, len) : sncmp(parent, sub, len)) == 0) {
        /*
            "/tmp" contains "/tmp" and "/tmp/x", but not "/tmpdir". A parent of "/" ends in the
            separator itself, so the child's next character is already part of its own name.
         */
        rc = sub[len] == '\0' || sub[len] == '/' || parent[len - 1] == '/';
    }
    wfree(parent);
    wfree(sub);
    return rc;
}


/*
    Audit the directories used to stage request data - CGI stdin and stdout, upload and PUT staging -
    against the served directories. A temporary file inside a served directory is fetchable over HTTP,
    so one client's in-flight request body or CGI response is readable by another. Returns the number of
    problems found; websOpen refuses to start the server when this is non-zero, because the exposure is
    unauthenticated and there is no request-time check that could contain it.
 */
PUBLIC int websCheckTempDirs(void)
{
    cchar *staging[3], *setting[3], *docs;
    int   i, j, count, problems;

    count = 0;
    staging[count] = ME_GOAHEAD_TEMP_DIR;
    setting[count++] = "ME_GOAHEAD_TEMP_DIR";
#if ME_GOAHEAD_UPLOAD
    staging[count] = ME_GOAHEAD_UPLOAD_DIR;
    setting[count++] = "ME_GOAHEAD_UPLOAD_DIR";
#endif
#if !ME_ROM
    staging[count] = ME_GOAHEAD_PUT_DIR;
    setting[count++] = "ME_GOAHEAD_PUT_DIR";
#endif

    problems = 0;
    for (i = 0; i < count; i++) {
        if (staging[i] == 0 || *staging[i] == '\0') {
            continue;
        }
        for (j = -1; j < routeCount; j++) {
            /*
                Routes without an explicit dir= serve the document root, so it is checked once here
                rather than once per route.
             */
            docs = (j < 0) ? websGetDocuments() : routes[j]->dir;
            if (docs == 0 || *docs == '\0') {
                continue;
            }
            if (dirContains(docs, staging[i])) {
                error("Security error: %s \"%s\" is inside the served directory \"%s\". Request data "
                      "staged there - CGI output, uploads, PUT bodies - is served to any client that "
                      "names the file. Set %s to a directory outside the documents directory.",
                      setting[i], staging[i], docs, setting[i]);
                problems++;
                break;
            }
        }
    }
    return problems;
}


PUBLIC int websOpenRoute(void)
{
    if ((handlers = hashCreate(-1)) < 0) {
        return -1;
    }
    websDefineHandler("continue", continueHandler, 0, 0, 0);
    websDefineHandler("redirect", redirectHandler, 0, 0, 0);
    return 0;
}


PUBLIC void websCloseRoute(void)
{
    WebsHandler *handler;
    WebsKey     *key;
    int         i;

    if (handlers >= 0) {
        for (key = hashFirst(handlers); key; key = hashNext(handlers, key)) {
            handler = key->content.value.symbol;
            if (handler->close) {
                (*handler->close)();
            }
            wfreeset(handler->name);
            wfree(handler);
        }
        hashFree(handlers);
        handlers = -1;
    }
    if (routes) {
        for (i = 0; i < routeCount; i++) {
            freeRoute(routes[i]);
        }
        wfree(routes);
        routes = 0;
    }
    routeCount = routeMax = 0;
}


PUBLIC int websDefineHandler(cchar *name, WebsHandlerProc match, WebsHandlerProc service, WebsHandlerClose close,
                             int flags)
{
    WebsHandler *handler;

    assert(name && *name);

    if ((handler = walloc(sizeof(WebsHandler))) == 0) {
        return -1;
    }
    memset(handler, 0, sizeof(WebsHandler));
    handler->name = sclone(name);
    handler->match = match;
    handler->service = service;
    handler->close = close;
    handler->flags = flags;

    hashEnter(handlers, name, valueSymbol(handler), 0);
    return 0;
}


static void addOption(WebsHash *hash, char *keys, char *value)
{
    char *sep, *key, *tok;

    if (*hash < 0) {
        *hash = hashCreate(-1);
    }
    sep = " \t,|";
    for (key = stok(keys, sep, &tok); key; key = stok(0, sep, &tok)) {
        if (strcmp(key, "none") == 0) {
            continue;
        }
        if (value == 0) {
            hashEnter(*hash, key, valueInteger(0), 0);
        } else {
            hashEnter(*hash, key, valueString(value, VALUE_ALLOCATE), 0);
        }
    }
}


#if !ME_GOAHEAD_AUTH
/*
    Refuse a configuration directive that describes an access control policy this build cannot
    enforce. Loading it and serving the resource unprotected leaves the operator with a configuration
    that reads as if authentication were in force while every protected route is public; declining to
    start is strictly the better failure. Returns -1 for the caller's result code.
 */
static int rejectAuthDirective(cchar *directive)
{
    error("Configuration directive \"%s\" requires ME_GOAHEAD_AUTH, but this build cannot enforce "
          "authentication. Refusing the configuration rather than serving it without the access "
          "control it describes. Rebuild with ME_GOAHEAD_AUTH=1 or remove the directive.", directive);
    return -1;
}
#endif


/*
    Load route and authentication configuration files
 */
PUBLIC int websLoad(cchar *path)
{
    WebsRoute *route;
    WebsHash  abilities, extensions, methods, redirects;
    char      *buf, *line, *kind, *next, *auth, *dir, *handler, *protocol, *uri, *option, *key, *value, *status;
    char      *redirectUri, *token;
    int       rc;

    assert(path && *path);

    rc = 0;
    if ((buf = websReadWholeFile(path)) == 0) {
        error("Cannot open config file %s", path);
        return -1;
    }
    for (line = stok(buf, "\r\n", &token); line; line = stok(NULL, "\r\n", &token)) {
        kind = stok(line, " \t", &next);
        if (kind == 0 || *kind == '\0' || *kind == '#') {
            continue;
        }
        if (smatch(kind, "route")) {
            auth = dir = handler = protocol = uri = 0;
            abilities = extensions = methods = redirects = -1;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "abilities")) {
                    addOption(&abilities, value, 0);
                } else if (smatch(key, "auth")) {
                    auth = value;
                } else if (smatch(key, "dir")) {
                    dir = value;
                } else if (smatch(key, "extensions")) {
                    addOption(&extensions, value, 0);
                } else if (smatch(key, "handler")) {
                    handler = value;
                } else if (smatch(key, "methods")) {
                    addOption(&methods, value, 0);
                } else if (smatch(key, "redirect")) {
                    if (strchr(value, '@')) {
                        status = stok(value, "@", &redirectUri);
                        if (smatch(status, "*")) {
                            status = "0";
                        }
                    } else {
                        status = "0";
                        redirectUri = value;
                    }
                    if (smatch(redirectUri, "https")) redirectUri = "https://";
                    if (smatch(redirectUri, "http")) redirectUri = "http://";
                    addOption(&redirects, status, redirectUri);
                } else if (smatch(key, "protocol")) {
                    protocol = value;
                } else if (smatch(key, "uri")) {
                    uri = value;
                } else {
                    error("Bad route keyword %s", key);
                    continue;
                }
            }
            if ((route = websAddRoute(uri, handler, -1)) == 0) {
                rc = -1;
                break;
            }
            if (websSetRouteMatch(route, dir, protocol, methods, extensions, abilities, redirects) < 0) {
                rc = -1;
                break;
            }
#if ME_GOAHEAD_AUTH
            if (auth && websSetRouteAuth(route, auth) < 0) {
                rc = -1;
                break;
            }
#else
            if (auth) {
                rc = rejectAuthDirective("auth");
                break;
            }
#endif

        } else if (smatch(kind, "user")) {
#if ME_GOAHEAD_AUTH
            char *name, *password, *roles;
            name = password = roles = 0;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "name")) {
                    name = value;
                } else if (smatch(key, "password")) {
                    password = value;
                } else if (smatch(key, "roles")) {
                    roles = value;
                } else {
                    error("Bad user keyword %s", key);
                    continue;
                }
            }
            if (websAddUser(name, password, roles) == 0) {
                rc = -1;
                break;
            }
#else
            rc = rejectAuthDirective(kind);
            break;
#endif

        } else if (smatch(kind, "role")) {
#if ME_GOAHEAD_AUTH
            char *name;
            name = 0;
            abilities = -1;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "name")) {
                    name = value;
                } else if (smatch(key, "abilities")) {
                    addOption(&abilities, value, 0);
                }
            }
            if (websAddRole(name, abilities) == 0) {
                rc = -1;
                break;
            }
#else
            rc = rejectAuthDirective(kind);
            break;
#endif

        } else {
            error("Unknown route keyword %s", kind);
            rc = -1;
            break;
        }
    }
    wfree(buf);
#if ME_GOAHEAD_AUTH
    websComputeAllUserAbilities();
#endif
    return rc;
}


/*
    Handler to just continue matching other routes
 */
static bool continueHandler(Webs *wp)
{
    return 0;
}


/*
    Handler to redirect to the default (code zero) URI
 */
static bool redirectHandler(Webs *wp)
{
    return websRedirectByStatus(wp, 0) == 0;
}


#if ME_GOAHEAD_LEGACY
PUBLIC int websUrlHandlerDefine(cchar *prefix, cchar *dir, int arg, WebsLegacyHandlerProc handler, int flags)
{
    WebsRoute  *route;
    static int legacyCount = 0;
    char       name[ME_GOAHEAD_LIMIT_STRING];

    assert(prefix && *prefix);
    assert(handler);

    fmt(name, sizeof(name), "%s-%d", prefix, legacyCount);
    if (websDefineHandler(name, 0, (WebsHandlerProc) handler, 0, WEBS_LEGACY_HANDLER) < 0) {
        return -1;
    }
    if ((route = websAddRoute(prefix, name, 0)) == 0) {
        return -1;
    }
    if (dir) {
        route->dir = sclone(dir);
    }
    return 0;
}


PUBLIC int websPublish(cchar *prefix, cchar *dir)
{
    WebsRoute *route;

    if ((route = websAddRoute(prefix, 0, 0)) == 0) {
        return -1;
    }
    route->dir = sclone(dir);
    return 0;
}
#endif


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
