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
static bool      strictMode = ME_GOAHEAD_STRICT;
static WebsRoute **retiredRoutes;                  /* Removed routes still held by in-flight requests */
static int       retiredCount;
static char      **configFiles;                    /* Route and credential files websLoad has read */
static int       configFileCount;
static WebsHash  routeMethods = -1;                 /* Methods named by a route's methods= beyond the standard set */

#define WEBS_MAX_ROUTE     16           /* Maximum passes over route set */

/* Verify at startup that a build matching routes with case does not serve a file system that ignores it */
#define CHECK_CASE_FOLDING (!ME_GOAHEAD_CASE_INSENSITIVE_FS && !ME_ROM && !ME_WIN_LIKE)

/********************************** Forwards **********************************/

static void addKnownMethods(WebsHash methods);
static bool continueHandler(Webs *wp);
static void freeRoute(WebsRoute *route);
static void growRoutes(void);
static int lookupRoute(cchar *uri);
static bool redirectHandler(Webs *wp);

/************************************ Code ************************************/
/* Return the action name and length from the route prefix, or zero for a route handling otherwise unnamed actions. */
static ssize routeActionName(WebsRoute *route, cchar **name)
{
    cchar *cp;

    *name = "";
    if (!route->handler || !smatch(route->handler->name, "action") || (cp = strchr(&route->prefix[1], '/')) == 0) {
        return 0;
    }
    *name = ++cp;
    return (ssize) strcspn(cp, "/");
}


/* Named action routes handle only their action; unnamed routes exclude actions claimed by named routes. */
PUBLIC bool websRouteServesAction(WebsRoute *route, cchar *name)
{
    cchar *own;
    ssize len;
    int   i;

    if (route == 0 || name == 0) {
        return 0;
    }
    if ((len = routeActionName(route, &own)) > 0) {
        return slen(name) == len && sncmp(own, name, len) == 0;
    }
    for (i = 0; i < routeCount; i++) {
        if ((len = routeActionName(routes[i], &own)) > 0 && slen(name) == len && sncmp(own, name, len) == 0) {
            return 0;
        }
    }
    return 1;
}

/*
    Fold ASCII case on case-insensitive filesystems to keep route authorization aligned with file lookup. Preserve
    the original path for open and CGI; validCanonicalPath rejects other filesystem aliases.
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
    Does a route's methods= admit the method? HEAD follows GET (RFC 9110 9.3.2). A route without methods= admits
    the safe methods GET, HEAD and POST.
 */
static bool routeAllowsMethod(WebsRoute *route, cchar *method)
{
    if (route->methods < 0) {
        return smatch(method, "GET") || smatch(method, "HEAD") || smatch(method, "POST");
    }
    return hashLookup(route->methods, method) || (smatch(method, "HEAD") && hashLookup(route->methods, "GET"));
}



static int routeRunsCode(WebsRoute *route)
{
    cchar *name;

    name = route->handler ? route->handler->name : "";
    return smatch(name, "jst") || smatch(name, "cgi") || smatch(name, "action");
}


/*
    Route the request. If wp->route is already set, test routes after that route
 */

PUBLIC void websRouteRequest(Webs *wp)
{
    WebsRoute   *route;
    WebsHandler *handler;
    ssize       plen, len;
    bool        safeMethod, gateOnly;
    int         i;

    assert(wp);
    if (!wp->path || !wp->method || !wp->protocol) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Incomplete request");
        return;
    }
#if ME_GOAHEAD_SECURE_ONLY
    if (!(wp->flags & WEBS_HTTPS)) {
        websRedirect(wp, "https://");
        return;
    }
#endif
    /*
        Entering with a route set means this is a re-route: a handler declined and websPump routed again.
        Every re-entry counts against the loop limit, however it was reached. Without this, a handler that
        declines forever spins the event loop and the single threaded server never services another
        connection.
     */
    if (wp->route && ++wp->routeCount >= WEBS_MAX_ROUTE) {
        wp->route = 0;
        trace(2, "Route loop for %s", wp->url);
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot find suitable route for request.");
        return;
    }
    safeMethod = smatch(wp->method, "POST") || smatch(wp->method, "GET") || smatch(wp->method, "HEAD");

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

        /*
            Recompute path length each iteration because match callbacks may rewrite it.
         */
        plen = slen(wp->path);
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
        gateOnly = 0;
        if (route->methods >= 0) {
            if (!routeAllowsMethod(route, wp->method)) {
                trace(5, "Route %s does not match method %s", route->prefix, wp->method);
                continue;
            }
        } else if (!safeMethod) {
            /* A route without methods= authenticates every method, even though it serves only safe methods. */
            if (!route->authType && route->abilities < 0) {
                continue;
            }
            gateOnly = 1;
        }
        if (route->extensions >= 0 && (wp->ext == 0 || !hashLookup(route->extensions, &wp->ext[1]))) {
            trace(5, "Route %s doesn match extension %s", route->prefix, wp->ext ? wp->ext : "");
            continue;
        }

        wp->route = route;
        /*
            Reject cross-site state-changing requests before route authentication and handling; browsers attach
            credentials automatically.
         */
        if (!(route->flags & WEBS_ROUTE_CROSS_SITE) && !smatch(wp->method, "GET") && !smatch(wp->method, "HEAD") &&
            !websSameOrigin(wp)) {
#if ME_GOAHEAD_AUTH
            if (route->authType) {
                websLogAuth(wp, 0, "refused", "request from another site");
            }
#endif
            websError(wp, HTTP_CODE_FORBIDDEN, "Request from another site refused");
            wp->route = 0;
            return;
        }
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
#if ME_GOAHEAD_DIGEST
        websAcceptDigestCount(wp);
