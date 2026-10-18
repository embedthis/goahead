/*
    win-arg-whitespace.tst - Windows command-line quoting round-trips every argv shape
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

#if ME_GOAHEAD_CGI
typedef struct ArgCase {
    cchar *arg;
    cchar *expected;
} ArgCase;

static ArgCase cases[] = {
    { "a\nb",           "\"a\nb\"" },
    { "a\vb",           "\"a\vb\"" },
    { "a\rb",           "\"a\rb\"" },
    { "a\fb",           "\"a\fb\"" },
    { "a\tb",           "\"a\tb\"" },
    { "a b",            "\"a b\"" },

    { "plain",          "plain" },
    { "&",              "&" },
    { "|",              "|" },
    { "<",              "<" },
    { ">",              ">" },
    { "^",              "^" },
    { "%",              "%" },
    { "(",              "(" },
    { ")",              ")" },
    { "path\\file",     "path\\file" },
    { "trail\\\\\\\\",  "trail\\\\\\\\" },
    { "caf\xC3\xA9",    "caf\xC3\xA9" },

    { "a\"b",           "\"a\\\"b\"" },
    { "\"",             "\"\\\"\"" },

    { "\\\"",           "\"\\\\\\\"\"" },
    { "a\\\\\"b",       "\"a\\\\\\\\\\\"b\"" },

    { "a b\\",          "\"a b\\\\\"" },
    { "a b\\\\",        "\"a b\\\\\\\\\"" },

    { "a\\ b",          "\"a\\ b\"" },

    { "",               "\"\"" },
    { 0, 0 }
};
#endif

int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI
    char  output[256], cmdline[256];
    char  *argp[4];
    ssize len;
    int   i;

    for (i = 0; cases[i].arg; i++) {
        len = appendWinArg(output, sizeof(output), cases[i].arg);
        output[len] = '\0';
        tmatch(output, cases[i].expected, "argument \"%s\" is quoted for the receiving CRT", cases[i].arg);
        teq(len, (ssize) strlen(cases[i].expected), "reported length matches what was written");
    }

    argp[0] = (char*) "prog.exe";
    argp[1] = (char*) "a b";
    argp[2] = (char*) "c";
    argp[3] = 0;
    len = buildWinCmdLine(argp, cmdline, sizeof(cmdline));
    tmatch(cmdline, "prog.exe \"a b\" c", "arguments are joined with one space each");
    teq(len, (ssize) strlen("prog.exe \"a b\" c"), "reported length matches the command line");

    argp[1] = (char*) "";
    len = buildWinCmdLine(argp, cmdline, sizeof(cmdline));
    tmatch(cmdline, "prog.exe \"\" c", "an empty argument keeps its place in the command line");

    teq(buildWinCmdLine(NULL, cmdline, sizeof(cmdline)), (ssize) 0, "a null argument list writes nothing");
    teq(appendWinArg(NULL, sizeof(output), "x"), (ssize) 0, "a null destination writes nothing");
#else
    tskip("CGI disabled");
#endif
    return 0;
}
