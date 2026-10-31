/*
    write-signals-library.tst.c - websOpen ignores SIGXFSZ at its default, so a write past the process file size
    limit fails with EFBIG, keeps an application handler, and websClose restores the default
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE && defined(SIGXFSZ)
#include <sys/resource.h>

#define LIMIT 4096
#define PATH  "write-signals-library.tmp"

static void appHandler(int signo) { }

static void (*currentHandler(void))(int)
{
    struct sigaction act;

    sigaction(SIGXFSZ, NULL, &act);
    return act.sa_handler;
}


int main(int argc, char **argv)
{
    struct rlimit saved, low;
    char          buf[LIMIT];
    ssize         rc;
    int           fd;

    signal(SIGXFSZ, SIG_DFL);
    ttrue(websOpen("../web", NULL) == 0, "open the library");
    ttrue(currentHandler() == SIG_IGN, "websOpen ignores SIGXFSZ left at its default");

    unlink(PATH);
    fd = open(PATH, O_CREAT | O_EXCL | O_WRONLY, 0600);
    ttrue(fd >= 0, "create a file to write past the limit");
    getrlimit(RLIMIT_FSIZE, &saved);
    low = saved;
    low.rlim_cur = LIMIT;
    ttrue(setrlimit(RLIMIT_FSIZE, &low) == 0, "lower the file size limit to %d bytes", LIMIT);
    memset(buf, 'x', sizeof(buf));
    rc = write(fd, buf, sizeof(buf));
    ttrue(rc == LIMIT, "a write up to the limit succeeds, got %zd", rc);
    /*
        The limit is reached. Without the signal ignored, this write terminates the process before any assertion
        can report it.
     */
    rc = write(fd, buf, 1);
    ttrue(rc < 0 && errno == EFBIG, "a write past the limit fails with EFBIG and the process survives, got %zd", rc);
    setrlimit(RLIMIT_FSIZE, &saved);
    close(fd);
    unlink(PATH);

    websClose();
    ttrue(currentHandler() == SIG_DFL, "websClose restores the default");

    signal(SIGXFSZ, appHandler);
    ttrue(websOpen("../web", NULL) == 0, "reopen the library");
    ttrue(currentHandler() == appHandler, "websOpen keeps an application handler");
    websClose();
    ttrue(currentHandler() == appHandler, "websClose keeps an application handler");
    signal(SIGXFSZ, SIG_DFL);
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX only");
    return 0;
}
#endif
