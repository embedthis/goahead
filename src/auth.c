/*
    auth.c -- Authorization Management

    This modules supports a user/role/ability based authorization scheme.

    In this scheme Users have passwords and can have multiple roles. A role is associated with the ability to do
    things like "admin" or "user" or "support". A role may have abilities (which are typically verbs) like "add",
    "shutdown".

    When the web server starts up, it loads a route and authentication configuration file that specifies the Users,
    Roles and Routes.  Routes specify the required abilities to access URLs by specifying the URL prefix. Once logged
    in, the user's abilities are tested against the route abilities. When the web server receivess a request, the set of
    Routes is consulted to select the best route. If the routes requires abilities, the user must be logged in and
    authenticated.

    Three authentication backend protocols are supported:
        HTTP basic authentication which uses browser dialogs and clear text passwords (insecure unless over TLS)
        HTTP digest authentication which uses browser dialogs
        Web form authentication which uses a web page form to login (insecure unless over TLS)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

#if ME_GOAHEAD_AUTH

#if ME_COMPILER_HAS_PAM
    #include <security/pam_appl.h>
#endif

/*********************************** Locals ***********************************/

static WebsHash   users = -1;
static WebsHash   roleTable = -1;
static char       *masterSecret;
static int        autoLogin = ME_GOAHEAD_AUTO_LOGIN;
static WebsVerify verifyPassword;
#if ME_GOAHEAD_DIGEST
static WebsHash nonceCounts = -1;               /* Digest nonce -> issue time and last accepted nonce count */
static WebsTime nonceFloor;                     /* Nonces issued at or before this time were evicted */
static int      nonceTotal;                     /* Records in nonceCounts */
static int      noncePruneId = -1;
#endif
static bool pmatch(cchar *s1, cchar *s2);

#define WEBS_MAX_AUTH_LOG_USER 64               /* Longest claimed user name written to the log */

#if ME_COMPILER_HAS_PAM
typedef struct {
    char *name;
    char *password;
} UserInfo;

#if MACOSX
typedef int Gid;
#else
typedef gid_t Gid;
#endif
#endif

/********************************** Forwards **********************************/

static void computeAbilities(WebsHash abilities, cchar *role, int depth);
static WebsVerify getVerify(WebsRoute *route);
static void computeUserAbilities(WebsUser *user);
static WebsUser *createUser(cchar *username, cchar *password, cchar *roles);
static void freeRole(WebsRole *rp);
static void freeUser(WebsUser *up);
static bool isBlowfishHash(cchar *password);
static bool isMd5Hash(cchar *password);
static bool isCompleteHash(cchar *hash);
static void verifyUnknownUser(Webs *wp);
static void logoutServiceProc(Webs *wp);
static void loginServiceProc(Webs *wp);
static void reportPasswordBudget(cchar *username, cchar *password);
static void setUsername(Webs *wp, cchar *username);

#if ME_GOAHEAD_DIGEST
static bool checkNonceCount(Webs *wp);
static char *formatDigestNonce(WebsTime when, uint64 counter);
static uint64 parseHex64(cchar *str);
static void pruneNonces(void *data, int id);
static char *calcDigest(Webs *wp, char *username, char *password);
static char *createDigestNonce(Webs *wp);
static char *parseDigestNonce(char *nonce, char **secret, WebsTime *when, char **counterStr);
#endif

#if ME_COMPILER_HAS_PAM
static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data);
#endif

/************************************ Code ************************************/
/*
    Resolve the user again when the username changes to avoid reusing another user's record.
 */
static void setUsername(Webs *wp, cchar *username)
{
    char *name;

    name = sclone(username);
    wfreeset(wp->username);
    wp->username = name;
    wp->user = 0;
}


#if ME_GOAHEAD_LOGGING
/*
    Log one authentication outcome. The user name is request data: it is truncated here and escaped by the log.
 */
static void logAuth(Webs *wp, int level, cchar *scheme, cchar *outcome, cchar *reason)
{
    char *name;

    name = snclone(wp->username ? wp->username : "-", WEBS_MAX_AUTH_LOG_USER);
    logmsg(level, "Authentication %s for user \"%s\" from %s on %s (%s)%s%s", outcome, name, wp->ipaddr,
           (wp->route && wp->route->prefix) ? wp->route->prefix : "-", scheme, reason ? ": " : "",
           reason ? reason : "");
    wfree(name);
}


PUBLIC void websLogAuth(Webs *wp, int level, cchar *outcome, cchar *reason)
{
    logAuth(wp, level, (wp->route && wp->route->authType) ? wp->route->authType : "none", outcome, reason);
}
#else
    #define logAuth(wp, level, scheme, outcome, reason) if (1); else { }
#endif


PUBLIC bool websAuthenticate(Webs *wp)
{
    WebsRoute *route;
    char      *username;
    int       cached;

    assert(wp);
    assert(wp->route);
    route = wp->route;

    if (!route || !route->authType || autoLogin) {
        /* Authentication not required */
        return 1;
    }
    cached = 0;
    if (wp->cookie && websGetSession(wp, 0) != 0) {
        /*
            Reuse sessions only when their authentication protocol matches the route.
         */
        if ((username = (char*) websGetSessionVar(wp, WEBS_SESSION_USERNAME, 0)) != 0 &&
            smatch(websGetSessionVar(wp, WEBS_SESSION_AUTHTYPE, 0), route->authType)) {
            cached = 1;
            setUsername(wp, username);
        }
    }
    if (!cached) {
        if (wp->authType && !smatch(wp->authType, route->authType)) {
            logAuth(wp, 0, route->authType, "failed", "wrong authentication scheme");
            websError(wp, HTTP_CODE_UNAUTHORIZED, "Access denied. Wrong authentication protocol type.");
            return 0;
        }
        /*
            Do not verify unchanged credentials again on reroute; Digest counts are accepted once.
         */
        if (wp->flags & WEBS_AUTHENTICATED) {
            return 1;
        }
        if (wp->authDetails && route->parseAuth) {
            wp->authReason = 0;
            if (!(route->parseAuth)(wp) || !wp->username || !*wp->username) {
                logAuth(wp, 0, route->authType, "failed", wp->authReason ? wp->authReason : "invalid credentials");
                /*
                    parseAuth may have taken ownership of a cloned username before failing.
                    Free it here so a failed authentication does not orphan the block.
                 */
                wfreeset(wp->username);
            }
        }
        if (!wp->username || !*wp->username) {
            if (route->askLogin) {
                (route->askLogin)(wp);
            }
            websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
            return 0;
        }
        wp->authReason = 0;
        if (!getVerify(route) || !(getVerify(route))(wp)
#if ME_GOAHEAD_DIGEST
            || (smatch(route->authType, "digest") && !checkNonceCount(wp))
#endif
            ) {
            logAuth(wp, 0, route->authType, "failed", wp->authReason ? wp->authReason : "invalid credentials");
            if (route->askLogin) {
                (route->askLogin)(wp);
            }
            websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
            return 0;
        }
        wp->flags |= WEBS_AUTHENTICATED;
        logAuth(wp, 2, route->authType, "succeeded", 0);
        /*
            Cache verified credentials only in an existing client session, rotating its ID first. Authentication
            still succeeds if rotation fails; creating sessions here could exhaust the table.
         */
        if (wp->session) {
            websLoginSession(wp, route->authType);
        }
    }
    //  Name the protocol that authenticated the request, not the one the client claimed
    wfreeset(wp->authType);
    wp->authType = sclone(route->authType);
    return 1;
}