#endif
#endif
        if (gateOnly) {
            wp->route = 0;
            continue;
        }
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
            // Rescan from route 0: the loop increment follows
            i = -1;
        }
    }
    if (wp->routeCount >= WEBS_MAX_ROUTE) {
        trace(2, "Route loop for %s", wp->url);
    }
    websError(wp, HTTP_CODE_NOT_FOUND, "Cannot find suitable route for request.");
    assert(wp->route == 0);
}


PUBLIC bool websRunRequest(Webs *wp)
{
    WebsRoute *route;

    assert(wp);
    /*
        Never dispatch a handler for a request whose response has already been written. The state
        machine is the primary guard; this is defence in depth for any future caller.
     */
    if (wp->finalized) {
        return 1;
    }
    if (!wp->path || !wp->method || !wp->protocol) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Incomplete request");
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
        wp->flags |= WEBS_VARS_ADDED;
        if (websSetQueryVars(wp) < 0 || websSetFormVars(wp) < 0) {
            websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Too many request parameters");
            return 1;
        }
    }
    wp->state = WEBS_RUNNING;
    trace(5, "Route %s calls handler %s", route->prefix, route->handler->name);

#if ME_GOAHEAD_LEGACY
    if (route->handler->legacy) {
        return (*route->handler->legacy)(wp, route->prefix, route->dir, route->flags) == 0;
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



static bool canAny(Webs *wp, cchar *abilities)
{
    char *copy, *ability, *tok;
    bool ok;

    ok = 0;
    copy = sclone(abilities);
    for (ability = stok(copy, "|", &tok); ability && !ok; ability = stok(NULL, "|", &tok)) {
        ok = can(wp, ability);
    }
    wfree(copy);
    return ok;
}


PUBLIC bool websCan(Webs *wp, WebsHash abilities)
{
    WebsKey *key;
    char    *ability;

    assert(wp);
    assert(abilities >= 0);

    if (!wp->user) {
        if (wp->authType) {
            if (!wp->username) {
                websLogAuth(wp, 0, "refused", "user not logged in");
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. User not logged in.");
                return 0;
            }
            if ((wp->user = websLookupUser(wp->username)) == 0) {
                websLogAuth(wp, 0, "refused", "unknown user");
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
            if (strchr(ability, '|') ? !canAny(wp, ability) : !can(wp, ability)) {
                websLogAuth(wp, 0, "refused", "insufficient abilities");
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Insufficient capabilities.");
                return 0;
            }
        }
    }
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
    /* Request paths are decoded and normalized before matching, so a prefix in any other form never matches. */
    if (!websCanonicalUriPath(uri)) {
        error("Route \"%s\" is not a canonical path and never matches. Write the prefix as the decoded request path: "
              "beginning with \"/\", without empty, \".\" or \"..\" segments, percent escapes or file system aliases.",
              uri);
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
    growRoutes();
    if (pos < 0) {
        pos = routeCount;
    }
    if (pos < routeCount) {
        memmove(&routes[pos + 1], &routes[pos], sizeof(WebsRoute*) * (routeCount - pos));
    }
    routes[pos] = route;
    routeCount++;
    return route;
}


/*
    Return an unmatchable selector value: lowercase method, uppercase extension, or leading extension dot. Otherwise NULL.
 */
static cchar *badSelector(WebsHash hash, bool upper)
{
    WebsKey *kp;
    cchar   *cp;

    if (hash < 0) {
        return 0;
    }
    for (kp = hashFirst(hash); kp; kp = hashNext(hash, kp)) {
        cp = kp->name.value.string;
        if (!upper && *cp == '.') {
            return cp;
        }
        for (; *cp; cp++) {
            if (upper ? islower((uchar) *cp) : isupper((uchar) *cp)) {
                return kp->name.value.string;
            }
        }
    }
    return 0;
}


PUBLIC int websSetRouteMatch(WebsRoute *route, cchar *dir, cchar *protocol, WebsHash methods, WebsHash extensions,
                             WebsHash abilities, WebsHash redirects)
{
    cchar *bad;

    assert(route);

    if (dir) {
        route->dir = sclone(dir);
    }
    route->protocol = protocol ? sclone(protocol) : 0;
    if (abilities >= 0 && hashFirst(abilities) == 0) {
        //  An empty ability set, such as abilities=none, is no requirement
        hashFree(abilities);
        abilities = -1;
    }
    route->abilities = abilities;
    route->extensions = extensions;
    route->methods = methods;
    route->redirects = redirects;
    addKnownMethods(methods);

    /* Reject selector spellings that cannot match normalized method, extension or protocol values. */
    if (protocol && !smatch(protocol, "http") && !smatch(protocol, "https")) {
        error("Route \"%s\" has protocol=%s, which never matches. Use protocol=http or protocol=https.",
              route->prefix, protocol);
        return -1;
    }
    if ((bad = badSelector(methods, 1)) != 0) {
        error("Route \"%s\" has method \"%s\", which never matches. Write methods in upper case.", route->prefix, bad);
        return -1;
    }
    if ((bad = badSelector(extensions, 0)) != 0) {
        error("Route \"%s\" has extension \"%s\", which never matches. Write extensions in lower case without the "
              "leading dot.", route->prefix, bad);
        return -1;
    }

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
    WebsRoute *route;
    int       i;

    assert(uri && *uri);

    if ((i = lookupRoute(uri)) < 0) {
        return -1;
    }
    route = routes[i];
    for (; i < routeCount - 1; i++) {
        routes[i] = routes[i + 1];
    }
    routeCount--;
    if (websRouteHolders(route) == 0) {
        freeRoute(route);
        return 0;
    }
    route->flags |= WEBS_ROUTE_REMOVED;
    retiredRoutes = wrealloc(retiredRoutes, sizeof(WebsRoute*) * (retiredCount + 1));
    retiredRoutes[retiredCount++] = route;
    return 0;
}


PUBLIC void websReapRoutes(void)
{
    int i;

    for (i = retiredCount - 1; i >= 0; i--) {
        if (websRouteHolders(retiredRoutes[i]) == 0) {
            freeRoute(retiredRoutes[i]);
            retiredRoutes[i] = retiredRoutes[--retiredCount];
        }
    }
    if (retiredCount == 0) {
        wfreeset(retiredRoutes);
    }
}


#if ME_GOAHEAD_AUTH
/*
    Does this route gate access behind authentication or an ability check?
 */
/*
    Identify authentication, ability, and protocol restrictions that broader routes could bypass.
 */
static int routeRequiresAuth(WebsRoute *route)
{
    return route->authType || route->abilities >= 0 || route->protocol != 0;
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
    Check whether every request under the prefix runs code; a named action route handles only that action.
 */
static int routeRunsAllCode(WebsRoute *route)
{
    cchar *action;

    if (!routeRunsCode(route)) {
        return 0;
    }
    return !smatch(route->handler->name, "action") || routeActionName(route, &action) == 0;
}


/* Continue, upload and redirect handlers do not serve protected content themselves. */
static int routeServesContent(WebsRoute *route)
{
    cchar *name;

    name = route->handler ? route->handler->name : "";
    return !smatch(name, "continue") && !smatch(name, "upload") && !smatch(name, "redirect");
}



static int routeIsGate(WebsRoute *route)
{
    return route->handler && smatch(route->handler->name, "continue");
}


/*
    Return the selector through which requests can reach a later nested route without authentication, or null if
    covered.
 */
static cchar *gateUncovered(WebsRoute *gate, WebsRoute *route)
{
    WebsKey *key;
    cchar   *safe[] = { "GET", "HEAD", "POST", 0 };
    int     i;

    if (gate->methods >= 0) {
        if (route->methods >= 0) {
            for (key = hashFirst(route->methods); key; key = hashNext(route->methods, key)) {
                if (!routeAllowsMethod(gate, key->name.value.string)) {
                    return key->name.value.string;
                }
            }
        } else {
            for (i = 0; safe[i]; i++) {
                if (!routeAllowsMethod(gate, safe[i])) {
                    return safe[i];
                }
            }
        }
    }
    if (gate->protocol && (!route->protocol || !smatch(gate->protocol, route->protocol))) {
        return route->protocol ? route->protocol : "another protocol";
    }
    if (gate->extensions >= 0) {
        if (route->extensions < 0) {
            return "another extension";
        }
        for (key = hashFirst(route->extensions); key; key = hashNext(route->extensions, key)) {
            if (!hashLookup(gate->extensions, key->name.value.string)) {
                return key->name.value.string;
            }
        }
    }
    return 0;
}



static int prefixesNest(WebsRoute *r1, WebsRoute *r2)
{
    WebsRoute *shorter, *longer;

    shorter = r1->prefixLen <= r2->prefixLen ? r1 : r2;
    longer = shorter == r1 ? r2 : r1;
    return matchRoutePrefix(longer->prefix, shorter->prefix, shorter->prefixLen) == 0;
}


/*
    Can one request satisfy both routes' selectors? A route without methods= admits the safe methods.
 */
static int selectorsOverlap(WebsRoute *r1, WebsRoute *r2)
{
    WebsKey *key;
    cchar   *safe[] = { "GET", "HEAD", "POST", 0 };
    int     i, common;

    if (r1->protocol && r2->protocol && !smatch(r1->protocol, r2->protocol)) {
        return 0;
    }
    if (r1->extensions >= 0 && r2->extensions >= 0) {
        for (common = 0, key = hashFirst(r2->extensions); key && !common; key = hashNext(r2->extensions, key)) {
            common = hashLookup(r1->extensions, key->name.value.string) != 0;
        }
        if (!common) {
            return 0;
        }
    }
    if (r2->methods >= 0) {
        for (key = hashFirst(r2->methods); key; key = hashNext(r2->methods, key)) {
            if (routeAllowsMethod(r1, key->name.value.string)) {
                return 1;
            }
        }
        return 0;
    }
    for (i = 0; safe[i]; i++) {
        if (routeAllowsMethod(r1, safe[i])) {
            return 1;
        }
    }
    return 0;
}


/*
    Does holding every ability the first set requires satisfy the second? A "|" entry is met by any one alternative.
 */
static int abilitiesCover(WebsHash have, WebsHash need)
{
    WebsKey *key;
    char    *copy, *ability, *tok;
    int     met;

    if (need < 0) {
        return 1;
    }
    if (have < 0) {
        return 0;
    }
    for (key = hashFirst(need); key; key = hashNext(need, key)) {
        if (hashLookup(have, key->name.value.string)) {
            continue;
        }
        met = 0;
        if (strchr(key->name.value.string, '|')) {
            copy = sclone(key->name.value.string);
            for (ability = stok(copy, "|", &tok); ability && !met; ability = stok(NULL, "|", &tok)) {
                met = hashLookup(have, ability) != 0;
            }
            wfree(copy);
        }
        if (!met) {
            return 0;
        }
    }
    return 1;
}


/*
    Does a request the earlier route authorizes meet the later route's requirement?
 */
static int routeMeets(WebsRoute *earlier, WebsRoute *later)
{
    if (later->authType && !smatch(earlier->authType, later->authType)) {
        return 0;
    }
    return abilitiesCover(earlier->abilities, later->abilities);
}


static int dirContains(cchar *dir, cchar *child);

/*
    Compare configured document roots for overlap, by the same rule as the temporary directory audit. Symlink
    aliases are not detected. Continue routes protect subsequent routes and overlap all roots.
 */
static int regionsOverlap(WebsRoute *r1, WebsRoute *r2)
{
    cchar *d1, *d2;

    if (routeIsGate(r1) || routeIsGate(r2)) {
        return 1;
    }
    d1 = r1->dir ? r1->dir : websGetDocuments();
    d2 = r2->dir ? r2->dir : websGetDocuments();
    if (d1 == 0 || d2 == 0) {
        return 1;
    }
    return dirContains(d1, d2) || dirContains(d2, d1);
}


/*
    Report route ordering, selector and action overlaps that bypass authentication. Return the problem count; a
    table without authentication warns without counting. Use error() so warnings appear at the default log level.
 */
static int checkRouteAuth(void)
{
    WebsRoute *strict, *loose, *candidate;
    cchar     *uncovered;
    int       i, j, authRoutes, problems;

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
        return 0;
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
            /*
                Equal-prefix routes receive requests declined by selectors. Protocol-only selection cannot be
                bypassed by path spelling.
             */
            if (candidate->prefixLen == strict->prefixLen && strict->extensions < 0 && strict->methods < 0) {
                continue;
            }
            if (loose == 0 || (routeIsQualified(loose) && !routeIsQualified(candidate))) {
                loose = candidate;
            }
        }
        if (loose && routeIsGate(strict)) {
            /*
                A gate's dir= serves nothing, so it is not a remedy: the directory must be served by an
                authenticated route of its own.
             */
            error("Security warning: gate route \"%s\" requires authentication, but later route \"%s\" "
                  "(handler=%s) does not and can serve the same requests. A case or Unicode folded request will "
                  "miss \"%s\" and be served without credentials. Serve the directory from \"%s\" itself, with "
                  "the file handler and its own dir=, or reorder the routes.",
                  strict->prefix, loose->prefix, loose->handler ? loose->handler->name : "?",
                  strict->prefix, strict->prefix);
            problems++;

        } else if (loose) {
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
    /*
        Check public routes missed by an authentication gate's selectors. Equal prefixes need this
        check for protocol-only gates; methods and extensions are checked above.
     */
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!strict->authType && strict->abilities < 0) {
            continue;
        }
        for (j = i + 1; j < routeCount; j++) {
            candidate = routes[j];
            if (routeRequiresAuth(candidate) || !routeServesContent(candidate) ||
                candidate->prefixLen < strict->prefixLen ||
                (candidate->prefixLen == strict->prefixLen && (strict->methods >= 0 || strict->extensions >= 0)) ||
                matchRoutePrefix(candidate->prefix, strict->prefix, strict->prefixLen) != 0 ||
                !regionsOverlap(strict, candidate)) {
                continue;
            }
            if ((uncovered = gateUncovered(strict, candidate)) != 0) {
                error("Security warning: route \"%s\" requires authentication only for some requests, but later "
                      "route \"%s\" (handler=%s) %s also accepts %s, which reaches it without credentials. "
                      "%s", strict->prefix, candidate->prefix, candidate->handler ? candidate->handler->name : "?",
                      candidate->prefixLen == strict->prefixLen ? "on the same prefix" : "under it",
                      uncovered, strict->protocol ? "Drop protocol= from the authenticated route and redirect http "
                      "to https with a protocol=http redirect=https route instead." :
                      "Drop the selector from the authenticated route or give the later route the same one.");
                problems++;
            }
        }
    }
    /*
        Report only the first earlier conflicting route; code routes are checked separately below.
     */
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!routeRequiresAuth(strict)) {
            continue;
        }
        for (j = 0; j < i; j++) {
            candidate = routes[j];
            if (routeRequiresAuth(candidate) || !routeServesContent(candidate) || routeRunsAllCode(candidate)) {
                continue;
            }
            if (!regionsOverlap(strict, candidate) ||
                matchRoutePrefix(strict->prefix, candidate->prefix, candidate->prefixLen) != 0) {
                continue;
            }
            error("Security warning: route \"%s\" (handler=%s) comes before authenticated route \"%s\" and "
                  "serves its requests without credentials. Put \"%s\" first.",
                  candidate->prefix, candidate->handler ? candidate->handler->name : "?", strict->prefix,
                  strict->prefix);
            problems++;
            break;
        }
    }
    /*
        Report earlier serving routes with weaker authentication or abilities. Gates pass requests on;
        abilities-only routes without auth admit nobody.
     */
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!strict->authType && strict->abilities < 0) {
            continue;
        }
        for (j = 0; j < i; j++) {
            candidate = routes[j];
            if (!candidate->authType || routeIsGate(candidate) || !routeServesContent(candidate) ||
                matchRoutePrefix(strict->prefix, candidate->prefix, candidate->prefixLen) != 0 ||
                !regionsOverlap(strict, candidate) || !selectorsOverlap(candidate, strict) ||
                routeMeets(candidate, strict)) {
                continue;
            }
            error("Security warning: route \"%s\" (auth=%s) comes before route \"%s\" (auth=%s) and serves its "
                  "requests under its own weaker requirement. Put \"%s\" first.", candidate->prefix,
                  candidate->authType, strict->prefix, strict->authType ? strict->authType : "none", strict->prefix);
            problems++;
            break;
        }
    }
    /* Unnamed action routes share otherwise unclaimed actions. Protect individual actions with named routes. */
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!routeRequiresAuth(strict) || !routeRunsAllCode(strict) || !smatch(strict->handler->name, "action")) {
            continue;
        }
        for (j = 0; j < routeCount; j++) {
            candidate = routes[j];
            if (j != i && !routeRequiresAuth(candidate) && routeRunsAllCode(candidate) &&
                smatch(candidate->handler->name, "action")) {
                error("Security warning: authenticated action route \"%s\" and unauthenticated action route \"%s\" "
                      "both run every action, so \"%s\" protects none of them. Name each protected action in its "
                      "own route, such as \"/action/upload\".", strict->prefix, candidate->prefix, strict->prefix);
                problems++;
                break;
            }
        }
    }

    for (j = 0; j < routeCount; j++) {
        candidate = routes[j];
        if (routeRequiresAuth(candidate) || !routeRunsAllCode(candidate)) {
            continue;
        }
        for (i = j + 1; i < routeCount; i++) {
            strict = routes[i];
            if (routeRequiresAuth(strict) && prefixesNest(strict, candidate) && regionsOverlap(strict, candidate)) {
                error("Security warning: route \"%s\" (handler=%s) comes before authenticated route \"%s\", so it "
                      "runs its %s without credentials. Put authenticated routes before the action, template and "
                      "CGI routes.", candidate->prefix, candidate->handler->name, strict->prefix,
                      smatch(candidate->handler->name, "action") ? "actions" :
                      smatch(candidate->handler->name, "jst") ? "templates" : "CGI programs");
                problems++;
                break;
            }
        }
    }
    return problems;
}

