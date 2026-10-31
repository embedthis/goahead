/*
    temp-dir-empty.tst.c - An empty temporary directory setting never resolves to the working directory

    Invariant: when ME_GOAHEAD_TEMP_DIR is empty and the platform supplies no system temporary directory,
    websGetTempDir resolves nothing, websTempFile refuses to stage a file, and the startup audit reports it. Against
    the defect websAbsolutePath("") returned the working directory, and request data (CGI stdin and stdout, uploads
    with an empty ME_GOAHEAD_UPLOAD_DIR, the credential file rewrite) was staged beside the server's configuration,
    which websTempFile's own comment forbids.

    The setting is compiled into osdep.c, so this test includes src/osdep.c with ME_GOAHEAD_TEMP_DIR defined empty,
    as ability-reload.tst.c includes auth.c; the resolution runs here with that setting while the rest of the library
    is the built one. On Windows an empty setting means GetTempPath, which always resolves, so the test skips there.
 */
#define ME_GOAHEAD_TEMP_DIR ""
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/osdep.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
#if ME_WIN_LIKE && !WINCE
    tskip("Windows resolves the system temporary directory when the setting is empty");
#else
    char  cwd[ME_GOAHEAD_LIMIT_FILENAME], *path;
    cchar *dir;

    tinfo("temp-dir-empty: regression - an empty ME_GOAHEAD_TEMP_DIR must not stage files in the working directory\n");
    ttrue(getcwd(cwd, sizeof(cwd)) != 0, "the working directory is known");
    dir = websGetTempDir();
    ttrue(dir != 0, "websGetTempDir returns a value");
    ttrue(dir == 0 || !smatch(dir, cwd), "an empty setting must not resolve to the working directory %s", cwd);
    path = websTempFile(NULL, "zempty");
    ttrue(path == 0, "websTempFile must refuse to stage a file with no temporary directory, created %s",
          path ? path : "none");
    if (path) {
        unlink(path);
        wfree(path);
    }
    websOsClose();
#endif
    return 0;
}
