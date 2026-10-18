/* Exercise the real route implementation with a checked memory move. */
/* TestMe's variadic reporting helpers forward format strings. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
static size_t moveLimit;
static void *checkedMove(void *dest, const void *src, size_t len)
{
    ttrue(len <= moveLimit, "route insertion must not copy beyond the live tail");
    return memmove(dest, src, len <= moveLimit ? len : moveLimit);
}
#undef memmove
#define memmove checkedMove
/* Compile the production module with its normal signed-conversion policy. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/route.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef memmove
int main(int argc, char **argv)
{
    int pos, i, rc;
    WebsRoute *original[16], *added;
    for (pos = 0; pos <= 16; pos++) {
        rc = websOpenRoute();
        teq(rc, 0, "open route table");
        for (i = 0; i < 16; i++) {
            original[i] = websAddRoute("/original", "continue", -1);
        }
        moveLimit = sizeof(WebsRoute*) * (size_t) (16 - pos);
        added = websAddRoute("/inserted", "continue", pos);
        teq(routeCount, 17, "insert adds one entry");
        teqp(routes[pos], added, "inserted route is at requested position");
        for (i = 0; i < 16; i++) {
            teqp(routes[i < pos ? i : i + 1], original[i], "existing routes retain their order");
        }
        websCloseRoute();
    }
    return 0;
}
