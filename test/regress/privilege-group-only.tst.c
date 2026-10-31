/*
    As root, refuse group-only privilege changes before modifying credentials. Requires root.
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE && ME_GOAHEAD_LOGGING
static char lastError[512];

static void captureLog(int flags, cchar *msg)
{
    if (flags & WEBS_ERROR_MSG) {
        scopy(lastError, sizeof(lastError), msg);
    }
}
#endif


int main(int argc, char **argv)
{
#if ME_UNIX_LIKE && ME_GOAHEAD_LOGGING
    struct group *gr;
    gid_t        groups[64], before[64];
    int          count, i, same;

    if (geteuid() != 0) {
        tskip("Only root can change its group");
        return 0;
    }
    if ((gr = getgrnam("nobody")) == NULL && (gr = getgrnam("nogroup")) == NULL) {
        tskip("No nobody or nogroup group to change to");
        return 0;
    }
    logSetHandler(captureLog);
    count = getgroups((int) (sizeof(before) / sizeof(before[0])), before);

    ttrue(websDropPrivileges(NULL, gr->gr_name) < 0, "root with a group and no user must be refused");
    ttrue(strstr(lastError, "user") != NULL, "and the refusal must say that a group needs a user");
    ttrue(getgid() == 0 && getegid() == 0, "the refusal must leave the group unchanged");
    same = getgroups((int) (sizeof(groups) / sizeof(groups[0])), groups) == count;
    for (i = 0; same && i < count; i++) {
        same = groups[i] == before[i];
    }
    ttrue(same, "and the supplementary groups unchanged");
#else
    tskip("POSIX with logging only");
#endif
    return 0;
}
