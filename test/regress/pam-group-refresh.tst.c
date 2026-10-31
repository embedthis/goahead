/*
    Refresh group roles at each login for PAM-created users; preserve configured roles for other users.
    PAM is stubbed; the host group database supplies the test account.
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

#if ME_GOAHEAD_AUTH && ME_UNIX_LIKE && defined(__has_include)
    #if __has_include(<security/pam_appl.h>)
        #define TEST_PAM 1
    #endif
#endif

#if TEST_PAM
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 1
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <grp.h>
#include <pwd.h>

#define PASSWORD "pass1"

/*
    PAM stubs. The conversation callback supplies the password, as a PAM module would ask for it.
 */
static const struct pam_conv *stubConv;
static int                   stubHandle;

int pam_start(const char *service, const char *user, const struct pam_conv *conv, pam_handle_t **pamh)
{
    stubConv = conv;
    *pamh = (pam_handle_t*) &stubHandle;
    return PAM_SUCCESS;
}


int pam_set_item(pam_handle_t *pamh, int item, const void *value)
{
    return PAM_SUCCESS;
}


int pam_authenticate(pam_handle_t *pamh, int flags)
{
    struct pam_message       message;
    const struct pam_message *messages[1];
    struct pam_response      *responses;
    int                      ok;

    message.msg_style = PAM_PROMPT_ECHO_OFF;
    message.msg = "Password:";
    messages[0] = &message;
    responses = 0;
    if (stubConv->conv(1, messages, &responses, stubConv->appdata_ptr) != PAM_SUCCESS || responses == 0) {
        return PAM_AUTH_ERR;
    }
    ok = responses[0].resp && strcmp(responses[0].resp, PASSWORD) == 0;
    free(responses[0].resp);
    free(responses);
    return ok ? PAM_SUCCESS : PAM_AUTH_ERR;
}


int pam_acct_mgmt(pam_handle_t *pamh, int flags)
{
    return PAM_SUCCESS;
}


int pam_end(pam_handle_t *pamh, int status)
{
    return PAM_SUCCESS;
}


static void quiet(int level, cchar *msg)
{
}


static bool pamLogin(cchar *username, cchar *password, WebsUser **user)
{
    Webs wp;
    bool ok;

    memset(&wp, 0, sizeof(wp));
    wp.username = sclone(username);
    wp.password = sclone(password);
    scopy(wp.ipaddr, sizeof(wp.ipaddr), "127.0.0.1");
    ok = websVerifyPasswordFromPam(&wp);
    *user = wp.user;
    wfree(wp.username);
    wfree(wp.password);
    return ok;
}


static bool holds(WebsUser *user, cchar *ability)
{
    return user != 0 && user->abilities >= 0 && hashLookup(user->abilities, ability) != 0;
}


int main(int argc, char **argv)
{
    struct passwd *pw;
    WebsUser      *user, *again;
    cchar         *account;

    tinfo("pam-group-refresh: a PAM account's groups are read again at every login\n");
    logSetHandler(quiet);
    users = hashCreate(-1);
    roleTable = hashCreate(-1);

    if ((pw = getpwuid(getuid())) == 0 || pw->pw_name == 0) {
        tskip("the test process has no account in the user database");
        return 0;
    }
    account = pw->pw_name;

    //  A: a refused login creates nothing
    ttrue(!pamLogin(account, "wrong", &user), "A1 a wrong password is refused");
    ttrue(websLookupUser(account) == 0, "A2 a refused login creates no user record");

    //  B: the first login creates a user holding the account's groups
    ttrue(pamLogin(account, PASSWORD, &user), "B1 the right password is accepted");
    ttrue(user != 0 && websLookupUser(account) == user, "B2 the first login creates a user record for the account");
    ttrue(user != 0 && user->abilities >= 0 && hashFirst(user->abilities) != 0,
          "B3 the account's groups are its abilities");

    //  C: the account's groups change at the system: the record holds a group the account no longer has
    teqi(websSetUserRoles(account, "stalegroup"), 0, "C1 record the superseded group membership");
    ttrue(holds(user, "stalegroup"), "C2 the superseded group is held until the next login");
    ttrue(pamLogin(account, PASSWORD, &again), "C3 the account logs in again");
    ttrue(again == user, "C4 the login resolves the same user record");
    ttrue(!holds(user, "stalegroup"),
          "C5 a login reads the account's groups again, so a group the account no longer holds is withdrawn");
    ttrue(user != 0 && hashFirst(user->abilities) != 0, "C6 the account's current groups are granted again");

    //  D: a user the application defined keeps its configured roles, whatever the account's groups
    teqi(websRemoveUser(account), 0, "D1 remove the PAM-created record");
    ttrue(websAddUser(account, 0, "operator") != 0, "D2 define the account as a user with configured roles");
    websComputeAllUserAbilities();
    ttrue(pamLogin(account, PASSWORD, &user), "D3 the defined user logs in through PAM");
    ttrue(user != 0 && smatch(user->roles, "operator"), "D4 a defined user keeps its configured roles");
    ttrue(holds(user, "operator"), "D5 a defined user keeps its configured abilities");

    websCloseAuth();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("PAM is not available in this build");
    return 0;
}
#endif
