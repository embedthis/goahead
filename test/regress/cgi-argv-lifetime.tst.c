/*
    cgi-argv-lifetime.tst.c - Regression: the CGI argv backing store must outlive the spawned child.

    cgiHandler builds argv for an ISINDEX query by tokenizing a clone of the query string in place
    with stok(). stok() does not allocate: it writes NULs into the buffer and returns pointers into
    it, so every argv element past the program name is an interior pointer into that one allocation.
    On a successful spawn the handler published argp into the CGI registry and then freed the
    backing store on the very next line.

    That was safe only by accident of the spawn backend. fork/execve and CreateProcess copy the
    argument vector before launchCgi() returns, so the freed buffer is never read again. VxWorks has
    no process model: launchCgi() uses taskSpawn(), which passes argp to the new task by pointer and
    returns immediately. The task then walks argv out of a released allocation - a use-after-free in
    a single shared address space, reachable from any ISINDEX CGI request.

    The fix transfers ownership instead of freeing: the registry holds the backing store in
    Cgi.query and releases it in cgiCleanup(), where the child is known to have been reaped. The
    lifetime is then uniform across platforms rather than conditional on whether the spawn backend
    copies.

    The faulting path is VxWorks-only and cannot be exercised on a test host, so this test pins the
    invariant in two ways:

      A) the defect shape - argv interior pointers stay readable for as long as the registry record
         lives, and the record's teardown releases the backing store exactly once
      B) the source invariant - src/cgi.c transfers the backing store to the registry and never
         frees it after publishing argp

    Suite B is what keeps this fixed. A future edit that reinstates the free at the spawn site, or a
    new spawn backend that assumes the old lifetime, fails here rather than on a customer's device.
 */

#include "testme.h"
#include "goahead.h"

/********************************* Suite A: the defect shape ********************/

/*
    Mirror of the Cgi registry record: argp holds interior pointers into query, and the record owns
    both.
 */
typedef struct Slot {
    char *cgiPath;
    char *query;
    char **argp;
} Slot;

/*
    Mirror of cgiHandler's ISINDEX argv build. The caller's query is cloned, tokenized in place, and
    the resulting vector is published into the slot along with the backing store that holds it.
 */
static void buildSlot(Slot *slot, cchar *rawQuery)
{
    char *cp, *tok;
    int  n;

    memset(slot, 0, sizeof(Slot));
    slot->cgiPath = sclone("/cgi-bin/cgitest");
    slot->argp = walloc(16 * sizeof(char*));
    slot->argp[0] = slot->cgiPath;
    n = 1;
    slot->query = sclone(rawQuery);
    for (cp = stok(slot->query, " ", &tok); cp != NULL; cp = stok(NULL, " ", &tok)) {
        if (n == 1) {
            slot->argp[n++] = "--";
        }
        slot->argp[n++] = cp;
    }
    slot->argp[n] = NULL;
}

/*
    Mirror of cgiCleanup(). Every field is freed unconditionally and cleared, so the record may be
    torn down more than once.
 */
static void destroySlot(Slot *slot)
{
    wfreeset(slot->cgiPath);
    wfreeset(slot->query);
    wfreeset(slot->argp);
}

