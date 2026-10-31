/*
    cgi-env-blacklist-gaps.tst.c - With the CGI variable prefix cleared, a request variable cannot name a
    process-level control variable the blacklist omits: HOME, TMPDIR, the OpenSSL loader and configuration
    variables, the Java launcher option variables, the versioned Lua startup variables, PERL5SHELL and TCLLIBPATH.

    These are only reachable when ME_GOAHEAD_CGI_VAR_PREFIX is built empty; the default CGI_ prefix neutralises
    every request variable name. Sibling of tickets 10094 and 10357, which established that the empty-prefix
    build must hold its own.
 */
#define ME_GOAHEAD_CGI_VAR_PREFIX ""
#include "cgi-handler-fixture.h"

#if ME_GOAHEAD_CGI && !VXWORKS
static cchar *controls[] = {
    "HOME", "TMPDIR", "OPENSSL_CONF", "OPENSSL_MODULES", "OPENSSL_ENGINES", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS",
    "LUA_INIT_5_4", "LUA_PATH_5_4", "LUA_CPATH_5_4", "PERL5SHELL", "TCLLIBPATH", NULL
};
static int exportedCounts[16], plainNames;

static void countControls(char **env)
{
    char **ep;
    int  i;

    memset(exportedCounts, 0, sizeof(exportedCounts));
    plainNames = 0;
    for (ep = env; ep && *ep; ep++) {
        if (sstarts(*ep, "plain_name=")) {
            plainNames++;
        }
        for (i = 0; controls[i]; i++) {
            if (sncmp(*ep, controls[i], slen(controls[i])) == 0 && (*ep)[slen(controls[i])] == '=') {
                exportedCounts[i]++;
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
    hashEnter(vars, "REMOTE_USER", valueString("", VALUE_ALLOCATE), 0);
    hashEnter(vars, "AUTH_TYPE", valueString("", VALUE_ALLOCATE), 0);
    for (i = 0; controls[i]; i++) {
        hashEnter(vars, controls[i], valueString("/staged/by/client", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    }
    hashEnter(vars, "plain_name", valueString("x", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    inspectEnv = countControls;
    primaryMode = S_IFREG;
    runHandler(vars);
    teq(plainNames, 1, "an ordinary request name is still exported");
    for (i = 0; controls[i]; i++) {
        teq(exportedCounts[i], 0, "%s is a process control variable and is not exported", controls[i]);
    }
    hashFree(vars);
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
