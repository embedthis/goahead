/* the VxWorks CGI entry symbol must not be chosen by the request. */

#include "testme.h"
#include "goahead.h"

/*
    Find src/ by walking upward from the runner's directory; return NULL if absent.
 */
static cchar *findSourceDir(void)
{
    static cchar *roots[] = { "../src", "../../src", "src", "../../../src", 0 };
    FILE         *fp;
    char         path[ME_GOAHEAD_LIMIT_FILENAME];
    int          i;

    for (i = 0; roots[i]; i++) {
        fmt(path, sizeof(path), "%s/cgi.c", roots[i]);
        if ((fp = fopen(path, "rb")) != 0) {
            fclose(fp);
            return roots[i];
        }
    }
    return 0;
}

static char *readSource(cchar *dir, cchar *name)
{
    FILE  *fp;
    char  path[ME_GOAHEAD_LIMIT_FILENAME], *buf;
    ssize len;

    fmt(path, sizeof(path), "%s/%s", dir, name);
    if ((fp = fopen(path, "rb")) == 0) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    len = (ssize) ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = walloc(len + 1);
    len = (ssize) fread(buf, 1, (size_t) len, fp);
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

int main(int argc, char **argv)
{
    cchar *dir;
    char  *buf, *start, *end;

    tinfo("cgi-vxworks-entry: regression - the VxWorks CGI entry symbol is fixed by the CGI file\n");

    if ((dir = findSourceDir()) == 0) {
        tskip("Source tree not present beside the test directory");
        return 0;
    }
    if ((buf = readSource(dir, "cgi.c")) == 0) {
        ttrue(0, "%s/cgi.c must be readable", dir);
        return 0;
    }


    start = strstr(buf, "\n#if VXWORKS\n#if _WRS_VXWORKS_MAJOR");
    tnotnull(start, "1 cgi.c must have a VxWorks launcher section");
    if (start) {
        start = strstr(start, "static CgiPid launchCgi(");
        tnotnull(start, "1a the VxWorks section must define the launcher");
    }
    if (start) {
        end = strstr(start, "static void vxWebsCgiEntry(CgiEntry entryAddr, char **argp");
        tnotnull(end, "2 the VxWorks task wrapper must follow the launcher");
        if (end) {
            *end = '\0';
            ttrue(strstr(start, "cgientry=") == 0,
                  "3 the launcher must not read the entry symbol from the CGI environment");
            ttrue(strstr(start, "= envp") == 0,
                  "4 the launcher must not scan the CGI environment");
            ttrue(strstr(start, "pEntry = sfmt(\"%s_cgientry\", basename);") != 0,
                  "5 the entry symbol must be named from the CGI basename");
        }
    }
    wfreeset(buf);
    return 0;
}
