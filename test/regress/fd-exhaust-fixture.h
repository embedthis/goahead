/*
    fd-exhaust-fixture.h - Exhaust this process's descriptors for C regression tests
 */

#ifndef _h_FD_EXHAUST_FIXTURE
#define _h_FD_EXHAUST_FIXTURE 1

#include <sys/resource.h>

#define EXHAUST_MAX 128

typedef struct Exhaust {
    struct rlimit saved;
    int           fds[EXHAUST_MAX];
    int           count;
} Exhaust;

static void exhaustDescriptors(Exhaust *ex)
{
    struct rlimit limit;
    int           fd;

    getrlimit(RLIMIT_NOFILE, &ex->saved);
    limit = ex->saved;
    limit.rlim_cur = 64;
    setrlimit(RLIMIT_NOFILE, &limit);
    for (ex->count = 0; ex->count < EXHAUST_MAX && (fd = dup(0)) >= 0; ex->count++) {
        ex->fds[ex->count] = fd;
    }
}


static void releaseDescriptors(Exhaust *ex)
{
    while (ex->count > 0) {
        close(ex->fds[--ex->count]);
    }
    setrlimit(RLIMIT_NOFILE, &ex->saved);
}

#endif /* _h_FD_EXHAUST_FIXTURE */
