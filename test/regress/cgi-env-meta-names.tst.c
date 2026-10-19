/*
    With an empty CGI variable prefix, reject request variables in the CGI meta-variable namespace
    while allowing ordinary names.
 */
#define ME_GOAHEAD_CGI_VAR_PREFIX ""
#include "cgi-handler-fixture.h"

#if ME_GOAHEAD_CGI && !VXWORKS
/*
    Reserved meta-variable names, including those the server leaves unset.
 */
static cchar *metaNames[] = {
    "DOCUMENT_ROOT", "HTTPS", "REMOTE_PORT", "REQUEST_SCHEME", "SERVER_ADMIN", "REDIRECT_STATUS",
    "SSL_CLIENT_VERIFY", "CONTEXT_DOCUMENT_ROOT", "SCRIPT_URL", "PATH_TRANSLATED", NULL
};
static int metaCounts[16], plainNames, serverUsers;

static void countMeta(char **env)
{
    char **ep;
    int  i;

    memset(metaCounts, 0, sizeof(metaCounts));
    plainNames = serverUsers = 0;
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
        for (i = 0; metaNames[i]; i++) {
            if (sncmp(*ep, metaNames[i], slen(metaNames[i])) == 0 && (*ep)[slen(metaNames[i])] == '=') {
                metaCounts[i]++;
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
    //  websSetEnv always sets AUTH_TYPE, empty for an unauthenticated request
    hashEnter(vars, "AUTH_TYPE", valueString("", 0), 0);
    for (i = 0; metaNames[i]; i++) {
        hashEnter(vars, metaNames[i], valueString("client", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    }
    hashEnter(vars, "plain_name", valueString("x", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    inspectEnv = countMeta;
    primaryMode = S_IFREG;
    runHandler(vars);
    teq(serverUsers, 1, "exactly one REMOTE_USER entry, the server's");
    teq(plainNames, 1, "an ordinary request variable is still exported");
    for (i = 0; metaNames[i]; i++) {
        ttrue(metaCounts[i] == 0, "a request variable must not define the CGI meta-variable %s", metaNames[i]);
    }
    hashFree(vars);
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