PUBLIC int websOpenAuth(int minimal)
{
    char sbuf[32];

    assert(minimal == 0 || minimal == 1);

    if ((users = hashCreate(-1)) < 0) {
        return -1;
    }
    if ((roleTable = hashCreate(-1)) < 0) {
        return -1;
    }
#if ME_GOAHEAD_DIGEST
    if ((nonceCounts = hashCreate(-1)) < 0) {
        return -1;
    }
    nonceFloor = 0;
    nonceTotal = 0;
    if (!minimal) {
        noncePruneId = websStartEvent(WEBS_SESSION_PRUNE, pruneNonces, 0);
    }
#endif
    if (!minimal) {
        /*
            Require random bytes before issuing Digest nonces.
         */
        if (websGetRandomBytes(sbuf, sizeof(sbuf), 0) < 0) {
            error("Cannot get random bytes for the authentication secret");
            return -1;
        }
        masterSecret = websEncode64Block(sbuf, sizeof(sbuf));
        websDefineNativeAction("login", loginServiceProc);
        websDefineNativeAction("logout", logoutServiceProc);
    }
    /* Preserve an application-installed verifier. */
    if (!verifyPassword) {
        if (smatch(ME_GOAHEAD_AUTH_STORE, "file")) {
            verifyPassword = websVerifyPasswordFromFile;
#if ME_COMPILER_HAS_PAM
        } else if (smatch(ME_GOAHEAD_AUTH_STORE, "pam")) {
            verifyPassword = websVerifyPasswordFromPam;
#endif
        } else {
            /* Refuse an unavailable configured store; falling back would use different credentials. */
            error("Password store \"%s\" is not available in this build. Rebuild with support for it, "
                  "or set ME_GOAHEAD_AUTH_STORE to \"file\".", ME_GOAHEAD_AUTH_STORE);
            return -1;
        }
    }
    /*
        Auto-login serves every authenticated route without credentials. It is a development aid, so
        it is not fatal, but it must never be silent: use error() so the warning appears at the
        default log level.
     */
    if (autoLogin) {
        error("Security warning: ME_GOAHEAD_AUTO_LOGIN is enabled. Routes that require "
              "authentication are served to unauthenticated clients. Do not deploy this build.");
    }
    return 0;
}


PUBLIC void websCloseAuth(void)
{
    WebsKey *key, *next;

    wfreeset(masterSecret);
    if (users >= 0) {
        for (key = hashFirst(users); key; key = next) {
            next = hashNext(users, key);
            freeUser(key->content.value.symbol);
        }
        hashFree(users);
        users = -1;
    }
    if (roleTable >= 0) {
        for (key = hashFirst(roleTable); key; key = next) {
            next = hashNext(roleTable, key);
            freeRole(key->content.value.symbol);
        }
        hashFree(roleTable);
        roleTable = -1;
    }
#if ME_GOAHEAD_DIGEST
    if (noncePruneId >= 0) {
        websStopEvent(noncePruneId);
        noncePruneId = -1;
    }
    if (nonceCounts >= 0) {
        hashFree(nonceCounts);
        nonceCounts = -1;
    }
#endif
    verifyPassword = 0;
}


static WebsUser *createUser(cchar *username, cchar *password, cchar *roles)
{
    WebsUser *user;

    assert(username);

    if ((user = walloc(sizeof(WebsUser))) == 0) {
        return 0;
    }
    user->name = sclone(username);
    user->roles = sclone(roles);
    user->password = sclone(password);
    user->abilities = -1;
    return user;
}


/*
    True when the stored hash is a Blowfish password hash, in the current BF2 format or the legacy
    BF1 format.
 */
static bool isBlowfishHash(cchar *password)
{
    return sstarts(password, WEBS_CRYPT_BLOWFISH ":") || sstarts(password, WEBS_CRYPT_BLOWFISH2 ":");
}


static bool isMd5Hash(cchar *password)
{
    cchar *cp;

    if (slen(password) != 32) {
        return 0;
    }
    for (cp = password; *cp; cp++) {
        if (!isxdigit((uchar) *cp)) {
            return 0;
        }
    }
    return 1;
}


/*
    Report legacy BF1 accounts whose username and realm crowd the password out of the Blowfish key
    schedule. Such a hash was stored without depending on the whole of the password it claims to
    check, and verification refuses the ones that reach the window, so the operator must be told
    which credentials to regenerate. Only websAddUser knows how the composite splits into username,
    realm and password, so the report belongs here rather than in the verifier.
 */
static void reportPasswordBudget(cchar *username, cchar *password)
{
    char  *ph, *salt, *tok;
    ssize budget;

    if (!sstarts(password, WEBS_CRYPT_BLOWFISH ":")) {
        return;
    }
    ph = sclone(password);
    stok(ph, ":", &tok);
    stok(NULL, ":", &tok);
    if ((salt = stok(NULL, ":", &tok)) == 0) {
        wfree(ph);
        return;
    }
    /*
        Key material is "salt:username:realm:password". Whatever the three known fields and their
        separators do not consume is the password's budget inside the window.
     */
    budget = WEBS_BLOWFISH_KEY_WINDOW - (slen(salt) + 1 + slen(username) + 1 + slen(ME_GOAHEAD_REALM) + 1);
    if (budget <= 0) {
        error("User \"%s\" has a legacy %s password hash that does not depend on the password. "
              "Authentication is refused for this user. Regenerate the password with gopass.",
              username, WEBS_CRYPT_BLOWFISH);
    } else if (budget < WEBS_BLOWFISH_MIN_BUDGET) {
        error("User \"%s\" has a legacy %s password hash that depends on only the first %zd password "
              "characters. Regenerate the password with gopass.", username, WEBS_CRYPT_BLOWFISH, budget);
    }
    wfree(ph);
}


