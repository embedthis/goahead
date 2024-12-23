/*
    route.c -- Route Management

    This module implements the loading of a route configuration file
    and the routing of requests.

    The route configuration is loaded form a text file that uses the schema (see route.txt)
        uri: type: uri: method: ability [ability...]: redirect
        user: name: password: role [role...]
        role: name: ability [ability...]

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static WebsRoute **routes = 0;
static WebsHash handlers = -1;
static int routeCount = 0;
static int routeMax = 0;

#define WEBS_MAX_ROUTE 16               /* Maximum passes over route set */

/********************************** Forwards **********************************/

static bool continueHandler(Webs *wp);
static void freeRoute(WebsRoute *route);
static void growRoutes(void);
static int lookupRoute(cchar *uri);
static bool redirectHandler(Webs *wp);

/************************************ Code ************************************/
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

    safeMethod = smatch(wp->method, "POST") || smatch(wp->method, "GET") || smatch(wp->method, "HEAD");
    plen = slen(wp->path);

    /*
        Resume routine from last matched route. This permits the legacy service() callbacks to return false
        and continue routing.
     */
    if (wp->route && !(wp->flags & WEBS_REROUTE)) {
        for (i = 0; i < routeCount; i++) {
            if (wp->route == routes[i]) {
                i++;
                break;
            }
        }
        if (i >= routeCount) {
            i = 0;
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
        if (strncmp(wp->path, route->prefix, len) != 0) {
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
        if (route->authType && !websAuthenticate(wp)) {
            return;
        }
        if (route->abilities >= 0 && !websCan(wp, route->abilities)) {
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
    WebsRoute   *route;

    assert(wp);
    assert(wp->path);
    assert(wp->method);
    assert(wp->protocol);
    assert(wp->route);

    if ((route = wp->route) == 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no route for request");
        return 1;
    }
    if (!route->handler) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no handler for route");
        return 1;
    }
    if (!wp->filename || route->dir) {
        wfree(wp->filename);
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
    WebsKey     *key;
    char        *ability, *cp, *start, abuf[ME_GOAHEAD_LIMIT_STRING];

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
    WebsUser    *user;
    char        *ability, *tok;

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
    WebsRoute   *route;
    WebsKey     *key;

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
        wfree(route->prefix);
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
    WebsRoute   *route;
    int         i;

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
    wfree(route->prefix);
    wfree(route->dir);
    wfree(route->protocol);
    wfree(route->authType);
    wfree(route);
}


PUBLIC int websRemoveRoute(cchar *uri)
{
    int         i;

    assert(uri && *uri);

    if ((i = lookupRoute(uri)) < 0) {
        return -1;
    }
    freeRoute(routes[i]);
    for (; i < routeCount; i++) {
        routes[i] = routes[i+1];
    }
    routeCount--;
    return 0;
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
            wfree(handler->name);
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


PUBLIC int websDefineHandler(cchar *name, WebsHandlerProc match, WebsHandlerProc service, WebsHandlerClose close, int flags)
{
    WebsHandler     *handler;

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
    char    *sep, *key, *tok;

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


/*
    Load route and authentication configuration files
 */
PUBLIC int websLoad(cchar *path)
{
    WebsRoute   *route;
    WebsHash    abilities, extensions, methods, redirects;
    char        *buf, *line, *kind, *next, *auth, *dir, *handler, *protocol, *uri, *option, *key, *value, *status;
    char        *redirectUri, *token;
    int         rc;

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
            websSetRouteMatch(route, dir, protocol, methods, extensions, abilities, redirects);
#if ME_GOAHEAD_AUTH
            if (auth && websSetRouteAuth(route, auth) < 0) {
                rc = -1;
                break;
            }
        } else if (smatch(kind, "user")) {
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
        } else if (smatch(kind, "role")) {
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
    static int  legacyCount = 0;
    char        name[ME_GOAHEAD_LIMIT_STRING];

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
    WebsRoute   *route;

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
