/*
    mem-failure-abort.tst.c - An allocation failure aborts after the memory notifier, even if the notifier returns
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE
static int notifyFd = -1;

static void returningNotifier(ssize size)
{
    if (write(notifyFd, "n", 1) < 0) { }
}



static int failInChild(bool notifier, bool grow, bool *notified)
{
    char    byte, *block;
    pid_t   pid;
    int     fds[2], status;

    *notified = 0;
    if (pipe(fds) < 0) {
        return -1;
    }
    if ((pid = fork()) == 0) {
        close(fds[0]);
        notifyFd = fds[1];
        websSetMemNotifier(notifier ? returningNotifier : NULL);
        signal(SIGABRT, SIG_DFL);
        if (grow) {
            block = walloc(16);
            block = wrealloc(block, MAXSSIZE / 4);
        } else {
            block = walloc(MAXSSIZE / 4);
        }
        _exit(block ? 1 : 0);
    }
    close(fds[1]);
    status = 0;
    waitpid(pid, &status, 0);
    *notified = read(fds[0], &byte, 1) == 1;
    close(fds[0]);
    return status;
}


int main(int argc, char **argv)
{
    bool    notified;
    int     status;

    status = failInChild(1, 1, &notified);
    ttrue(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "a failed wrealloc aborts when the notifier returns");
    ttrue(notified, "the notifier is told first");

    status = failInChild(1, 0, &notified);
    ttrue(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "a failed walloc aborts when the notifier returns");
    ttrue(notified, "the notifier is told first");

    status = failInChild(0, 1, &notified);
    ttrue(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "a failed wrealloc aborts with no notifier installed");

    status = failInChild(0, 0, &notified);
    ttrue(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "a failed walloc aborts with no notifier installed");
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX only");
    return 0;
}
#endif
