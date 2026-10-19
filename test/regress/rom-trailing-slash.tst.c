/*
    A ROM document with a trailing slash does not exist, as on a disk file system. The ROM lookup strips the slash
    for directories only, so /page.jst/ cannot reach the template source that /page.jst refuses.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef ME_ROM
#define ME_ROM 1
#include "goahead.h"

static uchar page[] = "0123456789abcdefghij";
static uchar tpl[] = "<% write('x'); %>";

WebsRomIndex websRomIndex[] = {
    { "/dir", 0, 0 },
    { "/dir/index.html", page, 20 },
    { "/page.txt", page, 20 },
    { "/page.jst", tpl, 17 },
    { 0, 0, 0 }
};

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/fs.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
    WebsFileInfo info;
    int          fd;

    websFsOpen();

    fd = websOpenFile("/page.txt", O_RDONLY, 0);
    ttrue(fd >= 0, "a document opens by its name");
    websCloseFile(fd);
    teq(websStatFile("/page.txt", &info), 0, "a document stats by its name");

    fd = websOpenFile("/dir/", O_RDONLY, 0);
    ttrue(fd >= 0, "a directory opens with a trailing slash");
    websCloseFile(fd);
    teq(websStatFile("/dir/", &info), 0, "a directory stats with a trailing slash");
    ttrue(info.isDir, "a directory with a trailing slash is a directory");
    teq(websStatFile("dir", &info), 0, "a directory stats without a leading slash");

    fd = websOpenFile("/page.txt/", O_RDONLY, 0);
    teq(fd, -1, "a document with a trailing slash does not open");
    websCloseFile(fd);
    teq(websStatFile("/page.txt/", &info), -1, "a document with a trailing slash does not stat");
    teq(websStatFile("page.txt/", &info), -1, "a document with a trailing slash and no leading slash does not stat");

    fd = websOpenFile("/page.jst/", O_RDONLY, 0);
    teq(fd, -1, "a template with a trailing slash does not open, so its source cannot be served");
    websCloseFile(fd);
    teq(websStatFile("/page.jst/", &info), -1, "a template with a trailing slash does not stat");

    teq(websStatFile("/missing/", &info), -1, "a missing name with a trailing slash does not stat");
    teq(romFileMax, 0, "every descriptor is released");
    websFsClose();
    return 0;
}