#else

/*
    With no authentication, identify the first code-executing or writable route in the warning.
 */
static void warnUnauthenticated(void)
{
    WebsRoute *route;
    int       i;

    for (i = 0; i < routeCount; i++) {
        route = routes[i];
        if (routeRunsCode(route) || (route->flags & WEBS_ROUTE_UPLOAD) || routeAllowsMethod(route, "PUT") ||
            routeAllowsMethod(route, "DELETE")) {
            error("Security warning: this build has ME_GOAHEAD_AUTH=0 and cannot authenticate, so every route is "
                  "served to unauthenticated clients, including route \"%s\" (handler=%s), which %s. Make every "
                  "route safe for anonymous use, or build with ME_GOAHEAD_AUTH=1 and protect it.", route->prefix,
                  route->handler ? route->handler->name : "?",
                  routeRunsCode(route) ? "runs code" : "accepts uploads or writes");
            return;
        }
    }
    error("Security warning: this build has ME_GOAHEAD_AUTH=0 and cannot authenticate, so every route is served to "
          "unauthenticated clients.");
}
#endif /* ME_GOAHEAD_AUTH */


#if CHECK_CASE_FOLDING

static bool flipCase(char *name)
{
    bool flipped;

    for (flipped = 0; *name; name++) {
        if (isupper((uchar) *name)) {
            *name = (char) tolower((uchar) *name);
            flipped = 1;
        } else if (islower((uchar) *name)) {
            *name = (char) toupper((uchar) *name);
            flipped = 1;
        }
    }
    return flipped;
}


