/*
    strict-default.tst.c - Strict mode is on unless the build sets ME_GOAHEAD_STRICT to 0
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

int main(int argc, char **argv)
{
    ttrue(websGetStrict(), "the default build must start in strict mode");
    websSetStrict(0);
    ttrue(!websGetStrict(), "websSetStrict can still turn it off");
    return 0;
}
