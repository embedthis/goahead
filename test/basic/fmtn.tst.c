/*
    fmtn.tst.c - Regression tests asserting that GoAhead's formatter has no %n conversion.

    GoAhead does not use the platform printf; sprintfCore() in src/runtime.c backs
    fmt/sfmt/sfmtv/websWrite/websWriteHeader/error/trace/logmsg. It used to implement %n, which
    writes the running output count through a pointer taken from the varargs region. That turns
    any reachable format-string bug into a write-what-where primitive, and the glibc
    _FORTIFY_SOURCE %n defence does not apply because libc printf is never called.

    The fix maps 'n' to CLASS_NORMAL in classMap and deletes case 'n' from sprintfCore, so "%n"
    is treated like any other unknown conversion: it emits a literal "n" and consumes no vararg.

    Four suites:
      O) the guard oracle itself detects a store, so suite A's verdict means something
      A) %n and its modifier/width spellings never write through the supplied pointer
      B) %n renders as a literal "n" in every position
      C) the neighbouring conversions still work (the classMap edit disturbed nothing else)

    Specs are routed through runSpec()/runFmt() rather than called with literal formats so the
    compiler's own -Wformat checking does not object to the deliberately bogus %n arguments.
    These tests target the runtime formatter, not the compile-time checker.
 */

#include "testme.h"
#include "goahead.h"

/*
    Sentinel written either side of the target so a short-, long- or int-width store is caught
    whatever its size. A live %n would set target to the number of chars emitted so far.
 */
#define SENTINEL 0x5A5A5A5A

typedef struct Guard {
    int before;
    int target;
    int after;
} Guard;

static void guardInit(Guard *g)
{
    g->before = SENTINEL;
    g->target = SENTINEL;
    g->after = SENTINEL;
}

static int guardIntact(Guard *g)
{
    return g->before == SENTINEL && g->target == SENTINEL && g->after == SENTINEL;
}

static char *runSpec(cchar *spec, void *arg)
{
    return sfmt(spec, arg);
}

static void runFmt(char *buf, ssize size, cchar *spec, void *arg)
{
    fmt(buf, size, spec, arg);
}

/*
    Run `spec` through sfmt() with &guard.target as the sole vararg. Assert the guard is
    untouched and the result matches `expected`.
 */
#define CASE_NOWRITE(label, spec, expected) do { \
    Guard g; \
    char  *result; \
    guardInit(&g); \
    result = runSpec((spec), &g.target); \
    ttrue(guardIntact(&g), \
          "%s: spec=[%s] wrote through the pointer (before=%d target=%d after=%d)", \
          (label), (spec), g.before, g.target, g.after); \
    ttrue(result && strcmp(result, (expected)) == 0, \
          "%s: spec=[%s] expected=[%s] got=[%s]", \
          (label), (spec), (expected), result ? result : "(NULL)"); \
    wfree(result); \
} while (0)

/* Same, but through fmt() into a caller buffer - the other public entry point. */
#define CASE_NOWRITE_FMT(label, spec, expected) do { \
    Guard g; \
    char  buf[128]; \
    guardInit(&g); \
    buf[0] = '\0'; \
    runFmt(buf, sizeof(buf), (spec), &g.target); \
    ttrue(guardIntact(&g), \
          "%s: spec=[%s] wrote through the pointer (before=%d target=%d after=%d)", \
          (label), (spec), g.before, g.target, g.after); \
    ttrue(strcmp(buf, (expected)) == 0, \
          "%s: spec=[%s] expected=[%s] got=[%s]", (label), (spec), (expected), buf); \
} while (0)

/* Rendering check. The vararg is surplus - a correct build consumes none of it. */
#define CASE_RENDER(label, spec, expected) do { \
    char *result = runSpec((spec), NULL); \
    ttrue(result && strcmp(result, (expected)) == 0, \
          "%s: spec=[%s] expected=[%s] got=[%s]", \
          (label), (spec), (expected), result ? result : "(NULL)"); \
    wfree(result); \
} while (0)

