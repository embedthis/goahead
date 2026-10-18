/* Check the ctype argument contract at the real lowercase implementation. */
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
static int checkedUpper(int value)
{
    ttrue(value >= 0 && value <= UCHAR_MAX, "isupper receives an unsigned byte");
    return isupper((unsigned char) value);
}
#undef isupper
#define isupper checkedUpper
/* Compile the production module with its normal signed-conversion policy. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/runtime.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef isupper
int main(int argc, char **argv)
{
    char input[3];
    int value;
    setlocale(LC_CTYPE, "C");
    for (value = 128; value <= 255; value++) {
        input[0] = (char) value;
        input[1] = 'A';
        input[2] = '\0';
        slower(input);
        teq((uchar) input[0], value, "high-bit byte preserved");
        teq(input[1], 'a', "ASCII folded");
    }
    return 0;
}
