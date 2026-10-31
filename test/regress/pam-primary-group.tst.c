/*
    PAM abilities include the account's primary group. PAM authentication is stubbed.
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


int main(int argc, char **argv)
{
    struct passwd *pw;
    struct group  *primary;
    WebsUser      *user;
    Webs          wp;

    tinfo("pam-primary-group: a PAM account holds the ability of its primary group\n");
    logSetHandler(quiet);
    users = hashCreate(-1);
    roleTable = hashCreate(-1);

    if ((pw = getpwuid(getuid())) == 0 || pw->pw_name == 0 || (primary = getgrgid(pw->pw_gid)) == 0) {
        tskip("the test process has no account or primary group in the user database");
        return 0;
    }
    memset(&wp, 0, sizeof(wp));
    wp.username = sclone(pw->pw_name);
    wp.password = sclone(PASSWORD);
    scopy(wp.ipaddr, sizeof(wp.ipaddr), "127.0.0.1");
    ttrue(websVerifyPasswordFromPam(&wp), "A1 the account logs in");
    user = wp.user;
    ttrue(user != 0, "A2 the login creates a user record for the account");
    ttrue(user != 0 && user->abilities >= 0 && hashLookup(user->abilities, primary->gr_name) != 0,
          "A3 the account holds the ability of its primary group \"%s\"", primary->gr_name);
    wfree(wp.username);
    wfree(wp.password);
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
