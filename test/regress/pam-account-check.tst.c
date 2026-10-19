/*
    PAM authentication must check the account status as well as the password and set PAM_RHOST.
    Stub PAM calls to accept passwords but refuse the expired account.
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

#if ME_GOAHEAD_AUTH && ME_COMPILER_HAS_PAM

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static struct pam_conv savedConv;
static char            handle;
static char            *startedUser;
static char            *rhost;
static int             authenticated;
static int             accountChecks;
static int             ended;

int pam_start(const char *service, const char *user, const struct pam_conv *conv, pam_handle_t **pamh)
{
    wfreeset(startedUser);
    startedUser = sclone(user);
    savedConv = *conv;
    *pamh = (pam_handle_t*) &handle;
    return PAM_SUCCESS;
}


int pam_set_item(pam_handle_t *pamh, int item, const void *value)
{
    if (item == PAM_RHOST) {
        wfreeset(rhost);
        rhost = sclone((cchar*) value);
    }
    return PAM_SUCCESS;
}


int pam_authenticate(pam_handle_t *pamh, int flags)
{
    struct pam_message  msg;
    const struct pam_message *msgs[1];
    struct pam_response *resp;
    int                 rc;

    msg.msg_style = PAM_PROMPT_ECHO_OFF;
    msg.msg = "Password:";
    msgs[0] = &msg;
    resp = 0;
    rc = savedConv.conv(1, msgs, &resp, savedConv.appdata_ptr);
    if (rc == PAM_SUCCESS && resp) {
        free(resp[0].resp);
        free(resp);
    }
    authenticated++;
    return rc == PAM_SUCCESS ? PAM_SUCCESS : PAM_AUTH_ERR;
}


int pam_acct_mgmt(pam_handle_t *pamh, int flags)
{
    accountChecks++;
    if (smatch(startedUser, "expired")) {
        return PAM_ACCT_EXPIRED;
    }
    if (smatch(startedUser, "mustchange")) {
        return PAM_NEW_AUTHTOK_REQD;
    }
    return PAM_SUCCESS;
}


int pam_end(pam_handle_t *pamh, int status)
{
    ended++;
    return PAM_SUCCESS;
}


static bool verify(Webs *wp, cchar *username)
{
    wfreeset(wp->username);
    wfreeset(wp->password);
    wp->username = sclone(username);
    wp->password = sclone("secret");
    wp->encoded = 0;
    wp->user = 0;
    wp->authReason = 0;
    return websVerifyPasswordFromPam(wp);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_AUTH && ME_COMPILER_HAS_PAM
    Webs *wp;
    int  before;

    teq(websOpenAuth(1), 0, "open the user table");
    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "192.0.2.77");

    ttrue(verify(wp, "active"), "an active account with the right password authenticates");
    ttrue(authenticated == 1, "the password was checked once");

    before = ended;
    ttrue(!verify(wp, "expired"), "an expired account with the right password is refused");
    ttrue(accountChecks == 2, "the account was checked after the password, for both accounts");
    ttrue(wp->authReason != 0 && !smatch(wp->authReason, "password does not match"),
          "the refusal is reported as an account refusal, not a password mismatch");
    ttrue(ended == before + 1, "the PAM transaction is ended after the refusal");
    tmatch(rhost, "192.0.2.77", "PAM is told the client address through PAM_RHOST");

    ttrue(!verify(wp, "mustchange"), "an account whose password must be changed is refused");

    ttrue(verify(wp, "active"), "the active account still authenticates after the refusals");

    wfree(wp->username);
    wfree(wp->password);
    wfree(wp);
    websCloseAuth();
#else
    tskip("PAM password store is not compiled in");
#endif
    return 0;
}