/*
    Probe filesystem case rules using an entry or the directory's own name. Do not write to the filesystem.
 */
static bool isCaseInsensitiveDir(cchar *dir)
{
    struct stat   s1, s2;
    struct dirent *dp;
    DIR           *dirp;
    char          *path, *name, *original, *variant;
    bool          result;

    path = websAbsolutePath(dir);
    original = variant = 0;
    if ((dirp = opendir(path)) != 0) {
        while (!variant && (dp = readdir(dirp)) != 0) {
            name = sclone(dp->d_name);
            if (flipCase(name)) {
                original = sfmt("%s/%s", path, dp->d_name);
                variant = sfmt("%s/%s", path, name);
            }
            wfree(name);
        }
        closedir(dirp);
    }
    if (!variant && (name = strrchr(path, '/')) != 0 && name[1]) {
        original = sclone(path);
        variant = sclone(path);
        if (!flipCase(&variant[name - path + 1])) {
            wfreeset(variant);
        }
    }
    result = variant && stat(original, &s1) == 0 && stat(variant, &s2) == 0 && s1.st_dev == s2.st_dev &&
             s1.st_ino == s2.st_ino;
    wfree(original);
    wfree(variant);
    wfree(path);
    return result;
}


/*
    Warn when case-sensitive routes serve case-insensitive filesystems, permitting alternate spellings to bypass routes.
 */
