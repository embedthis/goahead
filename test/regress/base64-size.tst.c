/* Check the encoder's actual allocation boundary, including allocator slack. */
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
static ssize requested;
static void *guardedAlloc(ssize size)
{
    uchar *ptr;
    requested = size;
    ptr = walloc(size + 16);
    memset(ptr + size, 0xA5, 16);
    return ptr;
}
#define walloc guardedAlloc
/* Compile the production module with its normal signed-conversion policy. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/crypt.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef walloc
int main(int argc, char **argv)
{
    cchar *expected[] = { "", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy" };
    char *encoded;
    int len, i;
    for (len = 0; len <= 6; len++) {
        encoded = websEncode64Block("foobar", len);
        tmatch(encoded, expected[len], "base64 known vector");
        for (i = 0; i < 16; i++) {
            teq((uchar) encoded[requested + i], 0xA5, "encoder must stay inside its allocation");
        }
        wfree(encoded);
    }
    encoded = websEncode64Block("", -1);
    tnull(encoded, "negative input length rejected");
    encoded = websEncode64Block("", SSIZE_MAX);
    tnull(encoded, "unrepresentable output size rejected before reading input");
    return 0;
}
