/*
    A failed cross-device PUT copy preserves the target and leaves no partial file.
    Simulate EXDEV and write failures.
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

#if ME_UNIX_LIKE && !ME_ROM
#include <dirent.h>

static cchar *stagedPath;
static int   failWrite;

//  The staged body is always on another device; any other rename is real
static int crossRename(cchar *from, cchar *to)
{
    if (smatch(from, stagedPath)) {
        errno = EXDEV;
        return -1;
    }
    return rename(from, to);
}

static ssize failingWrite(int fd, const void *buf, size_t len)
{
    if (failWrite) {
        errno = ENOSPC;
        return -1;
    }
    return write(fd, buf, len);
}

#define rename crossRename
#define write failingWrite
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../src/file.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef rename
#undef write


static void writeFile(cchar *path, cchar *text)
{
    int fd;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    ttrue(fd >= 0 && write(fd, text, slen(text)) == (ssize) slen(text), "write %s", path);
    close(fd);
}


static char *readFile(cchar *path)
{
    char  buf[64];
    ssize len;
    int   fd;

    if ((fd = open(path, O_RDONLY)) < 0) {
        return sclone("");
    }
    len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    buf[len > 0 ? len : 0] = '\0';
    return sclone(buf);
}


static int countEntries(cchar *dir)
{
    DIR           *dp;
    struct dirent *ep;
    int           count;

    count = 0;
    if ((dp = opendir(dir)) != 0) {
        while ((ep = readdir(dp)) != 0) {
            if (ep->d_name[0] != '.') {
                count++;
            }
        }
        closedir(dp);
    }
    return count;
}


static int put(cchar *dir, cchar *staged, cchar *target, cchar *body)
{
    Webs wp;
    int  rc;

    writeFile(staged, body);
    memset(&wp, 0, sizeof(wp));
    wp.putname = sclone(staged);
    wp.filename = sclone(target);
    wp.putfd = open(staged, O_RDONLY);
    stagedPath = staged;
    rc = commitPut(&wp);
    wfree(wp.putname);
    wfree(wp.filename);
    return rc;
}


int main(int argc, char **argv)
{
    char base[ME_GOAHEAD_LIMIT_FILENAME], staging[ME_GOAHEAD_LIMIT_FILENAME], docs[ME_GOAHEAD_LIMIT_FILENAME];
    char staged[ME_GOAHEAD_LIMIT_FILENAME], target[ME_GOAHEAD_LIMIT_FILENAME], *content;

    fmt(base, sizeof(base), "%s/goahead-put-xdev-%d", websGetTempDir(), (int) getpid());
    fmt(staging, sizeof(staging), "%s/staging", base);
    fmt(docs, sizeof(docs), "%s/docs", base);
    fmt(staged, sizeof(staged), "%s/put.tmp", staging);
    fmt(target, sizeof(target), "%s/page.txt", docs);
    mkdir(base, 0700);
    mkdir(staging, 0700);
    mkdir(docs, 0700);
    writeFile(target, "old");

    failWrite = 1;
    ttrue(put(base, staged, target, "new") < 0, "a failed cross-device copy fails the PUT");
    content = readFile(target);
    tmatch(content, "old", "a failed copy leaves the previous content");
    wfree(content);
    teqi(countEntries(docs), 1, "a failed copy leaves no partial file beside the target");

    failWrite = 0;
    ttrue(put(base, staged, target, "new") == 0, "a cross-device copy completes the PUT");
    content = readFile(target);
    tmatch(content, "new", "the copy replaces the target");
    wfree(content);
    teqi(countEntries(docs), 1, "the copy leaves only the target");
    teqi(countEntries(staging), 0, "the staged file is removed");

    unlink(target);
    unlink(staged);
    rmdir(staging);
    rmdir(docs);
    rmdir(base);
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX with PUT staging only");
    return 0;
}
#endif
