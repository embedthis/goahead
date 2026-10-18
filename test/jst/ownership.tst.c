/*
    ownership.tst.c -- Engine-owned frames are released; caller tables survive.
 */
#include "testme.h"
#include "goahead.h"
#include "js.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    WebsHash globals, functions, expected, probe;
    WebsKey *key;
    char *error;
    int jid, i;

    globals = hashCreate(64);
    functions = hashCreate(64);
    hashEnter(globals, "caller", valueString("preserved", VALUE_ALLOCATE), 0);
    for (i = 0; i < 20; i++) {
        expected = hashCreate(64);
        hashFree(expected);
        jid = jsOpenEngine(globals, functions);
        jsOpenBlock(jid);
        jsEval(jid, "var outer='owned';", NULL);
        jsOpenBlock(jid);
        error = NULL;
        jsEval(jid, "var inner='owned'; missing;", &error);
        tnotnull(error, "Evaluation fails with frames outstanding");
        wfree(error);
        jsCloseEngine(jid);
        probe = hashCreate(64);
        teqi(probe, expected, "All engine frames must release their hash handles");
        hashFree(probe);
        key = hashLookup(globals, "caller");
        tnotnull(key, "Caller globals survive engine close");
        if (key) {
            tmatch(key->content.value.string, "preserved", "Caller value preserved");
        }
        hashEnter(functions, "alive", valueInteger(i), 0);
    }
    hashFree(functions);
    hashFree(globals);
#endif
    return 0;
}
