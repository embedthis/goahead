/*
    stat-trailing-separator.tst.c - websStatFile and websOpenFile agree about a path with a trailing separator
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

static void suiteA_empty(void)
{
    WebsFileInfo info;

    tinfo("\n--- Suite A: empty path ---\n");
    ttrue(websStatFile("", &info) < 0, "an empty path must be refused");
    ttrue(websStatFile(NULL, &info) < 0, "a null path must be refused");
}


static void suiteB_file(void)
{
    WebsFileInfo info;
    int          fd;

    tinfo("\n--- Suite B: a file named with a trailing separator ---\n");
    ttrue(websStatFile("../web/index.html", &info) == 0 && !info.isDir, "the file itself is found");
    ttrue(websStatFile("../web/index.html/", &info) < 0, "a file named with a trailing '/' must not be found");
#if ME_WIN_LIKE
    ttrue(websStatFile("../web/index.html\\", &info) < 0, "a file named with a trailing '\\' must not be found");
#endif
    fd = websOpenFile("../web/index.html/", O_RDONLY | O_BINARY, 0);
    ttrue(fd < 0, "and open must refuse it too");
    if (fd >= 0) {
        websCloseFile(fd);
    }
}


static void suiteC_directory(void)
{
    WebsFileInfo info;

    tinfo("\n--- Suite C: a directory ---\n");
    ttrue(websStatFile("../web", &info) == 0 && info.isDir, "a directory is found");
    ttrue(websStatFile("../web/", &info) == 0 && info.isDir, "a directory named with a trailing '/' is found");
}


int main(int argc, char **argv)
{
    websFsOpen();
    suiteA_empty();
    suiteB_file();
    suiteC_directory();
    websFsClose();
    return 0;
}
