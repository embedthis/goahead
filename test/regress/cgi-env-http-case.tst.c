/*
    With no CGI variable prefix, reserve HTTP_ case variants on Windows, where environment names
    are case-insensitive.
 */
#define ME_GOAHEAD_CGI_VAR_PREFIX ""
#include "cgi-handler-fixture.h"

#if ME_GOAHEAD_CGI && !VXWORKS
static int headerCaseVariants, plainNames;

static void countHeaderVariants(char **env)
{
    char **ep;

    headerCaseVariants = plainNames = 0;
    for (ep = env; ep && *ep; ep++) {
        if (sstarts(*ep, "plain_name=")) {
            plainNames++;
        } else if (sncaselesscmp(*ep, "HTTP_ORIGIN=", 12) == 0 || sncaselesscmp(*ep, "HTTP_COOKIE=", 12) == 0) {
            headerCaseVariants++;
        }
    }
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && !VXWORKS
    WebsHash vars;

    /*
        The reservation table must cover the header namespace in every case, as it covers the other meta-variable
        prefixes: isMetaVarName folds case on Windows through envNameStarts.
     */
    ttrue(isMetaVarName("HTTP_ORIGIN"), "the HTTP_ header namespace is a reserved CGI meta-variable namespace");
    ttrue(isMetaVarName("HTTP_COOKIE"), "every HTTP_ name is reserved");
    ttrue(!isMetaVarName("plain_name"), "an ordinary name is not reserved");

    /*
        A request with no Origin or Cookie header, and parameters spelling those names in lower case.
     */
    vars = hashCreate(-1);
    hashEnter(vars, "REMOTE_USER", valueString("", VALUE_ALLOCATE), 0);
    hashEnter(vars, "AUTH_TYPE", valueString("", VALUE_ALLOCATE), 0);
    hashEnter(vars, "http_origin", valueString("http://forged.example", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    hashEnter(vars, "http_cookie", valueString("session=forged", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    hashEnter(vars, "plain_name", valueString("x", VALUE_ALLOCATE), WEBS_VAR_QUERY);
    inspectEnv = countHeaderVariants;
    primaryMode = S_IFREG;
    runHandler(vars);
    teq(plainNames, 1, "an ordinary request variable is still exported");
#if ME_WIN_LIKE
    teq(headerCaseVariants, 0, "a case variant of a header variable is not exported where names fold case");
#else
    tinfo("environment names are case-sensitive on this platform: the lower-case name cannot alias HTTP_ORIGIN");
#endif
    hashFree(vars);
#else
    tskip("CGI fixture unavailable");
#endif
    return 0;
}
