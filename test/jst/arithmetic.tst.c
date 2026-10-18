/* Checked integer arithmetic must reject undefined C operations and remain reusable. */
#include "testme.h"
#include "goahead.h"
#include "js.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    cchar *bad[] = {"2147483647+1;", "2147483647*2;", "1<<32;", "1<<31;",
        "2147483648+0;", "999999999999999999999999+0;", "var a=2147483647; a++;",
        "var a=\"-2147483648\"; var b=\"-1\"; var result=a/b;", NULL};
    cchar *good[] = {"2147483647+0;", "0-1;", "1<<30;", "8>>2;", "7/0;", "7%0;", "-7/2;", "-7%2;", NULL};
    cchar *values[] = {"2147483647", "-1", "1073741824", "2", "0", "0", "-3", "-1"};
    cchar *modulo = "var a=\"-2147483648\"; var b=\"-1\"; var result=a%b; result;";
    char *result, *error, *script;
    int jid, i;
    jid = jsOpenEngine(-1, -1);
    for (i = 0; bad[i]; i++) {
        error = NULL;
        script = sstarts(bad[i], "var ") ? sclone(bad[i]) : sfmt("var result=%s", bad[i]);
        result = jsEval(jid, script, &error);
        wfree(script);
        tnull(result, "Invalid integer operation must fail: %s", bad[i]);
        tnotnull(error, "Invalid operation must report an error");
        wfree(error);
    }
    for (i = 0; good[i]; i++) {
        error = NULL;
        script = sfmt("var result=%s result;", good[i]);
        result = jsEval(jid, script, &error);
        wfree(script);
        tnull(error, "Valid integer operation succeeds: %s", good[i]);
        tnotnull(result, "Valid operation produces a result");
        if (result) {
            tmatch(result, values[i], "Checked result");
        }
        wfree(error);
    }
    error = NULL;
    result = jsEval(jid, modulo, &error);
    tnull(error, "INT_MIN %% -1 is representable");
    tmatch(result, "0", "INT_MIN %% -1 is zero");
    wfree(error);
    jsCloseEngine(jid);
#endif
    return 0;
}
