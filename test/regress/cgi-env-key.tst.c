/* CGI environment construction drops non-portable names and preserves request-variable keys. */
#include "cgi-handler-fixture.h"
int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS
    WebsHash vars;
    WebsKey *key;
    int i;
    cchar *names[] = { "foo ", " \tfoo\r\n", " LD_PRELOAD " };
    for (i = 0; i < 3; i++) {
        vars = hashCreate(-1);
        hashEnter(vars, names[i], valueString("bar", VALUE_ALLOCATE), 0);
        checkEnv = 1;
        expectedEntry = NULL;
        primaryMode = S_IFREG;
        runHandler(vars);
        key = hashLookup(vars, names[i]);
        tnotnull(key, "original request variable remains retrievable");
        if (key) tmatch(key->name.value.string, names[i], "stored key is unchanged");
        hashFree(vars);
    }
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