WebsUser *websAddUser(cchar *username, cchar *password, cchar *roles)
{
    WebsUser *user;

    if (!username) {
        error("User is missing name");
        return 0;
    }
    if (websLookupUser(username)) {
        error("User %s already exists", username);
        /* Already exists */
        return 0;
    }
    reportPasswordBudget(username, password);
    if ((user = createUser(username, password, roles)) == 0) {
        return 0;
    }
    if (hashEnter(users, username, valueSymbol(user), 0) == 0) {
        return 0;
    }
    return user;
}


PUBLIC int websRemoveUser(cchar *username)
{
    WebsKey *key;

    assert(username);
    if ((key = hashLookup(users, username)) != 0) {
        websReleaseUser(key->content.value.symbol);
        freeUser(key->content.value.symbol);
    }
    return hashDelete(users, username);
}


static void freeUser(WebsUser *up)
{
    assert(up);
    hashFree(up->abilities);
    wfreeset(up->name);
    wfreeset(up->password);
    wfreeset(up->roles);
    wfree(up);
}


PUBLIC int websSetUserPassword(cchar *username, cchar *password)
{
    WebsUser *user;

    assert(username);
    if ((user = websLookupUser(username)) == 0) {
        return -1;
    }
    wfreeset(user->password);
    user->password = sclone(password);
    return 0;
}


PUBLIC int websSetUserRoles(cchar *username, cchar *roles)
{
    WebsUser *user;

    assert(username);
    if ((user = websLookupUser(username)) == 0) {
        return -1;
    }
    wfreeset(user->roles);
    user->roles = sclone(roles);
    computeUserAbilities(user);
    return 0;
}


WebsUser *websLookupUser(cchar *username)
{
    WebsKey *key;

    assert(username);
    if ((key = hashLookup(users, username)) == 0) {
        return 0;
    }
    return (WebsUser*) key->content.value.symbol;
}


static void computeAbilities(WebsHash abilities, cchar *role, int depth)
{
    WebsRole *rp;
    WebsKey  *key;

    assert(abilities >= 0);
    assert(role && *role);
    assert(depth >= 0);

    if (depth > 20) {
        error("Recursive ability definition for %s", role);
        return;
    }
    if (roleTable >= 0) {
        if ((key = hashLookup(roleTable, role)) != 0) {
            rp = (WebsRole*) key->content.value.symbol;
            for (key = hashFirst(rp->abilities); key; key = hashNext(rp->abilities, key)) {
                computeAbilities(abilities, key->name.value.string, ++depth);
            }
        } else {
            hashEnter(abilities, role, valueInteger(0), 0);
        }
    }
}


static void computeUserAbilities(WebsUser *user)
{
    char *ability, *roles, *tok;

    assert(user);
    hashFree(user->abilities);
    if ((user->abilities = hashCreate(-1)) < 0) {
        return;
    }
    roles = sclone(user->roles);
    for (ability = stok(roles, " \t,", &tok); ability; ability = stok(NULL, " \t,", &tok)) {
        computeAbilities(user->abilities, ability, 0);
    }
#if ME_DEBUG && DISABLE
    /*
        SECURITY Acceptable: - Debug trace of user abilities
        Do not enable in production
     */
    {
        WebsKey *key;
        trace(5 | WEBS_RAW_MSG, "User \"%s\" has abilities: ", user->name);
        for (key = hashFirst(user->abilities); key; key = hashNext(user->abilities, key)) {
            trace(5 | WEBS_RAW_MSG, "%s ", key->name.value.string);
            ability = key->name.value.string;
        }
        trace(5, "");
    }
#endif
    wfree(roles);
}


PUBLIC void websComputeAllUserAbilities(void)
{
    WebsUser *user;
    WebsKey  *sym;

    if (users) {
        for (sym = hashFirst(users); sym; sym = hashNext(users, sym)) {
            user = (WebsUser*) sym->content.value.symbol;
            computeUserAbilities(user);
        }
    }
}


WebsRole *websAddRole(cchar *name, WebsHash abilities)
{
    WebsRole *rp;

    if (!name) {
        error("Role is missing name");
        return 0;
    }
    if (hashLookup(roleTable, name)) {
        error("Role %s already exists", name);
        /* Already exists */
        return 0;
    }
    if ((rp = walloc(sizeof(WebsRole))) == 0) {
        return 0;
    }
    rp->abilities = abilities;
    if (hashEnter(roleTable, name, valueSymbol(rp), 0) == 0) {
        return 0;
    }
    return rp;
}


static void freeRole(WebsRole *rp)
{
    assert(rp);
    hashFree(rp->abilities);
    wfree(rp);
}


/*
    Does not recompute abilities for users that use this role
 */
PUBLIC int websRemoveRole(cchar *name)
{
    WebsRole *rp;
    WebsKey  *sym;

    assert(name && *name);
    if (roleTable >= 0) {
        if ((sym = hashLookup(roleTable, name)) == 0) {
            return -1;
        }
        rp = sym->content.value.symbol;
        hashFree(rp->abilities);
        wfree(rp);
        return hashDelete(roleTable, name);
    }
    return -1;
}


PUBLIC WebsHash websGetUsers(void)
{
    return users;
}


PUBLIC WebsHash websGetRoles(void)
{
    return roleTable;
}


