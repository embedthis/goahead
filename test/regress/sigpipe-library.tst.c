/*
    sigpipe-library.tst.c - websOpen ignores SIGPIPE at its default, keeps an application handler, and websClose
    restores the default
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE && defined(SIGPIPE)
static void appHandler(int signo) { }

static void (*currentHandler(void))(int)
{
    struct sigaction act;

    sigaction(SIGPIPE, NULL, &act);
    return act.sa_handler;
}


int main(int argc, char **argv)
{
    int     pair[2];
    ssize   rc;

    signal(SIGPIPE, SIG_DFL);
    ttrue(websOpen("../web", NULL) == 0, "open the library");
    ttrue(currentHandler() == SIG_IGN, "websOpen ignores SIGPIPE left at its default");

    ttrue(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0, "create a socket pair");
    close(pair[1]);
    rc = send(pair[0], "x", 1, 0);
    ttrue(rc < 0 && errno == EPIPE, "a write to a closed peer fails with EPIPE and the process survives");
    close(pair[0]);

    websClose();
    ttrue(currentHandler() == SIG_DFL, "websClose restores the default");

    signal(SIGPIPE, appHandler);
    ttrue(websOpen("../web", NULL) == 0, "reopen the library");
    ttrue(currentHandler() == appHandler, "websOpen keeps an application handler");
    websClose();
    ttrue(currentHandler() == appHandler, "websClose keeps an application handler");
    signal(SIGPIPE, SIG_DFL);
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX only");
    return 0;
}
#endif
