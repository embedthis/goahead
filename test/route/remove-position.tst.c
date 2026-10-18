/* Removing a route at full allocation capacity must not read the next pointer. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
/* Use the production module's normal signed-conversion policy. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/route.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
    WebsRoute *original[16];
    char name[32];
    int pos, i, rc;
    for (pos = 0; pos < 16; pos++) {
        rc = websOpenRoute();
        teq(rc, 0, "open table");
        for (i = 0; i < 16; i++) {
            fmt(name, sizeof(name), "/route-%d", i);
            original[i] = websAddRoute(name, "continue", -1);
        }
        teq(routeCount, routeMax, "exercise exact allocation boundary under ASan");
        fmt(name, sizeof(name), "/route-%d", pos);
        rc = websRemoveRoute(name);
        teq(rc, 0, "remove route");
        teq(routeCount, 15, "one route removed");
        for (i = 0; i < 15; i++) {
            teqp(routes[i], original[i < pos ? i : i + 1], "remaining routes retain order");
        }
        rc = websRemoveRoute(name);
        teq(rc, -1, "already removed route is absent");
        websCloseRoute();
    }
    rc = websOpenRoute();
    teq(rc, 0, "open single-entry table");
    websAddRoute("/only", "continue", -1);
    rc = websRemoveRoute("/only");
    teq(rc, 0, "remove sole route");
    teq(routeCount, 0, "table is empty");
    websCloseRoute();
    return 0;
}