PUBLIC bool websLoginUser(Webs *wp, cchar *username, cchar *password)
{
    char *pass;

    assert(wp);
    assert(wp->route);
    assert(username);
    assert(password);

    if (!wp->route || !getVerify(wp->route)) {
        return 0;
    }
    setUsername(wp, username);
    pass = sclone(password);
    wfreeset(wp->password);
    wp->password = pass;
    /*
        Clear cached password-format state before checking plaintext credentials.
     */
    wp->encoded = 0;
    wfreeset(wp->digest);
    wp->authReason = 0;

    if (!(getVerify(wp->route))(wp)) {
        logAuth(wp, 0, "form", "failed", wp->authReason ? wp->authReason : "invalid credentials");
        // Clear unverified identity before exposing it through request APIs.
        wfreeset(wp->username);
        wfreeset(wp->password);
        wp->user = 0;
        return 0;
    }
    /*
        Application login creates a form-auth session, not Basic or Digest authentication.
     */
    if (websLoginSession(wp, "form") != 0) {
        logAuth(wp, 0, "form", "succeeded", "login");
    } else {
        /*
            Session storage is full or unavailable. Fail the login rather than reporting success: the
            client would otherwise be redirected to the protected page carrying no session cookie and
            bounced straight back to the login form with no error anywhere it can see.
         */
        error("Cannot create session for %s", wp->username);
        return 0;
    }
    return 1;
}


PUBLIC bool websLogoutUser(Webs *wp)
{
    assert(wp);
    logAuth(wp, 0, "form", "logout", 0);
    websRemoveSessionVar(wp, WEBS_SESSION_USERNAME);
    websRemoveSessionVar(wp, WEBS_SESSION_AUTHTYPE);
    websDestroySession(wp);
    if (smatch(wp->authType, "basic") || smatch(wp->authType, "digest")) {
        websError(wp, HTTP_CODE_UNAUTHORIZED, "Logged out.");
        return 0;
    }
    websRedirectByStatus(wp, HTTP_CODE_OK);
    return 1;
}


/*
    Internal login service routine for Form-based auth
 */
/*
    Accept only local paths rooted at one slash; reject network-path references.
 */
static bool isLocalPath(cchar *uri)
{
    return uri && uri[0] == '/' && uri[1] != '/' && uri[1] != '\\';
}


/*
    Read credentials only from the body to keep them out of URL logs and history.
 */
static cchar *getCredential(Webs *wp, cchar *name)
{
    WebsKey *sp;

    if ((sp = hashLookup(wp->vars, name)) == 0 || sp->arg != WEBS_VAR_BODY) {
        return "";
    }
    return websGetVar(wp, name, "");
}


static void loginServiceProc(Webs *wp)
{
    WebsRoute *route;
    char      *referrer;

    assert(wp);
    route = wp->route;
    assert(route);

    /* Read the local redirect before login rotates the session. */
    referrer = sclone(websGetSessionVar(wp, "referrer", 0));
    if (websLoginUser(wp, getCredential(wp, "username"), getCredential(wp, "password"))) {
        if (isLocalPath(referrer)) {
            websRedirect(wp, referrer);
        } else {
            if (*referrer) {
                trace(2, "Ignoring a login referrer that is not a path on this server");
            }
            websRedirectByStatus(wp, HTTP_CODE_OK);
        }
        websSetSessionVar(wp, "loginStatus", "ok");
    } else {
        if (route->askLogin) {
            (route->askLogin)(wp);
        }
        /*
            SECURITY: only record the failure in a session that already exists. Creating one here would
            let any unauthenticated client allocate the scarcest server resource (capped by
            ME_GOAHEAD_LIMIT_SESSION_COUNT) with a single anonymous POST, and would hand the attacker a
            live server-issued session id. loginStatus is only meaningful to a client that already holds
            a session to read it back from.
         */
        if (websGetSession(wp, 0) != 0) {
            websSetSessionVar(wp, "loginStatus", "failed");
        }
        websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
    }
    wfree(referrer);
}


static void logoutServiceProc(Webs *wp)
{
    websLogoutUser(wp);
}


static void basicLogin(Webs *wp)
{
    assert(wp);
    assert(wp->route);
    wfreeset(wp->authResponse);
    wp->authResponse = sfmt("Basic realm=\"%s\"", ME_GOAHEAD_REALM);
}


/*
    Resolve the route or store verifier at use so setter changes affect loaded routes.
 */
static WebsVerify getVerify(WebsRoute *route)
{
    return route->verify ? route->verify : verifyPassword;
}


void websSetPasswordStoreVerify(WebsVerify verify)
{
    verifyPassword = verify;
}


WebsVerify websGetPasswordStoreVerify(void)
{
    return verifyPassword;
}


/*
    Reject truncated username:realm:password input so every password byte is hashed.
 */
static bool makePasswordPreimage(char *buf, ssize bufsize, cchar *username, cchar *password)
{
    if ((slen(username) + slen(ME_GOAHEAD_REALM) + slen(password) + 3) > bufsize) {
        return 0;
    }
    fmt(buf, bufsize, "%s:%s:%s", username, ME_GOAHEAD_REALM, password);
    return 1;
}


PUBLIC bool websVerifyPasswordFromFile(Webs *wp)
{
    char passbuf[WEBS_MAX_PASSWORD_BUFFER];
    bool success;

    assert(wp);
    if (!wp->user && (wp->user = websLookupUser(wp->username)) == 0) {
        trace(5, "verifyUser: Unknown user \"%s\"", wp->username);
        wp->authReason = "unknown user";
        verifyUnknownUser(wp);
        return 0;
    }
    /*
        Enforce a plaintext password length ceiling independent of username or realm length.
        For Digest auth wp->password holds a precomputed response hash, not plaintext, so the
        check is gated on !wp->encoded.
     */
    if (!wp->encoded && wp->password && slen(wp->password) > WEBS_MAX_PASSWORD) {
        trace(5, "Password for user \"%s\" exceeds maximum length", wp->username);
        wp->authReason = "password too long";
        return 0;
    }
    /*
        Verify wp->password against the stored user->password record. wp->password holds the
        client-computed MD5(HA1:nonce:HA2) when wp->encoded is set, otherwise plaintext. Digest is
        selected first on the request-level wp->digest signal so it cannot be diverted through the
        Blowfish branch, which carries no plaintext to bcrypt; Basic and form auth then dispatch on
        the stored hash format. Plaintext is wrapped as username:realm:password so a stored hash is
        not portable across users or realms.
     */
    if (wp->digest) {
        /* Digest requires a stored MD5 HA1. Refuse Blowfish hashes and accounts without passwords. */
        if (!isMd5Hash(wp->user->password)) {
            trace(5, "Digest authentication requires an MD5-HA1 stored hash for user \"%s\"", wp->username);
            wp->authReason = "stored password cannot verify digest credentials";
            return 0;
        }
        success = pmatch(wp->digest, wp->password);
    } else if (isBlowfishHash(wp->user->password)) {
        // Blowfish stored hash. wp->password is plaintext; bcrypt and verify.
        if (!makePasswordPreimage(passbuf, sizeof(passbuf), wp->username, wp->password)) {
            trace(5, "Credentials for user \"%s\" do not fit the password buffer", wp->username);
            wp->authReason = "credentials too long";
            return 0;
        }
        success = websCheckPassword(passbuf, wp->user->password);
    } else {
        // Legacy MD5 stored hash: MD5(username:realm:password).
        if (!wp->encoded) {
            /*
                wp->password is plaintext from Basic auth. Encode to MD5 and cache by setting wp->encoded
                so subsequent calls on the same request skip the rehash.
                SECURITY Acceptable: - Legacy MD5 hash - required for backward compatibility with digest auth
                which must use MD5. Newer clients may support other algorithms, but not widespread.
             */
            if (!makePasswordPreimage(passbuf, sizeof(passbuf), wp->username, wp->password)) {
                trace(5, "Credentials for user \"%s\" do not fit the password buffer", wp->username);
                wp->authReason = "credentials too long";
                return 0;
            }
            wfreeset(wp->password);
            wp->password = websMD5(passbuf);
            wp->encoded = 1;
        }
        success = pmatch(wp->password, wp->user->password);
    }
    if (success) {
        trace(5, "User \"%s\" authenticated", wp->username);
    } else {
        trace(5, "Password for user \"%s\" failed to authenticate", wp->username);
        wp->authReason = "password does not match";
    }
    return success;
}


