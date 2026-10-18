/*
    fmt-bounds.tst.c - The formatter clamps field width and numeric precision
 */

#include "testme.h"
#include "goahead.h"

#define LIMIT ME_GOAHEAD_LIMIT_FORMAT_FIELD

static void suiteA_width(void)
{
    char *s;

    tinfo("\n--- Suite A: width ---\n");
    s = sfmt("%*d", LIMIT * 4, 1);
    ttrue(slen(s) == LIMIT, "a \"*\" width past the limit must be clamped, got %d bytes", (int) slen(s));
    ttrue(s[LIMIT - 1] == '1' && s[0] == ' ', "the clamped width must still right align");
    wfree(s);

    s = sfmt("%99999999999999999999d", 1);
    ttrue(slen(s) == LIMIT, "a literal width past INT_MAX must saturate and clamp, got %d bytes", (int) slen(s));
    wfree(s);

    s = sfmt("%*d", INT_MIN, 1);
    ttrue(slen(s) == LIMIT, "a width of INT_MIN must left align to the limit, got %d bytes", (int) slen(s));
    ttrue(s[0] == '1' && s[LIMIT - 1] == ' ', "a negative width must left align");
    wfree(s);

    s = sfmt("%*d", INT_MAX, 1);
    ttrue(slen(s) == LIMIT, "a width of INT_MAX must be clamped, got %d bytes", (int) slen(s));
    wfree(s);

    s = sfmt("%*s", INT_MAX, "x");
    ttrue(slen(s) == LIMIT, "a string width of INT_MAX must be clamped, got %d bytes", (int) slen(s));
    wfree(s);

#if ME_FLOAT
    s = sfmt("%300g", 1.5);
    ttrue(slen(s) == 300 && smatch(&s[296], " 1.5"), "a float wider than the conversion buffer must keep its number");
    wfree(s);

    s = sfmt("[%-8g][%08.2f]", 1.5, 1.5);
    ttrue(smatch(s, "[1.5     ][00001.50]"), "float width must align and zero fill, got %s", s);
    wfree(s);
#endif
}


static void suiteB_precision(void)
{
    char *s;

    tinfo("\n--- Suite B: numeric precision ---\n");
    s = sfmt("%.*d", LIMIT * 4, 7);
    ttrue(slen(s) == LIMIT, "a numeric precision past the limit must be clamped, got %d bytes", (int) slen(s));
    wfree(s);

    s = sfmt("%.99999999999999999999x", 7);
    ttrue(slen(s) == LIMIT, "a literal precision past INT_MAX must saturate and clamp, got %d bytes",
        (int) slen(s));
    wfree(s);

    s = sfmt("%.*d", INT_MAX, 7);
    ttrue(slen(s) == LIMIT, "a precision of INT_MAX must be clamped, got %d bytes", (int) slen(s));
    wfree(s);

#if ME_FLOAT
    s = sfmt("%.*f", INT_MAX, 1.5);
    ttrue(sstarts(s, "1.500"), "a float precision of INT_MAX must still format, got %.20s", s);
    wfree(s);
#endif
}


static void suiteC_string(void)
{
    char *big, *s;
    int  len;

    tinfo("\n--- Suite C: string precision ---\n");
    len = LIMIT * 2;
    big = walloc(len + 1);
    memset(big, 'a', len);
    big[len] = '\0';
    s = sfmt("%.*s", len, big);
    ttrue(slen(s) == len, "string precision must not be clamped, got %d bytes", (int) slen(s));
    wfree(s);
    wfree(big);
}


static void suiteD_ordinary(void)
{
    char buf[80];

    tinfo("\n--- Suite D: ordinary widths ---\n");
    fmt(buf, sizeof(buf), "[%5d][%-5d][%.3d][%05d][%*d]", 42, 42, 7, 42, 4, 9);
    ttrue(smatch(buf, "[   42][42   ][007][00042][   9]"), "ordinary fields are unchanged, got %s", buf);

    fmt(buf, sizeof(buf), "[%.2s][%8.3s]", "abcdef", "abcdef");
    ttrue(smatch(buf, "[ab][     abc]"), "string precision and width are unchanged, got %s", buf);
}


int main(int argc, char **argv)
{
    suiteA_width();
    suiteB_precision();
    suiteC_string();
    suiteD_ordinary();
    return 0;
}
