/* Exercise the standalone CGI utility parser and its allocation ownership. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "testme.h"
static int liveBlocks;
static char *queryInput;
static void *countAlloc(size_t size) { void *p = malloc(size); if (p) liveBlocks++; return p; }
static void countFree(void *p) { if (p) liveBlocks--; free(p); }
static char *testGetenv(const char *key) { return strcmp(key, "QUERY_STRING") == 0 ? queryInput : NULL; }
#define malloc countAlloc
#define free countFree
#define getenv testGetenv
#define main cgitestMain
#define error cgitestError
#include "../../src/cgitest.c"
#undef main
#undef error
#undef malloc
#undef free
#undef getenv
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
    char **keys = NULL;
    char input[] = "&&first=one&&second=two&";
    int count;

    count = getVars(&keys, input, strlen(input));
    teq(count, 2, "empty fields do not inflate initialized pair count");
    if (count == 2) {
        ttrue(strcmp(keys[0], "first") == 0, "first key");
        ttrue(strcmp(keys[1], "one") == 0, "first value");
        ttrue(strcmp(keys[2], "second") == 0, "second key");
        ttrue(strcmp(keys[3], "two") == 0, "second value");
    }
    countFree(keys);
    teq(liveBlocks, 0, "parser allocation fully released");
    keys = NULL;
    count = getVars(&keys, "&&&", 3);
    teq(count, 0, "separator-only input has no pairs");
    countFree(keys);
    teq(liveBlocks, 0, "parser allocation fully released");
    keys = NULL;
    count = getVars(&keys, "bare&empty=&encoded=a%20b", strlen("bare&empty=&encoded=a%20b"));
    teq(count, 3, "bare and empty values retained");
    if (count == 3) {
        ttrue(keys[1] == NULL, "bare key has absent value");
        ttrue(strcmp(keys[3], "") == 0, "explicit empty value");
        ttrue(strcmp(keys[5], "a b") == 0, "percent decoding");
    }
    countFree(keys);
    teq(liveBlocks, 0, "parser allocation fully released");
    {
        char *args[] = {"cgitest", NULL};
        char **result;
        int resultCount, rc;
        queryInput = "a=1&b=2&HTTP_SWITCHES=-q";
        rc = getArgv(&resultCount, &result, 1, args);
        teq(rc, 0, "parse query switches");
        teq(resultCount, 2, "switch lookup visits final pair");
        ttrue(strcmp(result[1], "-q") == 0, "late query switch honored");
        finish();
        teq(liveBlocks, 0, "finish releases query storage");
        queryInput = NULL;
        rc = getQueryString(&queryBuf, &queryLen);
        teq(rc, 0, "absent query accepted");
        teq((int) queryLen, 0, "absent query empty");
    }
    {
        /*
            Decode only complete escapes without reading past the terminator.
         */
        struct { const char *in, *out; } cases[] = {
            { "a%2", "a%2" }, { "a%", "a%" }, { "%", "%" }, { "%zz", "%zz" }, { "%4", "%4" }, { "a%2g", "a%2g" },
            { "%41%42", "AB" }, { "%e9x", "\xe9x" }, { "a%20b", "a b" }, { NULL, NULL }
        };
        char buf[32];
        int  i;

        for (i = 0; cases[i].in; i++) {
            memset(buf, 'Z', sizeof(buf));
            strcpy(buf, cases[i].in);
            descape(buf);
            ttrue(strcmp(buf, cases[i].out) == 0, "descape \"%s\" must give \"%s\", got \"%s\"", cases[i].in,
                  cases[i].out, buf);
        }
    }
    return 0;
}
