/*
    Upload, PUT and CGI stdin staging writes use exclusive-create descriptors so path replacement cannot redirect them.
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE
#include <sys/stat.h>

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


static void descriptor(void)
{
    struct stat byPath, byFd;
    char        *path, buf[16];
    int         fd, other;
    ssize       len;

    path = websTempFileOpen(NULL, "tst", &fd);
    ttrue(path != 0, "the temp file is created");
    ttrue(fd >= 0, "its descriptor is returned");
    ttrue(stat(path, &byPath) == 0 && fstat(fd, &byFd) == 0 && byPath.st_ino == byFd.st_ino,
          "the descriptor is the file the path names");
    teqi((int) (byFd.st_mode & 0777), 0600, "the file is private");

    //  Replace the path: writes through the descriptor must not reach the replacement
    unlink(path);
    other = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
    ttrue(other >= 0, "the path is replaced");
    ttrue(write(fd, "staged", 6) == 6, "the descriptor is writable");
    len = read(other, buf, sizeof(buf));
    teqi((int) len, 0, "a write through the descriptor does not reach a replaced path");
    close(other);
    close(fd);
    unlink(path);
    wfree(path);
}


static void writers(void)
{
    cchar *dir, *files[] = { "upload.c", "http.c", "cgi.c", 0 };
    cchar *reopens[] = { "open(wp->uploadTmp", "open(wp->putname", "open(wp->cgiStdin", 0 };
    char  *src;
    int   i, j;

    if ((dir = findSourceDir()) == 0) {
        tinfo("source tree not present, writer check skipped");
        return;
    }
    for (i = 0; files[i]; i++) {
        src = readSource(dir, files[i]);
        ttrue(src != 0, "%s is readable", files[i]);
        for (j = 0; src && reopens[j]; j++) {
            ttrue(strstr(src, reopens[j]) == 0, "%s must not reopen a staging file: %s", files[i], reopens[j]);
        }
        wfree(src);
    }
}
#endif


int main(int argc, char **argv)
{
#if ME_UNIX_LIKE
    descriptor();
    writers();
#else
    tskip("POSIX only");
#endif
    return 0;
}