/*
    Check a configured Blowfish hash for unknown users to reduce timing differences. Ignore the result.
 */
static void verifyUnknownUser(Webs *wp)
{
    WebsKey  *key;
    WebsUser *user;
    char     passbuf[WEBS_MAX_PASSWORD_BUFFER];

    if (wp->encoded || slen(wp->password) > WEBS_MAX_PASSWORD ||
        !makePasswordPreimage(passbuf, sizeof(passbuf), wp->username, wp->password)) {
        return;
    }
    for (key = hashFirst(users); key; key = hashNext(users, key)) {
        user = key->content.value.symbol;
        if (isBlowfishHash(user->password) && isCompleteHash(user->password)) {
            websCheckPassword(passbuf, user->password);
            return;
        }
    }
}


/*
    True when a Blowfish hash has its four fields: algorithm, rounds, salt and hash.
 */
static bool isCompleteHash(cchar *hash)
{
    int fields;

    for (fields = 1; *hash; hash++) {
        if (*hash == ':') {
            fields++;
        }
    }
    return fields == 4;
}


/*
    Constant time password match
 */
static bool pmatch(cchar *s1, cchar *s2)
{
    ssize i, len1, len2, maxLen;
    uchar c, lengthDiff;

    len1 = slen(s1);
    len2 = slen(s2);

    /* Record if lengths differ, but don't return early */
    lengthDiff = (uchar) (len1 != len2);

    /* Always compare the maximum length to ensure constant time */
    maxLen = (len1 > len2) ? len1 : len2;

    /* Perform comparison over the full maximum length */
    for (i = 0, c = 0; i < maxLen; i++) {
        uchar c1 = (i < len1) ? (uchar) s1[i] : 0;
        uchar c2 = (i < len2) ? (uchar) s2[i] : 0;
        c |= c1 ^ c2;
    }

    /* Include length difference in the final result */
    c |= lengthDiff;

    return !c;
}


#if ME_COMPILER_HAS_PAM
/*
    Copy this routine if creating your own custom password store back end
 */
PUBLIC bool websVerifyPasswordFromPam(Webs *wp)
{
    WebsBuf         abilities;
    pam_handle_t    *pamh;
    UserInfo        info;
    struct pam_conv conv = { pamChat, &info };
    struct group    *gp;
    int             res, i;

    assert(wp);
    assert(wp->username && wp->username);
    assert(wp->password);

    /*
        PAM verifies a plaintext password. A Digest response or an encoded password is not one.
     */
    if (wp->encoded || wp->digest) {
        trace(5, "PAM cannot verify a Digest or encoded credential");
        wp->authReason = "PAM cannot verify digest credentials";
        return 0;
    }
    info.name = (char*) wp->username;
    info.password = (char*) wp->password;
    pamh = NULL;
    if ((res = pam_start("login", info.name, &conv, &pamh)) != PAM_SUCCESS) {
        return 0;
    }
    if ((res = pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK)) != PAM_SUCCESS) {
        pam_end(pamh, PAM_SUCCESS);
        trace(5, "httpPamVerifyUser failed to verify %s", wp->username);
        wp->authReason = "password does not match";
        return 0;
    }
    pam_end(pamh, PAM_SUCCESS);
    trace(5, "httpPamVerifyUser verified %s", wp->username);

    if (!wp->user) {
        wp->user = websLookupUser(wp->username);
    }
    if (!wp->user) {
        Gid groups[32];
        int ngroups;
        /*
            Create a temporary user with a abilities set to the groups
         */
        ngroups = sizeof(groups) / sizeof(Gid);
        if ((i = getgrouplist(wp->username, 99999, groups, &ngroups)) >= 0) {
            bufCreate(&abilities, 128, -1);
            for (i = 0; i < ngroups; i++) {
                if ((gp = getgrgid(groups[i])) != 0) {
                    bufPutStr(&abilities, gp->gr_name);
                    bufPutc(&abilities, ' ');
                }
            }
            bufAddNull(&abilities);
            trace(5, "Create temp user \"%s\" with abilities: %s", wp->username, abilities.servp);
            /*
                websAddUser copies the ability string, so the buffer is done with either way. Free
                it on both paths: this runs once per authentication of a user not already in the
                user table, so leaking it here is a remote-request-driven leak on a long-running
                device.
             */
            wp->user = websAddUser(wp->username, 0, abilities.servp);
            bufFree(&abilities);
            if (wp->user == 0) {
                return 0;
            }
            computeUserAbilities(wp->user);
        }
    }
    return 1;
}


/*
    Callback invoked by the pam_authenticate function
 */
