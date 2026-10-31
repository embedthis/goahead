/*
    ability-denial-status.tst.c - websCan distinguishes an unauthenticated request from an unauthorized one

    A request with no verified user is refused with 401, which asks it to authenticate. A verified user who lacks
    a required ability is refused with 403: authenticating again cannot help, and a 401 without a challenge is not
    a valid response (RFC 9110 15.5.2).

    DOCROOT defaults to the suite's ../web; pass -DDOCROOT=\"<abs path>\" to run from elsewhere.
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
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#ifndef DOCROOT
    #define DOCROOT "../web"
#endif

#if ME_GOAHEAD_AUTH

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif


static WebsHash abilitySet(cchar *ability)
{
    WebsHash abilities;

    abilities = hashCreate(-1);
    hashEnter(abilities, ability, valueInteger(0), 0);
    return abilities;
}


/*
    Check one ability against a request. username is the identity the request authenticated as, or NULL for an
    anonymous request; authType is the scheme the request claimed. Returns the response code, or 0 when allowed.
 */
static int denial(cchar *username, cchar *authType, cchar *ability)
{
    Webs     *wp;
    WebsHash abilities;
    int      wid, code;

    wid = websAlloc(-1);
    wp = webs[wid];
    if (username) {
        wp->username = sclone(username);
        wp->user = websLookupUser(username);
    }
    if (authType) {
        wp->authType = sclone(authType);
    }
    abilities = abilitySet(ability);
    code = websCan(wp, abilities) ? 0 : wp->code;
    hashFree(abilities);
    websFree(wp);
    return code;
}


int main(int argc, char **argv)
{
    int code;

    tinfo("ability-denial-status: 401 for no verified user, 403 for a verified user lacking the ability\n");

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen(DOCROOT, NULL), 0, "open the server");
    ttrue(websAddUser("viewer", "unused", "view") != 0, "add a user holding only view");
    websComputeAllUserAbilities();

    tinfo("\n--- Suite A: a verified user lacking the ability is forbidden ---\n");
    code = denial("viewer", "basic", "manage");
    teq(code, HTTP_CODE_FORBIDDEN, "A1 a verified user without the ability gets 403, not a challenge to log in");
    teq(denial("viewer", "basic", "view"), 0, "A2 a verified user with the ability is allowed");
    code = denial("viewer", 0, "manage");
    teq(code, HTTP_CODE_FORBIDDEN, "A3 the same through a continue gate, with no scheme on the request");

    tinfo("\n--- Suite B: a request with no verified user must authenticate ---\n");
    teq(denial(0, 0, "manage"), HTTP_CODE_UNAUTHORIZED, "B1 an anonymous request on an ability route gets 401");
    teq(denial(0, "basic", "manage"), HTTP_CODE_UNAUTHORIZED, "B2 a claimed scheme with no user gets 401");
    teq(denial("ghost", "basic", "manage"), HTTP_CODE_UNAUTHORIZED, "B3 an unknown user gets 401");

    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication is not enabled");
    return 0;
}
#endif
