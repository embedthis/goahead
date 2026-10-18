/*
    temp-dir-resolved.tst.c - Regression: the staging directory is resolved once to an absolute directory that exists, and the startup audit checks that directory.
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

static int isAbsolute(cchar *path)
{
#if ME_WIN_LIKE
    return isalpha((uchar) path[0]) && path[1] == ':';
#else
    return path[0] == '/';
#endif
}

static void suiteA_resolved(void)
{
    struct stat sbuf;
    cchar       *dir, *again;
    ssize       len;

    tinfo("\n--- Suite A: the directory is absolute, exists and is stable ---\n");
    dir = websGetTempDir();
    ttrue(dir != 0 && *dir != '\0', "A1 a temp directory is resolved");
    ttrue(isAbsolute(dir), "A2 the temp directory %s is absolute", dir);
    len = slen(dir);
    ttrue(len == 1 || (dir[len - 1] != '/' && dir[len - 1] != '\\'), "A3 no trailing separator on %s", dir);
    ttrue(stat(dir, &sbuf) == 0 && S_ISDIR(sbuf.st_mode), "A4 the temp directory exists");
    again = websGetTempDir();
    ttrue(dir == again, "A5 the directory is resolved once");
}

static void suiteB_tempFile(void)
{
    cchar *dir;
    char  *path;
    ssize len;

    tinfo("\n--- Suite B: websTempFile defaults to the resolved directory ---\n");
    dir = websGetTempDir();
    len = slen(dir);
    path = websTempFile(NULL, "zt");
    ttrue(path != 0, "B1 a temp file is created");
    if (path) {
        ttrue(strncmp(path, dir, len) == 0, "B2 %s is under %s", path, dir);
        ttrue((path[len] == '/' || path[len] == '\\') && strpbrk(&path[len + 1], "/\\") == 0,
              "B3 %s is directly under the directory", path);
        unlink(path);
        wfree(path);
    }
}


static void suiteC_audit(void)
{
    char  *saved, *spelling;
    cchar *dir;

    tinfo("\n--- Suite C: the audit reports the resolved directory ---\n");
    dir = websGetTempDir();
    saved = websGetDocuments() ? sclone(websGetDocuments()) : 0;

    websSetDocuments(dir);
    ttrue(websCheckTempDirs() > 0, "C1 serving the temp directory is reported");

    spelling = sfmt("%s/./sub/..", dir);
    websSetDocuments(spelling);
    ttrue(websCheckTempDirs() > 0, "C2 a dot-segment spelling of the temp directory is reported");
    wfree(spelling);

    spelling = sfmt("%s//", dir);
    websSetDocuments(spelling);
    ttrue(websCheckTempDirs() > 0, "C3 a repeated-separator spelling of the temp directory is reported");
    wfree(spelling);

    spelling = sfmt("%s/sub/..", dir);
    websSetDocuments(spelling);
    ttrue(websCheckTempDirs() > 0, "C4 a parent-segment spelling of the temp directory is reported");
    wfree(spelling);

    if (saved) {
        websSetDocuments(saved);
        wfreeset(saved);
    }
}

int main(int argc, char **argv)
{
    tinfo("temp-dir-resolved: regression - the staging directory is absolute, exists and is audited\n");

#if ME_WIN_LIKE
    /* Exercise the GetTempPath fallback rather than whatever the runner exports */
    _putenv("TEMP=");
    _putenv("TMP=");
#endif
    logOpen();
    suiteA_resolved();
    suiteB_tempFile();
    suiteC_audit();
    logClose();
    return 0;
}