static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data)
{
    UserInfo            *info;
    struct pam_response *reply;
    int                 i, j;

    i = 0;
    reply = 0;
    info = (UserInfo*) data;

    if (resp == 0 || msg == 0 || info == 0) {
        return PAM_CONV_ERR;
    }
    if ((reply = calloc(msgCount, sizeof(struct pam_response))) == 0) {
        return PAM_CONV_ERR;
    }
    for (i = 0; i < msgCount; i++) {
        reply[i].resp_retcode = 0;
        reply[i].resp = 0;

        switch (msg[i]->msg_style) {
        /*
            strdup, not sclone. Once this callback returns, the responses belong to libpam, which
            releases them with free(). Under ME_GOAHEAD_REPLACE_MALLOC walloc hands back an interior
            pointer into its own arena, and free() must never see one.
         */
        case PAM_PROMPT_ECHO_ON:
            reply[i].resp = strdup(info->name);
            break;

        case PAM_PROMPT_ECHO_OFF:
            /* Retrieve the user password and pass onto pam */
            reply[i].resp = strdup(info->password);
            break;

        default:
            goto err;
        }
        if (reply[i].resp == 0) {
            goto err;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;

err:
    /*
        Release the responses already built. libpam never sees them, so this side owns them, and
        they are C library allocations like the array itself.
     */
    for (j = 0; j < i; j++) {
        free(reply[j].resp);
    }
    free(reply);
    return PAM_CONV_ERR;
}
#endif /* ME_COMPILER_HAS_PAM */


static bool parseBasicDetails(Webs *wp)
{
    char *cp, *userAuth;

    assert(wp);
    /*
        Split userAuth into userid and password
     */
    cp = 0;
    if ((userAuth = websDecode64(wp->authDetails)) != 0) {
        if ((cp = strchr(userAuth, ':')) != NULL) {
            *cp++ = '\0';
        }
    }
    wfreeset(wp->password);
    if (cp) {
        setUsername(wp, userAuth);
        wp->password = sclone(cp);
        wp->encoded = 0;
    } else {
        setUsername(wp, "");
        wp->password = sclone("");
    }
    wfree(userAuth);
    return 1;
}


#if ME_GOAHEAD_DIGEST
static void digestLogin(Webs *wp)
{
    char *nonce, *opaque, *domain;

    assert(wp);
    assert(wp->route);

    wfreeset(wp->authResponse);
    /* Without a clock there is no nonce to issue, so offer no challenge */
    if ((nonce = createDigestNonce(wp)) == 0) {
        return;
    }
    /* Opaque is unused. Set to anything */
    opaque = "5ccc069c403ebaf9f0171e9517f40e41";
    /*
        Omit an unknown Digest domain; RFC 7616 then covers this server's URIs.
     */
    domain = websGetServerUrl() ? sfmt("domain=\"%s\", ", websGetServerUrl()) : sclone("");
    wp->authResponse = sfmt(
        "Digest realm=\"%s\", %sqop=\"%s\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\", stale=\"%s\"",
        ME_GOAHEAD_REALM, domain, "auth", nonce, opaque, "MD5", (wp->flags & WEBS_NONCE_STALE) ? "TRUE" : "FALSE");
    wfree(domain);
    wfree(nonce);
}


static bool parseDigestDetails(Webs *wp)
{
    WebsTime when, now;
    char     *decoded, *value, *tok, *key, *keyBuf, *dp, *sp, *secret, *counterStr, *expected;
    int      seenComma;

    assert(wp);
    key = keyBuf = sclone(wp->authDetails);

    while (*key) {
        while (*key && isspace((uchar) *key)) {
            key++;
        }
        tok = key;
        while (*tok && !isspace((uchar) *tok) && *tok != ',' && *tok != '=') {
            tok++;
        }
        if (*tok) {
            *tok++ = '\0';
        }

        while (isspace((uchar) *tok)) {
            tok++;
        }
        seenComma = 0;
        if (*tok == '\"') {
            /*
                Skip escaped quotes while scanning; never advance past a trailing backslash.
             */
            value = ++tok;
            while (*tok != '\"' && *tok != '\0') {
                if (*tok == '\\' && tok[1]) {
                    tok++;
                }
                tok++;
            }
        } else {
            value = tok;
            while (*tok != ',' && *tok != '\0') {
                tok++;
            }
            seenComma++;
        }
        if (*tok) {
            *tok++ = '\0';
        }

        /*
            Handle back-quoting
         */
        if (strchr(value, '\\')) {
            for (dp = sp = value; *sp; sp++) {
                if (*sp == '\\' && sp[1]) {
                    sp++;
                }
                *dp++ = *sp;
            }
            *dp = '\0';
        }

        /*
            user, response, oqaque, uri, realm, nonce, nc, cnonce, qop
         */
        switch (tolower((uchar) *key)) {
        case 'a':
            if (scaselesscmp(key, "algorithm") == 0) {
                break;
            } else if (scaselesscmp(key, "auth-param") == 0) {
                break;
            }
            break;

        case 'c':
            if (scaselesscmp(key, "cnonce") == 0) {
                wfreeset(wp->cnonce);
                wp->cnonce = sclone(value);
            }
            break;

        case 'd':
            if (scaselesscmp(key, "domain") == 0) {
                break;
            }
            break;

        case 'n':
            if (scaselesscmp(key, "nc") == 0) {
                wfreeset(wp->nc);
                wp->nc = sclone(value);
            } else if (scaselesscmp(key, "nonce") == 0) {
                wfreeset(wp->nonce);
                wp->nonce = sclone(value);
            }
            break;

        case 'o':
            if (scaselesscmp(key, "opaque") == 0) {
                wfreeset(wp->opaque);
                wp->opaque = sclone(value);
            }
            break;

        case 'q':
            if (scaselesscmp(key, "qop") == 0) {
                wfreeset(wp->qop);
                wp->qop = sclone(value);
            }
            break;

        case 'r':
            if (scaselesscmp(key, "realm") == 0) {
                wfreeset(wp->realm);
                wp->realm = sclone(value);
            } else if (scaselesscmp(key, "response") == 0) {
                /*
                    SECURITY Acceptable: MD5 is used for legacy digest authentication.
                    Store the response digest in the password field. This is MD5(HA1:nonce:HA2) where
                    HA1 is MD5(user:realm:password) and HA2 is MD5(method:digestUri)
                 */
                wfreeset(wp->password);
                wp->password = sclone(value);
                wp->encoded = 1;
            }
            break;

        case 's':
            /*  Just ignore "stale" */
            break;

        case 'u':
            if (scaselesscmp(key, "uri") == 0) {
                wfreeset(wp->digestUri);
                wp->digestUri = sclone(value);
            } else if (scaselesscmp(key, "username") == 0 || scaselesscmp(key, "user") == 0) {
                setUsername(wp, value);
            }
            break;

        default:
            /*  Just ignore keywords we don't understand */
            ;
        }
        key = tok;
        if (!seenComma) {
            while (*key && *key != ',') {
                key++;
            }
            if (*key) {
                key++;
            }
        }
    }
    wfree(keyBuf);

    /*
        Require every mandatory field to be nonempty before parsing.
     */
    if (wp->username == 0 || wp->realm == 0 || wp->nonce == 0 || *wp->nonce == '\0' || wp->route == 0 ||
        wp->password == 0) {
        wp->authReason = "missing digest fields";
        return 0;
    }
    /*
        Require the client's realm to match this server's challenge.
     */
    if (!smatch(wp->realm, ME_GOAHEAD_REALM)) {
        wp->authReason = "realm mismatch";
        return 0;
    }
    /*
        Compare Digest uri with the request target in origin form (RFC 7616 3.4.6).
     */
    if (!wp->digestUri || !smatch(websOriginForm(wp->digestUri), websOriginForm(wp->url))) {
        wp->authReason = "digest uri does not match the request target";
        return 0;
    }
    if (wp->qop && (wp->cnonce == 0 || wp->nc == 0)) {
        wp->authReason = "missing digest fields";
        return 0;
    }
    if (wp->qop == 0) {
        wp->qop = sclone("");
    }
    /*
        Require the exact issued nonce encoding so alternate spellings cannot bypass replay tracking. Defer expiry
        rejection until the response verifies, to report stale nonces.
     */
    when = 0; secret = 0; counterStr = 0;
    decoded = parseDigestNonce(wp->nonce, &secret, &when, &counterStr);
    if (decoded == 0 || secret == 0 || counterStr == 0) {
        wp->authReason = "bad nonce";
        wfree(decoded);
        return 0;
    }
    if (!smatch(wp->qop, "auth")) {
        wp->authReason = "bad qop";
        wfree(decoded);
        return 0;
    }
    if ((now = websTime()) < 0) {
        wp->authReason = "system clock unavailable";
        wfree(decoded);
        return 0;
    }
    if (when > now) {
        wp->authReason = "nonce has a future timestamp";
        wfree(decoded);
        return 0;
    }
    expected = formatDigestNonce(when, parseHex64(counterStr));
    if (!pmatch(expected, wp->nonce)) {
        wfree(expected);
        wp->authReason = "nonce mismatch";
        wfree(decoded);
        return 0;
    }
    wfree(expected);
    wp->nonceTime = when;
    if (!wp->user) {
        if ((wp->user = websLookupUser(wp->username)) == 0) {
            wp->authReason = "unknown user";
            wfree(decoded);
            return 0;
        }
    }
    wfree(decoded);
    wfreeset(wp->digest);
    wp->digest = calcDigest(wp, 0, wp->user->password);
    return 1;
}


static bool nonceStale(WebsTime when)
{
    WebsTime now;

    return (now = websTime()) < 0 || when + ME_GOAHEAD_NONCE_DURATION < now || when <= nonceFloor;
}


/*
    Check nonce freshness and increasing count without recording it until authorization succeeds. Pack issue time
    and count into two 32-bit words for Windows compatibility.
 */
static bool checkNonceCount(Webs *wp)
{
    WebsKey *key;
    cchar   *cp;

    if (slen(wp->nc) != 8) {
        wp->authReason = "invalid nonce count";
        return 0;
    }
    for (cp = wp->nc; *cp; cp++) {
        if (!isxdigit((uchar) *cp)) {
            wp->authReason = "invalid nonce count";
            return 0;
        }
    }
    if (nonceStale(wp->nonceTime)) {
        wp->authReason = "nonce is stale";
        wp->flags |= WEBS_NONCE_STALE;
        return 0;
    }
    if ((key = hashLookup(nonceCounts, wp->nonce)) != 0 && (uint) hextoi(wp->nc) <= (uint) key->content.value.big[1]) {
        wp->authReason = "nonce count already used";
        return 0;
    }
    return 1;
}


/* Evict the oldest nonces and mark their issue times stale to prevent replay after eviction. */
static void evictNonces(void)
{
    WebsKey  *key, *next;
    WebsTime when, oldest;

    while (nonceTotal >= ME_GOAHEAD_LIMIT_NONCES) {
        oldest = 0;
        for (key = hashFirst(nonceCounts); key; key = hashNext(nonceCounts, key)) {
            when = (WebsTime) (uint) key->content.value.big[0];
            if (oldest == 0 || when < oldest) {
                oldest = when;
            }
        }
        for (key = hashFirst(nonceCounts); key; key = next) {
            next = hashNext(nonceCounts, key);
            if ((WebsTime) (uint) key->content.value.big[0] <= oldest) {
                hashDelete(nonceCounts, key->name.value.string);
                nonceTotal--;
            }
        }
        if (oldest > nonceFloor) {
            nonceFloor = oldest;
        }
    }
}


PUBLIC void websAcceptDigestCount(Webs *wp)
{
    WebsValue record;

    if (!(wp->flags & WEBS_AUTHENTICATED) || !wp->route || !smatch(wp->route->authType, "digest") || !wp->nonce ||
        !wp->nc || nonceCounts < 0) {
        return;
    }
    if (!hashLookup(nonceCounts, wp->nonce)) {
        evictNonces();
        nonceTotal++;
    }
    memset(&record, 0, sizeof(record));
    record.valid = 1;
    record.type = big;
    record.value.big[0] = (long) (uint) wp->nonceTime;
    record.value.big[1] = (long) (uint) hextoi(wp->nc);
    hashEnter(nonceCounts, wp->nonce, record, 0);
}


static void pruneNonces(void *data, int id)
{
    WebsKey  *key, *next;
    WebsTime now;

    if (nonceCounts >= 0 && (now = websTime()) >= 0) {
        for (key = hashFirst(nonceCounts); key; key = next) {
            next = hashNext(nonceCounts, key);
            if ((WebsTime) (uint) key->content.value.big[0] + ME_GOAHEAD_NONCE_DURATION < now) {
                hashDelete(nonceCounts, key->name.value.string);
                nonceTotal--;
            }
        }
    }
    websRestartEvent(noncePruneId, WEBS_SESSION_PRUNE);
}


/*
    Nonce format: base64(MD5(masterSecret:realm:hex_time:hex_counter):hex_time:hex_counter).
    SECURITY Acceptable: MD5 supports legacy Digest nonce construction without exposing the secret.
 */
static char *formatDigestNonce(WebsTime when, uint64 counter)
{
    char *hash, *preimage, *plain, *encoded;

    preimage = sfmt("%s:%s:%x:%llx", masterSecret, ME_GOAHEAD_REALM, (uint) when, (unsigned long long) counter);
    hash = websMD5Block(preimage, slen(preimage), NULL);
    wfree(preimage);
    plain = sfmt("%s:%x:%llx", hash, (uint) when, (unsigned long long) counter);
    wfree(hash);
    encoded = websEncode64(plain);
    wfree(plain);
    return encoded;
}


/*
    Return a nonce with a 32-bit timestamp, or NULL if the clock fails or exceeds the field.
 */
static char *createDigestNonce(Webs *wp)
{
    static uint64 next = 0;
    WebsTime      now;

    assert(wp);
    assert(wp->route);
    if ((now = websTime()) < 0 || now > (WebsTime) UINT_MAX) {
        trace(2, "Cannot issue Digest nonce: system clock unavailable");
        return 0;
    }
    return formatDigestNonce(now, next++);
}


static uint64 parseHex64(cchar *str)
{
    uint64 value;

    for (value = 0; str && isxdigit((uchar) *str); str++) {
        value = value * 16 + (uint64) (isdigit((uchar) *str) ? *str - '0' : tolower((uchar) *str) - 'a' + 10);
    }
    return value;
}


/*
    Parse a digest nonce of the form base64(hash:hex_time:hex_counter).
    Sets *secret to the hash field, *realm to ME_GOAHEAD_REALM (always the server realm),
    *when to the decoded timestamp, and *counterStr to the counter string.
    The caller owns the returned decoded buffer; *secret and *counterStr point into it.
 */
static char *parseDigestNonce(char *nonce, char **secret, WebsTime *when, char **counterStr)
{
    char *tok, *decoded, *whenStr;

    assert(nonce);
    assert(secret);
    assert(when);
    assert(counterStr);

    if ((decoded = websDecode64(nonce)) == 0) {
        return 0;
    }
    *secret = 0;
    *when = 0;
    *counterStr = 0;
    if (strchr(decoded, ':')) {
        *secret = stok(decoded, ":", &tok);
        whenStr = stok(NULL, ":", &tok);
        *counterStr = stok(NULL, ":", &tok);
        *when = (WebsTime) hextoi(whenStr);
    }
    return decoded;
}


/*
   Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
   SECURITY Acceptable: MD5 is used for legacy digest authentication.
 */
static char *calcDigest(Webs *wp, char *username, char *password)
{
    char *a1, *a2, *preimage, *ha1, *ha2, *result;

    assert(wp);
    assert(password);

    /*
        Hash complete inputs without truncation. With no username, password already contains HA1.
     */
    if (username == 0) {
        ha1 = sclone(password);
    } else {
        a1 = sfmt("%s:%s:%s", username, wp->realm, password);
        ha1 = websMD5(a1);
        wfree(a1);
    }
    a2 = sfmt("%s:%s", wp->method, wp->digestUri);
    ha2 = websMD5(a2);
    wfree(a2);

    /*
        H(HA1:nonce:nc:cnonce:qop:HA2) for qop=auth and auth-int, else the RFC 2069 form H(HA1:nonce:HA2)
     */
    if (smatch(wp->qop, "auth") || smatch(wp->qop, "auth-int")) {
        preimage = sfmt("%s:%s:%s:%s:%s:%s", ha1, wp->nonce, wp->nc, wp->cnonce, wp->qop, ha2);
    } else {
        preimage = sfmt("%s:%s:%s", ha1, wp->nonce, ha2);
    }
    result = websMD5(preimage);
    wfree(preimage);
    wfree(ha1);
    wfree(ha2);
    return result;
}
#endif /* ME_GOAHEAD_DIGEST */


PUBLIC int websSetRouteAuth(WebsRoute *route, cchar *auth)
{
    WebsParseAuth parseAuth;
    WebsAskLogin  askLogin;

    assert(route);
    assert(auth && *auth);

    askLogin = 0;
    parseAuth = 0;
    if (smatch(auth, "basic")) {
        askLogin = basicLogin;
        parseAuth = parseBasicDetails;
#if ME_GOAHEAD_DIGEST
    } else if (smatch(auth, "digest")) {
#if ME_COMPILER_HAS_PAM
        /* PAM requires plaintext passwords and cannot verify Digest responses. */
        if (getVerify(route) == websVerifyPasswordFromPam) {
            error("Route \"%s\" requests digest authentication, which the PAM password store cannot verify. "
                  "Use basic or form authentication, or the file password store.", route->prefix);
            return -1;
        }
#endif
        askLogin = digestLogin;
        parseAuth = parseDigestDetails;
#endif
    } else if (smatch(auth, "form")) {
        /*
            Form authentication carries no per-request credential to parse and issues no challenge:
            the login action establishes the session and the route's abilities gate the request. It
            still sets authType so the route counts as authenticated.
         */
#if !ME_GOAHEAD_DIGEST
    } else if (smatch(auth, "digest")) {
        error("Route \"%s\" requests digest authentication, but this build has ME_GOAHEAD_DIGEST=0. "
              "Rebuild with ME_GOAHEAD_DIGEST=1 or use another authentication type.", route->prefix);
        return -1;
#endif
    } else {
        /*
            Refuse an unrecognized type. Falling back to the form behaviour gives a route that can
            never issue a challenge and denies every request.
         */
        error("Route \"%s\" requests unknown authentication type \"%s\". Use \"basic\", \"digest\" "
              "or \"form\".", route->prefix, auth);
        return -1;
    }
#if !ME_COM_SSL
    error("Warning: Route \"%s\" uses %s authentication but this build has no TLS: credentials and session cookies "
          "cross the network unencrypted", route->prefix, auth);
#endif
    route->authType = sclone(auth);
    route->askLogin = askLogin;
    route->parseAuth = parseAuth;
    return 0;
}
#endif /* ME_GOAHEAD_AUTH */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
