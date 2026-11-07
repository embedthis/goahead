/*
    auth-cleartext-warning.tst.c - A plain HTTP listener warns for each authenticated route it can carry

    Credentials and session cookies cross the network unencrypted when an authenticated route is reachable over
    plain HTTP, whatever TLS the build has, so the check runs at the first plain HTTP listener rather than at
    route load. A route restricted to protocol=https, a route behind an earlier http-to-https redirect, and a
    server behind a trusted TLS-terminating proxy are not reported.
 */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/route.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

#define ENDPOINT "http://127.0.0.1:80"

static int  warnings;
static char messages[4096];

static void captureLog(int level, cchar *msg)
{
    if (msg && strstr(msg, "Security warning:")) {
        warnings++;
        if (slen(messages) + slen(msg) + 2 < (ssize) sizeof(messages)) {
            strcat(messages, msg);
            strcat(messages, "\n");
        }
    }
}


static void reset(void)
{
    websCloseRoute();
    ttrue(websOpenRoute() == 0, "open routes");
    warnings = 0;
    messages[0] = '\0';
}


static WebsRoute *addAuthRoute(cchar *uri, cchar *type, cchar *protocol)
{
    WebsRoute *route;

    route = websAddRoute(uri, "continue", -1);
    ttrue(route != 0, "add route %s", uri);
    teq(websSetRouteMatch(route, 0, protocol, -1, -1, -1, -1), 0, "set the match for %s", uri);
    teq(websSetRouteAuth(route, type), 0, "set %s authentication on %s", type, uri);
    return route;
}


static void checkNamed(cchar *uri, cchar *type)
{
    char expected[256];

    fmt(expected, sizeof(expected), "route \"%s\" uses %s authentication and " ENDPOINT " is plain HTTP", uri, type);
    ttrue(strstr(messages, expected) != 0, "the warning names the %s route, its type and the listener: %s", type,
          messages);
}


int main(int argc, char **argv)
{
    WebsHash  redirects;
    WebsRoute *route;
    int       authRoutes;

    logSetHandler(captureLog);

    /*
        Every authenticated route a plain listener can carry is named once; public routes are not
     */
    reset();
    authRoutes = 0;
    addAuthRoute("/basic/", "basic", 0);
    authRoutes++;
#if ME_GOAHEAD_DIGEST
    addAuthRoute("/digest/", "digest", 0);
    authRoutes++;
#endif
    addAuthRoute("/form/", "form", 0);
    authRoutes++;
    ttrue(websAddRoute("/public/", "continue", -1) != 0, "add a public route");
    websCheckCleartextAuth(ENDPOINT, 0);
    teq(warnings, authRoutes, "each authenticated route warns once");
    checkNamed("/basic/", "basic");
#if ME_GOAHEAD_DIGEST
    checkNamed("/digest/", "digest");
#endif
    checkNamed("/form/", "form");
    ttrue(strstr(messages, "unencrypted") != 0, "and says the credentials are unencrypted");
    ttrue(strstr(messages, "/public/") == 0, "a public route is not reported");

    websCheckCleartextAuth("http://127.0.0.1:8080", 0);
    teq(warnings, authRoutes, "a second plain listener does not repeat the warnings");

    /*
        A route restricted to https never carries a plain HTTP request
     */
    reset();
    addAuthRoute("/secure/", "basic", "https");
    websCheckCleartextAuth(ENDPOINT, 0);
    teq(warnings, 0, "a protocol=https route is not reported: %s", messages);

    /*
        An earlier route that redirects plain HTTP to https covers the routes under its prefix
     */
    reset();
    route = websAddRoute("/", "redirect", -1);
    ttrue(route != 0, "add the redirect gate");
    redirects = hashCreate(-1);
    hashEnter(redirects, "0", valueString("https://", VALUE_ALLOCATE), 0);
    teq(websSetRouteMatch(route, 0, "http", -1, -1, -1, redirects), 0, "redirect every http request to https");
    addAuthRoute("/gated/", "basic", 0);
    websCheckCleartextAuth(ENDPOINT, 0);
    teq(warnings, 0, "a route behind an http-to-https redirect is not reported: %s", messages);

    /*
        A redirect gate that sends requests elsewhere than https, or that comes after the route, covers nothing
     */
    reset();
    route = websAddRoute("/", "redirect", -1);
    redirects = hashCreate(-1);
    hashEnter(redirects, "0", valueString("/index.html", VALUE_ALLOCATE), 0);
    teq(websSetRouteMatch(route, 0, "http", -1, -1, -1, redirects), 0, "redirect to a page, not to https");
    addAuthRoute("/gated/", "basic", 0);
    websCheckCleartextAuth(ENDPOINT, 0);
    teq(warnings, 1, "a redirect that does not go to https does not cover the route");

    reset();
    addAuthRoute("/gated/", "basic", 0);
    route = websAddRoute("/", "redirect", -1);
    redirects = hashCreate(-1);
    hashEnter(redirects, "0", valueString("https://", VALUE_ALLOCATE), 0);
    teq(websSetRouteMatch(route, 0, "http", -1, -1, -1, redirects), 0, "redirect placed after the route");
    websCheckCleartextAuth(ENDPOINT, 0);
    teq(warnings, 1, "a redirect after the authenticated route does not cover it");

    /*
        A trusted proxy terminates TLS for the server, so its plain listener is not reported
     */
    reset();
    addAuthRoute("/basic/", "basic", 0);
    websCheckCleartextAuth(ENDPOINT, 1);
    teq(warnings, 0, "behind a trusted proxy the plain listener is not reported: %s", messages);

    websCloseRoute();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication and logging are required");
    return 0;
}
#endif
