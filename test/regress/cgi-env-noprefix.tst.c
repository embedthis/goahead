/* With the CGI variable prefix cleared, a padded request variable cannot export a second server name. */
#define ME_GOAHEAD_CGI_VAR_PREFIX ""
#include "cgi-handler-fixture.h"

#if ME_GOAHEAD_CGI && !VXWORKS
static int remoteUsers, authTypes, others;

static void countEntries(char **env)
{
    char **ep;

    remoteUsers = authTypes = others = 0;
    for (ep = env; ep && *ep; ep++) {
        if (sstarts(*ep, "REMOTE_USER=")) {
            remoteUsers++;
            tmatch(*ep, "REMOTE_USER=", "REMOTE_USER holds the server value");
        } else if (sstarts(*ep, "AUTH_TYPE=")) {
            authTypes++;
            tmatch(*ep, "AUTH_TYPE=", "AUTH_TYPE holds the server value");
        } else {
            others++;
        }
    }
}
#endif

#if ME_GOAHEAD_CGI && !VXWORKS
/*
    Test case variants of protected environment names; Windows compares them case-insensitively.
 */
static cchar *variants[] = {
    "Path", "path", "PATH", "ComSpec", "PathExt", "SystemRoot", "WinDir", "Temp", "Tmp", "remote_user", "Auth_Type",
    "ld_preload", "LD_PRELOAD", NULL
};
static int variantCounts[16], serverUsers;

static void countVariants(char **env)
{
    char **ep;
    int  i;

    memset(variantCounts, 0, sizeof(variantCounts));
    serverUsers = 0;
    for (ep = env; ep && *ep; ep++) {
        if (sstarts(*ep, "REMOTE_USER=")) {
            serverUsers++;
            tmatch(*ep, "REMOTE_USER=server", "REMOTE_USER holds the server value");
            continue;
        }
        for (i = 0; variants[i]; i++) {
            if (sncmp(*ep, variants[i], slen(variants[i])) == 0 && (*ep)[slen(variants[i])] == '=') {
                variantCounts[i]++;
            }
        }
    }
}


static int exported(cchar *name)
{
    int i;

    for (i = 0; variants[i]; i++) {
        if (smatch(variants[i], name)) {
            return variantCounts[i];
        }
    }
    return -1;
}


static void caseVariants(void)
{
    WebsHash vars;
    int      i;

    vars = hashCreate(-1);
    hashEnter(vars, "REMOTE_USER", valueString("server", VALUE_ALLOCATE), 0);
    for (i = 0; variants[i]; i++) {
        hashEnter(vars, variants[i], valueString("client", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    }
    inspectEnv = countVariants;
    primaryMode = S_IFREG;
    runHandler(vars);
    teq(serverUsers, 1, "exactly one REMOTE_USER entry, the server's");
#if ME_WIN_LIKE
    for (i = 0; variants[i]; i++) {
        teq(variantCounts[i], 0, "a case variant of a protected name is not exported on Windows");
    }
#else
    teq(exported("PATH"), 0, "PATH is not exported");
    teq(exported("LD_PRELOAD"), 0, "LD_PRELOAD is not exported");
    teq(exported("Path"), 1, "names are case-sensitive on POSIX, so Path is exported as is");
    teq(exported("path"), 1, "path is exported as is");
    teq(exported("ld_preload"), 1, "ld_preload is exported as is");
    teq(exported("remote_user"), 1, "remote_user is a distinct name on POSIX");
#endif
    hashFree(vars);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS
    WebsHash vars;
    cchar    *padded[] = { "REMOTE_USER ", "REMOTE_USER\t", "\nREMOTE_USER", " AUTH_TYPE", "REMOTE_USER\r", NULL };
    int      i;

    vars = hashCreate(-1);
    hashEnter(vars, "REMOTE_USER", valueString("", VALUE_ALLOCATE), 0);
    hashEnter(vars, "AUTH_TYPE", valueString("", VALUE_ALLOCATE), 0);
    for (i = 0; padded[i]; i++) {
        hashEnter(vars, padded[i], valueString(strstr(padded[i], "AUTH") ? "Basic" : "admin", VALUE_ALLOCATE),
                  WEBS_VAR_QUERY);
    }
    hashEnter(vars, "a.b", valueString("x", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    hashEnter(vars, "plain_name", valueString("x", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    inspectEnv = countEntries;
    primaryMode = S_IFREG;
    runHandler(vars);
    teq(remoteUsers, 1, "exactly one REMOTE_USER entry");
    teq(authTypes, 1, "exactly one AUTH_TYPE entry");
    teq(others, 1, "only the portable request name is exported");
    hashFree(vars);
    caseVariants();
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
