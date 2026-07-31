/*
    free-and-clear.tst.c - Regression: a structure field freed outside its destructor must be cleared.

    Every GoAhead structure destructor - termWebs(), freeRoute(), freeUser(), freeUploadFile() - frees
    its fields unconditionally. That is only correct while every other free site nulls what it frees.
    A PUT whose staging file could not be opened freed wp->putname on the error path and left the
    dangling pointer in the structure, so termWebs() freed the same chunk a second time. wfree() is a
    function, not a macro, so it cannot clear the caller's pointer for it.

    The durable remedy is wfreeset(), which frees and clears in one step, plus the invariant that no
    structure field is ever freed with a bare wfree(). Three suites:

      A) wfreeset() semantics - clears the field, tolerates NULL, and re-freeing is a no-op
      B) the defect shape - error-path free followed by an unconditional destructor free is safe
      C) the invariant - no bare wfree(ptr->field) remains anywhere under src/

    Suite C is the part that keeps this fixed. A future error path that reaches for wfree() on a
    structure field fails here rather than in an allocator on a customer's device.
 */

#include "testme.h"
#include "goahead.h"

/********************************* Suite A: semantics ***************************/

typedef struct Holder {
    char *name;
    char *value;
} Holder;

static void suiteA_semantics(void)
{
    Holder holder;
    char   *keep;

    tinfo("\n--- Suite A: wfreeset frees and clears ---\n");

    holder.name = sclone("staging-file");
    holder.value = 0;

    wfreeset(holder.name);
    tnull(holder.name, "A1 wfreeset must clear the field it frees");

    /*
        wfree() tolerates NULL, so the cleared field is safe to free again. This is what makes the
        unconditional destructor free correct.
     */
    wfreeset(holder.name);
    tnull(holder.name, "A2 re-freeing a cleared field must remain a no-op");

    wfreeset(holder.value);
    tnull(holder.value, "A3 wfreeset on an already NULL field must be a no-op");

    /*
        The macro is a do/while block, so it must behave as a single statement in an unbraced
        if/else. A macro that expanded to two bare statements would free unconditionally here.
     */
    holder.name = sclone("kept");
    keep = holder.name;
    if (holder.value)
        wfreeset(holder.name);
    else
        holder.value = sclone("set");
    teqp(holder.name, keep, "A4 wfreeset must not run from the untaken branch of an if/else");
    tnotnull(holder.value, "A5 the else branch must have run");
    wfreeset(holder.name);
    wfreeset(holder.value);
}

/********************************* Suite B: the defect shape ********************/

/*
    Mirror of the PUT staging path: allocate a name, fail to open the file, free the name on the
    error path, then let the request destructor free every field unconditionally.
 */
static void stageAndFail(Holder *holder)
{
    holder->name = websTempFile(NULL, "put");
    tnotnull(holder->name, "B1 websTempFile must return a name");
    /* open() failed - full disk, bad PUT dir, fd exhaustion, or a permissions error */
    wfreeset(holder->name);
}

static void destroyHolder(Holder *holder)
{
    wfreeset(holder->name);
    wfreeset(holder->value);
}

static void suiteB_defectShape(void)
{
    Holder holder;
    int    i;

    tinfo("\n--- Suite B: error-path free followed by the destructor ---\n");

    /*
        Repeat so a freed chunk is handed back out in between. Under a double free the second
        iteration frees a block that is live again, which the allocator or ASan reports.
     */
    for (i = 0; i < 32; i++) {
        memset(&holder, 0, sizeof(holder));
        stageAndFail(&holder);
        tnull(holder.name, "B2 the error path must leave no dangling field (iteration %d)", i);
        holder.value = sclone("payload");
        destroyHolder(&holder);
        tnull(holder.name, "B3 the destructor must leave the field cleared (iteration %d)", i);
    }
}

/********************************* Suite C: the invariant ***********************/

static cchar *sources[] = {
    "action.c", "alloc.c", "auth.c", "cgi.c", "crypt.c", "file.c", "fs.c", "goahead.c", "http.c",
    "js.c", "jst.c", "mbedtls.c", "openssl.c", "options.c", "osdep.c", "rom.c", "route.c",
    "runtime.c", "socket.c", "test.c", "time.c", "upload.c", 0
};

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
        fmt(path, sizeof(path), "%s/http.c", roots[i]);
        if ((fp = fopen(path, "rb")) != 0) {
            fclose(fp);
            return roots[i];
        }
    }
    return 0;
}

/*
    Read a whole file. Returns NULL if it cannot be opened.
 */
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
    Count occurrences of wfree(IDENT-> in one source file. wfreeset( and wfreeHandle( share the
    prefix, so the character after "wfree" must be the open parenthesis.
 */
static int countBareFieldFrees(cchar *dir, cchar *name, char *report, ssize reportLen)
{
    char *buf, *cp, *ip;
    int  hits;

    hits = 0;
    report[0] = '\0';
    if ((buf = readSource(dir, name)) == 0) {
        return -1;
    }
    for (cp = strstr(buf, "wfree("); cp; cp = strstr(cp + 1, "wfree(")) {
        if (cp != buf && (isalnum((uchar) cp[-1]) || cp[-1] == '_')) {
            continue;
        }
        for (ip = cp + 6; isalnum((uchar) *ip) || *ip == '_'; ip++) { }
        if (ip == cp + 6 || ip[0] != '-' || ip[1] != '>') {
            continue;
        }
        if (hits++ == 0) {
            /*
                Report the offending line. scopy fails outright when the source does not fit, so
                bound the copy at the end of the line and at the report buffer.
             */
            if ((ip = strchr(cp, '\n')) == 0) {
                ip = cp + strlen(cp);
            }
            if ((ip - cp) >= reportLen) {
                ip = cp + reportLen - 1;
            }
            sncopy(report, reportLen, cp, ip - cp);
        }
    }
    wfreeset(buf);
    return hits;
}

static void suiteC_invariant(void)
{
    cchar *dir;
    char  *report;
    int   i, hits;

    tinfo("\n--- Suite C: no bare wfree on a structure field under src/ ---\n");

    if ((dir = findSourceDir()) == 0) {
        tskip("Source tree not present beside the test directory");
        return;
    }
    tinfo("Scanning %s\n", dir);
    report = walloc(256);
    for (i = 0; sources[i]; i++) {
        hits = countBareFieldFrees(dir, sources[i], report, 256);
        ttrue(hits >= 0, "C1 %s/%s is readable", dir, sources[i]);
        ttrue(hits <= 0, "C2 %s/%s clears every structure field it frees - use wfreeset, found: %s",
              dir, sources[i], report);
    }
    wfreeset(report);
}

/********************************* Main *****************************************/

int main(int argc, char **argv)
{
    tinfo("free-and-clear: regression - a field freed outside its destructor must be cleared\n");

    suiteA_semantics();
    suiteB_defectShape();
    suiteC_invariant();

    return 0;
}
