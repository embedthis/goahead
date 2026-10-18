/* Zero is a valid hash handle, including when the hash is empty. */
#include "cgi-handler-fixture.h"
int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS
    WebsHash vars;
    vars = hashCreate(17);
    teq(vars, 0, "first hash occupies handle zero");
    checkEnv = 1;
    primaryMode = S_IFREG;
    expectedEntry = NULL;
    runHandler(vars);
    teq(tempCalls, 2, "empty hash reaches controlled spawn failure");
    hashEnter(vars, "TEST_ENV", valueString("hello", VALUE_ALLOCATE), 0);
    expectedEntry = "TEST_ENV=hello";
    runHandler(vars);
    teq(tempCalls, 2, "populated zero hash reaches controlled spawn failure");
    hashFree(vars);
    expectedEntry = NULL;
    runHandler(-1);
    teq(tempCalls, 2, "invalid hash produces empty terminated environment");
#else
    tskip("CGI file-check backend unavailable");
#endif
    return 0;
}
