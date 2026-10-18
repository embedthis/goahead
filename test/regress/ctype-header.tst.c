/* A checked libc boundary catches signed ctype arguments even on tolerant libcs. */
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
#include <locale.h>
static int checkedPrint(int value)
{
    ttrue(value >= 0 && value <= UCHAR_MAX, "isprint receives an unsigned byte");
    return isprint((unsigned char) value);
}
#undef isprint
#define isprint checkedPrint
/* Compile the production module with its normal signed-conversion policy. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef isprint
int main(int argc, char **argv)
{
    char input[2], *result;
    int value, kind;
    setlocale(LC_CTYPE, "C");
    for (kind = TOKEN_HEADER_KEY; kind <= TOKEN_HEADER_VALUE; kind++) {
        for (value = 128; value <= 255; value++) {
            input[0] = (char) value;
            input[1] = '\0';
            result = validateToken(input, input + 1, kind);
            tnull(result, "high-bit header byte is rejected");
        }
    }
    return 0;
}
