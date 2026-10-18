/* Verify primary and .exe fallback checks use the complete file-type field. */
#include "cgi-handler-fixture.h"
int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS && defined(S_IFSOCK)
    int modes[] = { S_IFREG, S_IFDIR, S_IFIFO, S_IFSOCK, S_IFCHR, S_IFBLK };
    int i, fallback;
    checkEnv = 0;
    for (fallback = 0; fallback < 2; fallback++) {
        for (i = 0; i < 6; i++) {
            primaryMode = fallback ? 0 : modes[i];
            fallbackMode = fallback ? modes[i] : 0;
            runHandler(-1);
            if (modes[i] == S_IFREG) {
                teq(tempCalls, 2, "regular program reaches controlled spawn failure");
            } else {
                teq(tempCalls, 0, "non-regular object is rejected before spawn");
                teq(reportedCode, HTTP_CODE_NOT_FOUND, "non-regular program is not found");
            }
        }
    }
#else
    tskip("POSIX CGI file types unavailable");
#endif
    return 0;
}
