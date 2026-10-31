/*
    With no CGI variable prefix, refuse proxy environment variables in both upper and lower case.
 */
#define ME_GOAHEAD_CGI_VAR_PREFIX ""
#include "cgi-handler-fixture.h"

#if ME_GOAHEAD_CGI && !VXWORKS
/*
    Each spelling a proxy-aware client library reads, plus one upper-case entry already refused as a control.
 */
static cchar *proxyNames[] = {
    "http_proxy", "https_proxy", "all_proxy", "ftp_proxy", "no_proxy",
    "HTTPS_PROXY", "ALL_PROXY", "FTP_PROXY", "NO_PROXY", "HTTP_PROXY", NULL
};
static int exportedCounts[16], plainNames;

static void countProxyNames(char **env)
{
    char **ep;
    int  i;

    memset(exportedCounts, 0, sizeof(exportedCounts));
    plainNames = 0;
    for (ep = env; ep && *ep; ep++) {
        if (sstarts(*ep, "plain_name=")) {
            plainNames++;
        }
        for (i = 0; proxyNames[i]; i++) {
            if (sncmp(*ep, proxyNames[i], slen(proxyNames[i])) == 0 && (*ep)[slen(proxyNames[i])] == '=') {
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
    for (i = 0; proxyNames[i]; i++) {
        hashEnter(vars, proxyNames[i], valueString("http://127.0.0.1:26399/", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    }
    hashEnter(vars, "plain_name", valueString("x", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    inspectEnv = countProxyNames;
    primaryMode = S_IFREG;
    runHandler(vars);
    teq(plainNames, 1, "an ordinary request name is still exported");
    for (i = 0; proxyNames[i]; i++) {
        teq(exportedCounts[i], 0, "%s selects the proxy of a CGI program's client library and is not exported",
            proxyNames[i]);
    }
    hashFree(vars);
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