/********************************* Suite O: oracle self-check *******************/
/*
    A test that can never fail proves nothing. Suite A's whole verdict rests on guardIntact()
    noticing a store through &g.target, so prove the detector is live before trusting it:
    a store of any width - short, int or long - must be seen.

    Sub-int-width stores go through memcpy rather than a punned pointer cast: casting an int*
    to short* and dereferencing violates strict aliasing, which a compiler is free to optimise
    away at -O2. memcpy expresses the same byte-level store with defined behaviour everywhere.

    This writes to a local struct only; it does not exercise the formatter.
 */
static void suiteO_oracle(void)
{
    Guard g;
    short sval;
    char  cval;

    tinfo("\n--- Suite O: guard oracle self-check ---\n");

    guardInit(&g);
    ttrue(guardIntact(&g), "O1 pristine guard reports intact");

    guardInit(&g);
    g.target = 8;
    ttrue(!guardIntact(&g), "O2 int-width store detected");

    guardInit(&g);
    sval = 8;
    memcpy(&g.target, &sval, sizeof(sval));
    ttrue(!guardIntact(&g), "O3 short-width store detected");

    guardInit(&g);
    cval = 8;
    memcpy(&g.target, &cval, sizeof(cval));
    ttrue(!guardIntact(&g), "O4 byte-width store detected");

    /* A zero count - what a %n at offset 0 would store - must also be caught */
    guardInit(&g);
    g.target = 0;
    ttrue(!guardIntact(&g), "O5 zero store detected");
}

/********************************* Suite A: no write ****************************/

static void suiteA_noWrite(void)
{
    tinfo("\n--- Suite A: %%n never writes through the vararg pointer ---\n");

    CASE_NOWRITE("A1  bare",            "%n",            "n");
    CASE_NOWRITE("A2  short",           "%hn",           "n");
    CASE_NOWRITE("A3  long",            "%ln",           "n");
    CASE_NOWRITE("A4  long-long",       "%lln",          "n");
    CASE_NOWRITE("A5  cap-L",           "%Ln",           "n");
    CASE_NOWRITE("A6  width",           "%10n",          "n");
    CASE_NOWRITE("A7  precision",       "%.10n",         "n");
    CASE_NOWRITE("A8  width-precision", "%10.4n",        "n");
    CASE_NOWRITE("A9  zero-pad",        "%08n",          "n");
    CASE_NOWRITE("A10 left",            "%-10n",         "n");
    CASE_NOWRITE("A11 alternate",       "%#n",           "n");
    CASE_NOWRITE("A12 sign",            "%+n",           "n");
    CASE_NOWRITE("A13 space",           "% n",           "n");
    CASE_NOWRITE("A14 comma",           "%,n",           "n");
    CASE_NOWRITE("A15 all-modifiers",   "%-+ #,012.6ln", "n");

    /* Repeated %n - the classic "walk the varargs and write" shape */
    CASE_NOWRITE("A16 repeated", "%n%n%n%n%n%n%n%n", "nnnnnnnn");

    /* %n reached after output has accumulated - a live %n would store a non-zero count */
    CASE_NOWRITE("A17 after-text", "AAAAAAAA%n", "AAAAAAAAn");

    /* The same specs through fmt() into a caller buffer */
    CASE_NOWRITE_FMT("A18 fmt-bare",     "%n",     "n");
    CASE_NOWRITE_FMT("A19 fmt-short",    "%hn",    "n");
    CASE_NOWRITE_FMT("A20 fmt-long",     "%ln",    "n");
    CASE_NOWRITE_FMT("A21 fmt-repeated", "%n%n%n", "nnn");
}

/********************************* Suite B: rendering ***************************/
/*
    "%n" must render as a literal "n" wherever it appears, with the '%' swallowed - exactly how
    any other unknown conversion (e.g. "%m", "%j") is already handled.
 */
