/*
    With no CGI variable prefix, reserve PHP_, ORIG_ and UNIQUE_ID runtime meta-variables.
    Ordinary uppercase names remain exportable.
 */
#define ME_GOAHEAD_CGI_VAR_PREFIX ""
#include "cgi-handler-fixture.h"

#if ME_GOAHEAD_CGI && !VXWORKS
static cchar *runtimeNames[] = {
    "PHP_AUTH_USER", "PHP_AUTH_PW", "PHP_AUTH_TYPE", "PHP_AUTH_DIGEST", "PHP_SELF",
    "ORIG_SCRIPT_FILENAME", "ORIG_PATH_INFO", "ORIG_SCRIPT_NAME", "UNIQUE_ID", NULL
};
static int runtimeCounts[16], plainNames, upperNames, serverUsers;

static void countRuntime(char **env)
{
    char **ep;
    int  i;

    memset(runtimeCounts, 0, sizeof(runtimeCounts));
    plainNames = upperNames = serverUsers = 0;
    for (ep = env; ep && *ep; ep++) {
        if (sstarts(*ep, "REMOTE_USER=")) {
            serverUsers++;
            tmatch(*ep, "REMOTE_USER=server", "REMOTE_USER holds the server value");
            continue;
        }
        if (sstarts(*ep, "plain_name=")) {
            plainNames++;
            continue;
        }
        if (sstarts(*ep, "CMD=")) {
            upperNames++;
            continue;
        }
        for (i = 0; runtimeNames[i]; i++) {
            if (sncmp(*ep, runtimeNames[i], slen(runtimeNames[i])) == 0 && (*ep)[slen(runtimeNames[i])] == '=') {
                runtimeCounts[i]++;
            }
        }
    }
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS
    WebsHash vars;
    int      i;

    vars = hashCreate(-1);
    hashEnter(vars, "REMOTE_USER", valueString("server", VALUE_ALLOCATE), 0);
    hashEnter(vars, "AUTH_TYPE", valueString("", 0), 0);
    for (i = 0; runtimeNames[i]; i++) {
        hashEnter(vars, runtimeNames[i], valueString("client", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    }
    hashEnter(vars, "plain_name", valueString("x", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    hashEnter(vars, "CMD", valueString("status", VALUE_ALLOCATE), WEBS_VAR_BODY);
    inspectEnv = countRuntime;
    primaryMode = S_IFREG;
    runHandler(vars);
    teq(serverUsers, 1, "exactly one REMOTE_USER entry, the server's");
    teq(plainNames, 1, "an ordinary request variable is still exported");
    teq(upperNames, 1, "an upper-case request variable outside the reserved namespaces is still exported");
    for (i = 0; runtimeNames[i]; i++) {
        ttrue(runtimeCounts[i] == 0, "a request variable must not define the CGI runtime variable %s",
              runtimeNames[i]);
    }
    hashFree(vars);
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