static int checkCaseFolding(void)
{
    cchar *docs, *dir;
    int   i, j, problems;

    problems = 0;
    docs = websGetDocuments();
    for (i = -1; i < routeCount; i++) {
        dir = (i < 0) ? docs : routes[i]->dir;
        if (dir == 0 || *dir == '\0' || (i >= 0 && smatch(dir, docs))) {
            continue;
        }
        for (j = 0; j < i && !smatch(routes[j]->dir, dir); j++) { }
        if (j < i || !isCaseInsensitiveDir(dir)) {
            continue;
        }
        error("Security warning: the served directory \"%s\" is on a file system that ignores letter case, but "
              "this build matches routes with case, so a request spelled in another case misses its route and is "
              "served by a broader one. Build with ME_GOAHEAD_CASE_INSENSITIVE_FS=1.", dir);
        problems++;
    }
    return problems;
}
#endif /* CHECK_CASE_FOLDING */


/*
    Return the number of route-policy bypasses.
 */
PUBLIC int websCheckRoutes(void)
{
    int problems;

    if (routeCount == 0) {
        return 0;
    }
    problems = 0;
#if CHECK_CASE_FOLDING
    problems += checkCaseFolding();
#endif
#if ME_GOAHEAD_AUTH
    problems += checkRouteAuth();
#else
    warnUnauthenticated();
#endif
    return problems;
}