static void suiteA_defectShape(void)
{
    Slot slot;
    int  i;

    tinfo("\n--- Suite A: argv interior pointers outlive the spawn ---\n");

    /*
        Repeat so a freed chunk is handed back out in between. Reading argv through a released
        backing store returns recycled bytes, which the comparisons below catch even without a
        sanitizer; under ASan the read traps outright.
     */
    for (i = 0; i < 32; i++) {
        buildSlot(&slot, "alpha beta gamma");

        /*
            The spawn point. On the unfixed code the backing store was released here while the
            spawned task still held argp.
         */
        tnotnull(slot.query, "A1 the registry must own the argv backing store (iteration %d)", i);

        /*
            The child reads argv after the handler has returned. Every element past the marker is an
            interior pointer into slot.query.
         */
        tmatch(slot.argp[0], "/cgi-bin/cgitest", "A2 ARG[0] must be the program path (iteration %d)", i);
        tmatch(slot.argp[1], "--", "A3 ARG[1] must be the end-of-options marker (iteration %d)", i);
        tmatch(slot.argp[2], "alpha", "A4 ARG[2] must still read as the first operand (iteration %d)", i);
        tmatch(slot.argp[3], "beta", "A5 ARG[3] must still read as the second operand (iteration %d)", i);
        tmatch(slot.argp[4], "gamma", "A6 ARG[4] must still read as the third operand (iteration %d)", i);
        tnull(slot.argp[5], "A7 the vector must be null terminated (iteration %d)", i);

        /*
            argv elements are interior pointers, not separate allocations: freeing one would be a
            free of a non-chunk address. Only the backing store is owned.
         */
        teqp(slot.argp[2], slot.query, "A8 the first operand must be the head of the backing store (iteration %d)", i);
        ttrue(slot.argp[4] > slot.query && slot.argp[4] < slot.query + slen("alpha beta gamma"),
              "A9 later operands must be interior pointers into the backing store (iteration %d)", i);

        destroySlot(&slot);
        tnull(slot.query, "A10 teardown must free and clear the backing store (iteration %d)", i);

        /* The unconditional destructor must tolerate a second pass */
        destroySlot(&slot);
    }

    /*
        A query with no tokens leaves argv holding only the program path, and the backing store is
        still owned and still released.
     */
    buildSlot(&slot, "");
    tmatch(slot.argp[0], "/cgi-bin/cgitest", "A11 an empty query must still yield ARG[0]");
    tnull(slot.argp[1], "A12 an empty query must add no operands");
    destroySlot(&slot);
    tnull(slot.query, "A13 an empty query's backing store must still be released");
}

/********************************* Suite B: the source invariant ****************/

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

static void suiteB_sourceInvariant(void)
{
    cchar *dir;
    char  *buf, *handoff;

    tinfo("\n--- Suite B: cgi.c hands the backing store to the registry ---\n");

    if ((dir = findSourceDir()) == 0) {
        tskip("Source tree not present beside the test directory");
        return;
    }
    if ((buf = readSource(dir, "cgi.c")) == 0) {
        ttrue(0, "B0 %s/cgi.c must be readable", dir);
        return;
    }
    ttrue(strstr(buf, "char *query;") != 0,
          "B1 the Cgi registry record must declare the argv backing store");

    handoff = strstr(buf, "cgip->query = query;");
    tnotnull(handoff, "B2 cgiHandler must transfer the backing store to the registry on a successful spawn");

    /*
        The error paths above the handoff free query legitimately - no child exists to read argv.
        After the handoff the registry owns it, so a free anywhere below is the defect returning:
        either at the old spawn site or in a new backend that assumes the argv vector was copied.
     */
    if (handoff) {
        ttrue(strstr(handoff, "wfree(query)") == 0,
              "B3 cgiHandler must not free the backing store after publishing argp");
    }
    ttrue(strstr(buf, "wfreeset(cgip->query);") != 0,
          "B4 cgiCleanup must release the backing store once the child has been reaped");

    /*
        cgip->stdIn aliases wp->cgiStdin and is released by the Webs teardown. Converting it to
        wfreeset here would double-free against that teardown.
     */
    ttrue(strstr(buf, "wfreeset(cgip->stdIn)") == 0,
          "B5 cgiCleanup must not free the borrowed stdIn name");

    wfreeset(buf);
}

/********************************* Main *****************************************/

int main(int argc, char **argv)
{
    tinfo("cgi-argv-lifetime: regression - the argv backing store must outlive the spawned child\n");

    suiteA_defectShape();
    suiteB_sourceInvariant();

    return 0;
}
