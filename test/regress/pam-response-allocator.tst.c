/*
    pam-response-allocator.tst.c - Regression: PAM response strings must be C library allocations.

    pamChat() (src/auth.c) is the conversation callback libpam invokes to collect the username and
    password. Once it returns PAM_SUCCESS the response array and every string in it belong to libpam,
    which releases them with the C library's free(). That makes the allocator a contract, not a
    preference: GoAhead must hand over memory free() can take back.

    pamChat filled those strings with sclone(), which allocates through walloc(). That is only correct
    while walloc is malloc. With ME_GOAHEAD_REPLACE_MALLOC=1 it is not - walloc carves blocks out of
    its own arena and returns a pointer past a WebsAlloc header (src/alloc.c), so free() is handed an
    interior pointer, or one that was never on the C heap at all. ME_GOAHEAD_REPLACE_MALLOC and
    ME_GOAHEAD_AUTH_STORE="pam" are independent options, so that build is supported and every PAM
    authentication corrupted the heap. Appweb's copy of this function has always used strdup().

    The suite is a source scan rather than a call: pamChat is static, and the test suite builds with
    the "file" password store, so ME_COMPILER_HAS_PAM is 0 and the function is not even compiled in.
    A scan still pins the invariant - it is exactly the regression that occurred, a strdup modernised
    to sclone without noticing that this allocation is freed by a third party.

    Two suites:
      A) every assignment into a pam_response .resp field uses strdup
      B) the scan is not vacuous - the field and the allocator are both actually present
 */

#include "testme.h"
#include "goahead.h"

#define REPORT_LEN 160

/*
    The test binary's working directory depends on the runner, so locate src/ by probing upwards.
    Returns NULL when the source tree is not present beside the tests.
 */
static cchar *findSourceDir(void)
{
    static cchar *roots[] = { "../src", "../../src", "src", "../../../src", 0 };
    FILE         *fp;
    char         path[ME_GOAHEAD_LIMIT_FILENAME];
    int          i;

    for (i = 0; roots[i]; i++) {
        fmt(path, sizeof(path), "%s/auth.c", roots[i]);
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


/*
    Copy the source line containing cp into report, bounded at the newline and at the buffer.
 */
static void reportLine(cchar *buf, cchar *cp, char *report, ssize reportLen)
{
    cchar *ip, *start;

    for (start = cp; start > buf && start[-1] != '\n'; start--) { }
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if ((ip = strchr(start, '\n')) == 0) {
        ip = start + slen(start);
    }
    if ((ip - start) >= reportLen) {
        ip = start + reportLen - 1;
    }
    sncopy(report, reportLen, start, ip - start);
}


/*
    Find the next assignment into a ".resp" field. Returns a pointer to the '.' or NULL. Skips
    ".resp_retcode", which shares the prefix, by requiring the character after "resp" to end the
    identifier.
 */
static cchar *nextRespAssign(cchar *cp)
{
    cchar *ip;

    for (; (cp = strstr(cp, ".resp")) != 0; cp++) {
        ip = cp + 5;
        if (isalnum((uchar) *ip) || *ip == '_') {
            /* .resp_retcode or similar */
            continue;
        }
        while (*ip == ' ' || *ip == '\t') {
            ip++;
        }
        if (*ip != '=' || ip[1] == '=') {
            /* A read, or a comparison, not an assignment */
            continue;
        }
        return cp;
    }
    return 0;
}


/*
    Count .resp assignments whose right hand side is neither strdup() nor a null initialiser.
 */
static int countForeignRespAllocs(cchar *buf, int *total, char *report, ssize reportLen)
{
    cchar *cp, *ip;
    int   hits;

    hits = 0;
    *total = 0;
    report[0] = '\0';

    for (cp = nextRespAssign(buf); cp; cp = nextRespAssign(cp + 1)) {
        (*total)++;
        ip = strchr(cp, '=') + 1;
        while (*ip == ' ' || *ip == '\t') {
            ip++;
        }
        if (sncmp(ip, "strdup(", 7) == 0) {
            continue;
        }
        if (sncmp(ip, "0;", 2) == 0 || sncmp(ip, "NULL;", 5) == 0) {
            /* Clearing the field, not allocating into it */
            continue;
        }
        if (hits++ == 0) {
            reportLine(buf, cp, report, reportLen);
        }
    }
    return hits;
}


static void suiteA_allocator(void)
{
    cchar *dir;
    char  *buf, *report;
    int   hits, total;

    tinfo("\n--- Suite A: pam_response strings must be strdup allocations ---\n");

    if ((dir = findSourceDir()) == 0) {
        tskip("Source tree not present beside the test directory");
        return;
    }
    if ((buf = readSource(dir, "auth.c")) == 0) {
        tfail("A1 cannot read %s/auth.c", dir);
        return;
    }
    report = walloc(REPORT_LEN);
    hits = countForeignRespAllocs(buf, &total, report, REPORT_LEN);

    ttrue(hits == 0,
          "A2 every pam_response .resp assignment must use strdup - libpam frees these with free(), "
          "and walloc memory (sclone) is not free()-able under ME_GOAHEAD_REPLACE_MALLOC. Found: %s",
          report);

    wfreeset(report);
    wfreeset(buf);
}


/*
    A scan that matches nothing passes for the wrong reason. Assert the shape it is looking for is
    still in the source, so a rename or a removal fails here rather than silently ending coverage.
 */
static void suiteB_notVacuous(void)
{
    cchar *dir;
    char  *buf;
    int   hits, total;
    char  report[REPORT_LEN];

    tinfo("\n--- Suite B: the scan still has something to scan ---\n");

    if ((dir = findSourceDir()) == 0) {
        tskip("Source tree not present beside the test directory");
        return;
    }
    if ((buf = readSource(dir, "auth.c")) == 0) {
        tfail("B1 cannot read %s/auth.c", dir);
        return;
    }
    ttrue(strstr(buf, "pamChat") != 0, "B2 pamChat must still exist in auth.c");

    hits = countForeignRespAllocs(buf, &total, report, sizeof(report));
    ttrue(total >= 3, "B3 expected the .resp initialiser and both prompt assignments, saw %d", total);
    ttrue(hits >= 0, "B4 scan completed");
    ttrue(strstr(buf, "strdup(") != 0, "B5 auth.c must still allocate PAM responses with strdup");

    wfreeset(buf);
}


int main(int argc, char **argv)
{
    tinfo("pam-response-allocator: regression - memory handed to libpam must be free()-able\n");

    suiteA_allocator();
    suiteB_notVacuous();

    return 0;
}