/*
    Legacy entry point. Retained so existing callers keep working; websCheckRoutes performs this check
    and the route shadowing check.
 */
PUBLIC void websCheckRouteAuth(void)
{
    (void) websCheckRoutes();
}


PUBLIC void websSetStrict(bool on)
{
    strictMode = on;
}


PUBLIC bool websGetStrict(void)
{
    return strictMode;
}


PUBLIC char *websAbsolutePath(cchar *path)
{
    char cwd[ME_GOAHEAD_LIMIT_FILENAME], *result, *cp, *tail, *normalized;
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
        Normalize after the drive or UNC prefix; clamp .. at the root.
     */
    cp = result;
#if ME_WIN_LIKE
    if (isalpha((uchar) cp[0]) && cp[1] == ':') {
        cp += 2;
    } else if (cp[0] == '/' && cp[1] == '/') {
        cp++;
    }
#endif
    if (*cp == '/') {
        tail = sclone(cp);
        *cp = '\0';
        normalized = websNormalizeUriPath(tail);
        cp = sfmt("%s%s%s", result, *normalized == '/' ? "" : "/", normalized);
        wfree(result);
        wfree(normalized);
        wfree(tail);
        result = cp;
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
    Compare configured directories using filesystem case rules. Symlink aliases are not detected.
 */
static int dirContains(cchar *dir, cchar *child)
{
    char  *parent, *sub;
    ssize len;
    int   rc;

    parent = websAbsolutePath(dir);
    sub = websAbsolutePath(child);
    len = slen(parent);
    rc = 0;
    if (len > 0 && matchRoutePrefix(sub, parent, len) == 0) {
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
    Report loaded configuration files inside served directories, where clients could fetch them.
 */
PUBLIC int websCheckConfigFiles(void)
{
    cchar *docs;
    int   i, j, problems;

    problems = 0;
    for (i = 0; i < configFileCount; i++) {
        for (j = -1; j < routeCount; j++) {
            docs = (j < 0) ? websGetDocuments() : routes[j]->dir;
            if (docs == 0 || *docs == '\0') {
                continue;
            }
            if (dirContains(docs, configFiles[i])) {
                error("Security error: the configuration file \"%s\" is inside the served directory \"%s\" and "
                      "would be served to any client that names it. Move it outside the documents directory.",
                      configFiles[i], docs);
                problems++;
                break;
            }
        }
    }
    return problems;
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
    struct { cchar *dir, *setting; } staging[3];
    struct stat sbuf;
    cchar       *docs;
    int         i, j, count, problems;

    problems = 0;
    count = 0;
    staging[count].dir = websGetTempDir();
    staging[count++].setting = "ME_GOAHEAD_TEMP_DIR";
    if (stat(staging[0].dir, &sbuf) < 0 || !S_ISDIR(sbuf.st_mode)) {
        error("Security error: the temporary directory \"%s\" does not exist or is not a directory. Set "
              "ME_GOAHEAD_TEMP_DIR to an existing directory outside the documents directory.", staging[0].dir);
        problems++;
    }
#if ME_GOAHEAD_UPLOAD
    staging[count].dir = ME_GOAHEAD_UPLOAD_DIR;
    staging[count++].setting = "ME_GOAHEAD_UPLOAD_DIR";
#endif
#if !ME_ROM
    staging[count].dir = ME_GOAHEAD_PUT_DIR;
    staging[count++].setting = "ME_GOAHEAD_PUT_DIR";
#endif

    for (i = 0; i < count; i++) {
        if (staging[i].dir == 0 || *staging[i].dir == '\0') {
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
            if (dirContains(docs, staging[i].dir)) {
                error("Security error: %s \"%s\" is inside the served directory \"%s\". Request data "
                      "staged there - CGI output, uploads, PUT bodies - is served to any client that "
                      "names the file. Set %s to a directory outside the documents directory.",
                      staging[i].setting, staging[i].dir, docs, staging[i].setting);
                problems++;
                break;
            }
        }
    }
    return problems;
}


/*
    Recognize methods declared by routes instead of rejecting them as unimplemented.
 */
static void addKnownMethods(WebsHash methods)
{
    WebsKey *key;

    if (methods < 0) {
        return;
    }
    if (routeMethods < 0) {
        routeMethods = hashCreate(-1);
    }
    for (key = hashFirst(methods); key; key = hashNext(methods, key)) {
        hashEnter(routeMethods, key->name.value.string, valueInteger(0), 0);
    }
}


/*
    Methods are case-sensitive. A spelling the server does not implement, such as "head", is unknown.
 */
PUBLIC bool websKnownMethod(cchar *method)
{
    static cchar *standard[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS", "TRACE", 0 };
    int          i;

    if (method == 0) {
        return 0;
    }
    for (i = 0; standard[i]; i++) {
        if (smatch(method, standard[i])) {
            return 1;
        }
    }
    return routeMethods >= 0 && hashLookup(routeMethods, method) != 0;
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
    for (i = 0; i < retiredCount; i++) {
        freeRoute(retiredRoutes[i]);
    }
    retiredCount = 0;
    wfreeset(retiredRoutes);
    for (i = 0; i < configFileCount; i++) {
        wfreeset(configFiles[i]);
    }
    configFileCount = 0;
    wfreeset(configFiles);
    routeCount = routeMax = 0;
    if (routeMethods >= 0) {
        hashFree(routeMethods);
        routeMethods = -1;
    }
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
#if ME_GOAHEAD_LEGACY
    if (flags & WEBS_LEGACY_HANDLER) {
        /* The caller cast a legacy service to WebsHandlerProc. Restore its own type. */
        handler->legacy = (WebsLegacyHandlerProc) (void (*)(void)) service;
    }
#endif

    hashEnter(handlers, name, valueSymbol(handler), 0);
    return 0;
}


/*
    Split required keys on spaces, tabs, and commas; accepted alternatives also split on |.
 */
static void addOption(WebsHash *hash, char *keys, cchar *sep, char *value)
{
    char *key, *tok;

    if (*hash < 0) {
        *hash = hashCreate(-1);
    }
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
    Split key=value; report missing values and return null on error.
 */
static char *splitOption(cchar *kind, char *option, char **value)
{
    char *key;

    key = stok(option, "=", value);
    if (*value == 0 || **value == '\0') {
        error("Bad %s keyword %s: it needs a value", kind, key);
        return 0;
    }
    return key;
}


/*
    Load route and authentication configuration files
 */
PUBLIC int websLoad(cchar *path)
{
    WebsRoute *route;
    WebsHash  abilities, extensions, methods, redirects;
    char      *buf, *line, *kind, *next, *auth, *dir, *handler, *protocol, *uri, *option, *key, *value, *status;
    char      *redirectUri, *token;
    int       rc, badKeyword, flags;

    assert(path && *path);

    rc = 0;
    badKeyword = 0;
    if ((buf = websReadWholeFile(path)) == 0) {
        error("Cannot open config file %s", path);
        return -1;
    }
    configFiles = wrealloc(configFiles, sizeof(char*) * (configFileCount + 1));
    configFiles[configFileCount++] = sclone(path);
    for (line = stok(buf, "\r\n", &token); line; line = stok(NULL, "\r\n", &token)) {
        kind = stok(line, " \t", &next);
        if (kind == 0 || *kind == '\0' || *kind == '#') {
            continue;
        }
        if (smatch(kind, "route")) {
            auth = dir = handler = protocol = uri = 0;
            abilities = extensions = methods = redirects = -1;
            flags = 0;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                if ((key = splitOption(kind, option, &value)) == 0) {
                    badKeyword = 1;
                    continue;
                }
                if (smatch(key, "abilities")) {
                    addOption(&abilities, value, " \t,", 0);
                } else if (smatch(key, "auth")) {
                    auth = value;
                } else if (smatch(key, "dir")) {
                    /*
                        Reject empty directories, which would expose the filesystem root.
                     */
                    if (value == 0 || *value == '\0') {
                        error("Route dir= must name a directory");
                        badKeyword = 1;
                    }
                    dir = value;
                } else if (smatch(key, "extensions")) {
                    addOption(&extensions, value, " \t,|", 0);
                } else if (smatch(key, "handler")) {
                    handler = value;
                } else if (smatch(key, "methods")) {
                    addOption(&methods, value, " \t,|", 0);
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
                    addOption(&redirects, status, " \t,|", redirectUri);
                } else if (smatch(key, "cross-site")) {
                    if (smatch(value, "allow")) {
                        flags |= WEBS_ROUTE_CROSS_SITE;
                    } else {
                        error("Route cross-site= must be allow");
                        badKeyword = 1;
                    }
                } else if (smatch(key, "protocol")) {
                    protocol = value;
                } else if (smatch(key, "upload")) {
                    if (smatch(value, "true")) {
                        flags |= WEBS_ROUTE_UPLOAD;
                    } else if (!smatch(value, "false")) {
                        error("Route upload= must be true or false");
                        badKeyword = 1;
                    }
                } else if (smatch(key, "uri")) {
                    uri = value;
                } else {
                    error("Bad route keyword %s", key);
                    badKeyword = 1;
                }
            }
            /*
                Reject unknown qualifiers so typos cannot silently remove route protection.
             */
            if (badKeyword) {
                rc = -1;
                break;
            }
            if ((route = websAddRoute(uri, handler, -1)) == 0) {
                rc = -1;
                break;
            }
            route->flags |= flags;
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
                if ((key = splitOption(kind, option, &value)) == 0) {
                    badKeyword = 1;
                    continue;
                }
                if (smatch(key, "name")) {
                    name = value;
                } else if (smatch(key, "password")) {
                    password = value;
                } else if (smatch(key, "roles")) {
                    roles = value;
                } else {
                    error("Bad user keyword %s", key);
                    badKeyword = 1;
                }
            }
            if (badKeyword) {
                rc = -1;
                break;
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
                if ((key = splitOption(kind, option, &value)) == 0) {
                    badKeyword = 1;
                    continue;
                }
                if (smatch(key, "name")) {
                    name = value;
                } else if (smatch(key, "abilities")) {
                    addOption(&abilities, value, " \t,|", 0);
                } else {
                    error("Bad role keyword %s", key);
                    badKeyword = 1;
                }
            }
            if (badKeyword) {
                rc = -1;
                break;
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
    WebsRoute   *route;
    WebsHandler *def;
    static int  legacyCount = 0;
    char        name[ME_GOAHEAD_LIMIT_STRING];

    assert(prefix && *prefix);
    assert(handler);

    fmt(name, sizeof(name), "%s-%d", prefix, legacyCount);
    if (websDefineHandler(name, 0, 0, 0, WEBS_LEGACY_HANDLER) < 0) {
        return -1;
    }
    def = hashLookupSymbol(handlers, name);
    def->legacy = handler;
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
