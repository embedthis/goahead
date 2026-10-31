/*
    abspath-deep-cwd.tst.c - websAbsolutePath resolves a relative directory from a deep working directory

    websAbsolutePath joins a relative path to the working directory so that the staging-directory audit
    (websCheckTempDirs) and the route-region comparison judge directories by one absolute spelling. It read the
    working directory into a fixed ME_GOAHEAD_LIMIT_FILENAME buffer and, when that was too small, returned the
    relative path unchanged. A relative spelling never compares as inside an absolute one, so from a working
    directory longer than the buffer the audit let a staging directory inside the documents directory pass.

    Runs from a scratch directory deeper than ME_GOAHEAD_LIMIT_FILENAME, built with relative mkdir steps.
 */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_UNIX_LIKE
#include <sys/stat.h>

#define BASE    "abspath-deep-cwd.dir"
#define LEVELS  3
#define SEGMENT "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"

/*
    Descend LEVELS directories of 100 characters each, so the working directory exceeds
    ME_GOAHEAD_LIMIT_FILENAME (256) while staying well inside PATH_MAX.
 */
static bool descend(void)
{
    int i;

    if (mkdir(BASE, 0755) < 0 && errno != EEXIST) {
        return 0;
    }
    if (chdir(BASE) < 0) {
        return 0;
    }
    for (i = 0; i < LEVELS; i++) {
        if ((mkdir(SEGMENT, 0755) < 0 && errno != EEXIST) || chdir(SEGMENT) < 0) {
            return 0;
        }
    }
    return 1;
}


static void ascendAndRemove(cchar *start)
{
    int i;

    for (i = 0; i < LEVELS; i++) {
        chdir("..");
        rmdir(SEGMENT);
    }
    chdir("..");
    rmdir(BASE);
    chdir(start);
}


static void testDeepWorkingDirectory(void)
{
    char  start[4096], cwd[4096], *path, *saved;

    ttrue(getcwd(start, sizeof(start)) != NULL, "the test must know its starting directory");
    ttrue(descend(), "the deep working directory must be created: errno %d", errno);
    ttrue(getcwd(cwd, sizeof(cwd)) != NULL && slen(cwd) > ME_GOAHEAD_LIMIT_FILENAME,
          "the working directory must be longer than ME_GOAHEAD_LIMIT_FILENAME, got %d", (int) slen(cwd));

    /*
        A relative directory is still resolved to its absolute spelling from a deep working directory.
     */
    path = websAbsolutePath("tmp");
    ttrue(path[0] == '/', "A1 a relative directory must resolve to an absolute path, got \"%s\"", path);
    ttrue(sstarts(path, cwd), "A2 the absolute path must start with the working directory, got \"%s\"", path);
    wfree(path);

    /*
        SI-3: the audit compares the documents directory with the staging directories by their absolute spellings.
        With the documents directory "." the compiled-in upload directory (ME_GOAHEAD_UPLOAD_DIR, "tmp") and the
        PUT directory (ME_GOAHEAD_PUT_DIR, ".") lie inside it, and the audit must report that from any working
        directory.
     */
    saved = websGetDocuments() ? sclone(websGetDocuments()) : 0;
    websSetDocuments(".");
    ttrue(websCheckTempDirs() > 0, "A3 a staging directory inside the documents directory must be reported from "
          "a deep working directory");
    if (saved) {
        websSetDocuments(saved);
        wfreeset(saved);
    }
    ascendAndRemove(start);
}


int main(int argc, char **argv)
{
    testDeepWorkingDirectory();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Unix-like working directories only");
    return 0;
}
#endif
