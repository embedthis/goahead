/*
    privilege-drop.tst.c - websDropPrivileges runs the server as the requested user and group, or fails
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE
static void suiteA_nothing(void)
{
    uid_t uid;

    tinfo("\n--- Suite A: nothing requested ---\n");
    uid = geteuid();
    ttrue(websDropPrivileges(NULL, NULL) == 0, "no user or group must succeed");
    ttrue(websDropPrivileges("", "") == 0, "an empty user and group must succeed");
    ttrue(geteuid() == uid, "and must not change the user");
}


static void suiteB_unknown(void)
{
    tinfo("\n--- Suite B: unknown accounts ---\n");
    ttrue(websDropPrivileges("goahead-no-such-user", NULL) < 0, "an unknown user must be refused");
    ttrue(websDropPrivileges(NULL, "goahead-no-such-group") < 0, "an unknown group must be refused");
}


static void suiteC_unprivileged(void)
{
    struct passwd *pw, *other;
    struct group  *gr;
    char          user[ME_GOAHEAD_LIMIT_STRING], group[ME_GOAHEAD_LIMIT_STRING];

    tinfo("\n--- Suite C: not root ---\n");
    pw = getpwuid(geteuid());
    gr = getgrgid(getegid());
    ttrue(pw && gr, "the current user and group must have names");
    scopy(user, sizeof(user), pw->pw_name);
    scopy(group, sizeof(group), gr->gr_name);

    ttrue(websDropPrivileges(user, group) == 0, "the current user and group must be accepted");
    ttrue(websDropPrivileges(NULL, group) == 0, "the current group alone must be accepted");

    other = getpwnam("root");
    if (other) {
        ttrue(websDropPrivileges("root", NULL) < 0, "another user must be refused when not root");
    }
}


static void suiteD_root(void)
{
    struct passwd *pw;
    gid_t         groups[64];
    uid_t         uid;
    gid_t         gid;
    int           i, count;

    tinfo("\n--- Suite D: root ---\n");
    if ((pw = getpwnam("nobody")) == NULL) {
        tinfo("No nobody account to drop to");
        return;
    }
    uid = pw->pw_uid;
    gid = pw->pw_gid;
    ttrue(websDropPrivileges("nobody", NULL) == 0, "root must be able to drop to nobody");
    ttrue(getuid() == uid && geteuid() == uid, "the real and effective user must be nobody");
    ttrue(getgid() == gid && getegid() == gid, "the real and effective group must be nobody's group");
    ttrue(setuid(0) < 0 && seteuid(0) < 0, "root must not be recoverable");
    count = getgroups(sizeof(groups) / sizeof(groups[0]), groups);
    for (i = 0; i < count; i++) {
        ttrue(groups[i] != 0 || gid == 0, "root's supplementary group must be gone");
    }
}
#endif


int main(int argc, char **argv)
{
#if ME_UNIX_LIKE
    suiteA_nothing();
    suiteB_unknown();
    if (geteuid() == 0) {
        suiteD_root();
    } else {
        suiteC_unprivileged();
    }
#else
    ttrue(websDropPrivileges(NULL, NULL) == 0, "no user or group must succeed");
    ttrue(websDropPrivileges("nobody", NULL) < 0, "a change must be refused where it is not supported");
#endif
    return 0;
}