static void suiteB_render(void)
{
    Guard g;
    char  *result;

    tinfo("\n--- Suite B: %%n renders as a literal 'n' ---\n");

    CASE_RENDER("B1 leading",       "%nabc",   "nabc");
    CASE_RENDER("B2 trailing",      "abc%n",   "abcn");
    CASE_RENDER("B3 embedded",      "a%nb",    "anb");
    CASE_RENDER("B4 only",          "%n",      "n");
    CASE_RENDER("B5 doubled-pct",   "%%n",     "%n");
    CASE_RENDER("B6 like-unknown",  "%m",      "m");
    CASE_RENDER("B7 like-unknown2", "%j",      "j");
    CASE_RENDER("B8 padded",        "%.1024n", "n");

    /*
        B9: the attacker-shaped payload - leak conversions followed by the write primitive.
        Every vararg is consumed by the %x pair; %n must take none and store nothing.
     */
    guardInit(&g);
    result = sfmt("%x-%x-%n", 0xdead, 0xbeef, &g.target);
    ttrue(guardIntact(&g),
          "B9 poc-chain: wrote through the pointer (before=%d target=%d after=%d)",
          g.before, g.target, g.after);
    ttrue(result && strcmp(result, "dead-beef-n") == 0,
          "B9 poc-chain: expected=[dead-beef-n] got=[%s]", result ? result : "(NULL)");
    wfree(result);
}

/********************************* Suite C: neighbours **************************/
/*
    The classMap edit changed exactly one byte. Confirm the conversions on either side of 'n'
    (and the common ones) still behave.
 */
static void suiteC_neighbours(void)
{
    char *result;

    tinfo("\n--- Suite C: neighbouring conversions unaffected ---\n");

    result = sfmt("%d", 42);
    ttrue(result && strcmp(result, "42") == 0, "C1 %%d got=[%s]", result ? result : "(NULL)");
    wfree(result);

    result = sfmt("%s", "hello");
    ttrue(result && strcmp(result, "hello") == 0, "C2 %%s got=[%s]", result ? result : "(NULL)");
    wfree(result);

    result = sfmt("%o", 8);
    ttrue(result && strcmp(result, "10") == 0, "C3 %%o got=[%s]", result ? result : "(NULL)");
    wfree(result);

    result = sfmt("%x", 255);
    ttrue(result && strcmp(result, "ff") == 0, "C4 %%x got=[%s]", result ? result : "(NULL)");
    wfree(result);

    result = sfmt("%u", 7);
    ttrue(result && strcmp(result, "7") == 0, "C5 %%u got=[%s]", result ? result : "(NULL)");
    wfree(result);

    result = sfmt("%c", 'z');
    ttrue(result && strcmp(result, "z") == 0, "C6 %%c got=[%s]", result ? result : "(NULL)");
    wfree(result);

    result = sfmt("%ld", (long) 123456);
    ttrue(result && strcmp(result, "123456") == 0, "C7 %%ld got=[%s]", result ? result : "(NULL)");
    wfree(result);

    result = sfmt("%-6dX", 5);
    ttrue(result && strcmp(result, "5     X") == 0, "C8 %%-6d got=[%s]", result ? result : "(NULL)");
    wfree(result);

    /* 'i' and 'o' bracket 'n' in classMap; both must still be CLASS_TYPE */
    result = sfmt("%i", 9);
    ttrue(result && strcmp(result, "9") == 0, "C9 %%i got=[%s]", result ? result : "(NULL)");
    wfree(result);
}

/********************************* Main ****************************************/

int main(int argc, char **argv)
{
    tinfo("fmtn: regression - sprintfCore must not implement %%n\n");

    suiteO_oracle();
    suiteA_noWrite();
    suiteB_render();
    suiteC_neighbours();

    return 0;
}
