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

static WebsHash users = -1;
static WebsHash roles = -1;
static char *masterSecret;
static int autoLogin = ME_GOAHEAD_AUTO_LOGIN;
static WebsVerify verifyPassword = websVerifyPasswordFromFile;
static bool pmatch(cchar *s1, cchar *s2);

#ifndef ME_GOAHEAD_NONCE_DURATION
    #define ME_GOAHEAD_NONCE_DURATION 60
#endif

#if ME_COMPILER_HAS_PAM
typedef struct {
    char    *name;
    char    *password;
} UserInfo;

#if MACOSX
    typedef int Gid;
#else
    typedef gid_t Gid;
#endif
#endif

/********************************** Forwards **********************************/

static void computeAbilities(WebsHash abilities, cchar *role, int depth);
static void computeUserAbilities(WebsUser *user);
static WebsUser *createUser(cchar *username, cchar *password, cchar *roles);
static void freeRole(WebsRole *rp);
static void freeUser(WebsUser *up);
static void logoutServiceProc(Webs *wp);
static void loginServiceProc(Webs *wp);

#if ME_GOAHEAD_DIGEST
static char *calcDigest(Webs *wp, char *username, char *password);
static char *createDigestNonce(Webs *wp);
static char *parseDigestNonce(char *nonce, char **secret, char **realm, WebsTime *when);
#endif

#if ME_COMPILER_HAS_PAM
static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data);
#endif

/************************************ Code ************************************/

PUBLIC bool websAuthenticate(Webs *wp)
{
    WebsRoute   *route;
    char        *username;
    int         cached;

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
            Retrieve authentication state from the session storage. Faster than re-authenticating.
         */
        if ((username = (char*) websGetSessionVar(wp, WEBS_SESSION_USERNAME, 0)) != 0) {
            cached = 1;
            wfree(wp->username);
            wp->username = sclone(username);
        }
    }
    if (!cached) {
        if (wp->authType && !smatch(wp->authType, route->authType)) {
            websError(wp, HTTP_CODE_UNAUTHORIZED, "Access denied. Wrong authentication protocol type.");
            return 0;
        }
        if (wp->authDetails && route->parseAuth) {
            if (!(route->parseAuth)(wp)) {
                wp->username = 0;
            }
        }
        if (!wp->username || !*wp->username) {
            if (route->askLogin) {
                (route->askLogin)(wp);
            }
            websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
            return 0;
        }
        if (!(route->verify)(wp)) {
            if (route->askLogin) {
                (route->askLogin)(wp);
            }
            websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
            return 0;
        }
        /*
            Store authentication state and user in session storage
         */
        if (websGetSession(wp, 1) != 0) {
            websSetSessionVar(wp, WEBS_SESSION_USERNAME, wp->username);
        }
    }
    return 1;
}


PUBLIC int websOpenAuth(int minimal)
{
    char    sbuf[64];

    assert(minimal == 0 || minimal == 1);

    if ((users = hashCreate(-1)) < 0) {
        return -1;
    }
    if ((roles = hashCreate(-1)) < 0) {
        return -1;
    }
    if (!minimal) {
        fmt(sbuf, sizeof(sbuf), "%x:%x", rand(), time(0));
        masterSecret = websMD5(sbuf);
        websDefineAction("login", loginServiceProc);
        websDefineAction("logout", logoutServiceProc);
    }
    if (smatch(ME_GOAHEAD_AUTH_STORE, "file")) {
        verifyPassword = websVerifyPasswordFromFile;
#if ME_COMPILER_HAS_PAM
    } else if (smatch(ME_GOAHEAD_AUTH_STORE, "pam")) {
        verifyPassword = websVerifyPasswordFromPam;
#endif
    }
    return 0;
}


PUBLIC void websCloseAuth(void)
{
    WebsKey     *key, *next;

    wfree(masterSecret);
    if (users >= 0) {
        for (key = hashFirst(users); key; key = next) {
            next = hashNext(users, key);
            freeUser(key->content.value.symbol);
        }
        hashFree(users);
        users = -1;
    }
    if (roles >= 0) {
        for (key = hashFirst(roles); key; key = next) {
            next = hashNext(roles, key);
            freeRole(key->content.value.symbol);
        }
        hashFree(roles);
        roles = -1;
    }
}


#if KEEP
PUBLIC int websWriteAuthFile(char *path)
{
    FILE        *fp;
    WebsKey     *kp, *ap;
    WebsRole    *role;
    WebsUser    *user;
    char        *tempFile;

    assert(path && *path);

    tempFile = websTempFile(NULL, NULL);
    if ((fp = fopen(tempFile, "w" FILE_TEXT)) == 0) {
        error("Cannot open %s", tempFile);
        wfree(tempFile);
        return -1;
    }
    fprintf(fp, "#\n#   %s - Authorization data\n#\n\n", basename(path));

    if (roles >= 0) {
        for (kp = hashFirst(roles); kp; kp = hashNext(roles, kp)) {
            role = kp->content.value.symbol;
            fprintf(fp, "role name=%s abilities=", kp->name.value.string);
            for (ap = hashFirst(role->abilities); ap; ap = hashNext(role->abilities, ap)) {
                fprintf(fp, "%s,", ap->name.value.string);
            }
            fputc('\n', fp);
        }
        fputc('\n', fp);
    }
    if (users >= 0) {
        for (kp = hashFirst(users); kp; kp = hashNext(users, kp)) {
            user = kp->content.value.symbol;
            fprintf(fp, "user name=%s password=%s roles=%s", user->name, user->password, user->roles);
            fputc('\n', fp);
        }
    }
    fclose(fp);
    unlink(path);
    if (rename(tempFile, path) < 0) {
        error("Cannot create new %s", path);
        wfree(tempFile);
        return -1;
    }
    wfree(tempFile);
    return 0;
}
#endif


static WebsUser *createUser(cchar *username, cchar *password, cchar *roles)
{
    WebsUser    *user;

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


WebsUser *websAddUser(cchar *username, cchar *password, cchar *roles)
{
    WebsUser    *user;

    if (!username) {
        error("User is missing name");
        return 0;
    }
    if (websLookupUser(username)) {
        error("User %s already exists", username);
        /* Already exists */
        return 0;
    }
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
    WebsKey     *key;

    assert(username);
    if ((key = hashLookup(users, username)) != 0) {
        freeUser(key->content.value.symbol);
    }
    return hashDelete(users, username);
}


static void freeUser(WebsUser *up)
{
    assert(up);
    hashFree(up->abilities);
    wfree(up->name);
    wfree(up->password);
    wfree(up->roles);
    wfree(up);
}


PUBLIC int websSetUserPassword(cchar *username, cchar *password)
{
    WebsUser    *user;

    assert(username);
    if ((user = websLookupUser(username)) == 0) {
        return -1;
    }
    wfree(user->password);
    user->password = sclone(password);
    return 0;
}


PUBLIC int websSetUserRoles(cchar *username, cchar *roles)
{
    WebsUser    *user;

    assert(username);
    if ((user = websLookupUser(username)) == 0) {
        return -1;
    }
    wfree(user->roles);
    user->roles = sclone(roles);
    computeUserAbilities(user);
    return 0;
}


WebsUser *websLookupUser(cchar *username)
{
    WebsKey     *key;

    assert(username);
    if ((key = hashLookup(users, username)) == 0) {
        return 0;
    }
    return (WebsUser*) key->content.value.symbol;
}


static void computeAbilities(WebsHash abilities, cchar *role, int depth)
{
    WebsRole    *rp;
    WebsKey     *key;

    assert(abilities >= 0);
    assert(role && *role);
    assert(depth >= 0);

    if (depth > 20) {
        error("Recursive ability definition for %s", role);
        return;
    }
    if (roles >= 0) {
        if ((key = hashLookup(roles, role)) != 0) {
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
    char    *ability, *roles, *tok;

    assert(user);
    if ((user->abilities = hashCreate(-1)) == 0) {
        return;
    }
    roles = sclone(user->roles);
    for (ability = stok(roles, " \t,", &tok); ability; ability = stok(NULL, " \t,", &tok)) {
        computeAbilities(user->abilities, ability, 0);
    }
#if ME_DEBUG
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
    WebsUser    *user;
    WebsKey     *sym;

    if (users) {
        for (sym = hashFirst(users); sym; sym = hashNext(users, sym)) {
            user = (WebsUser*) sym->content.value.symbol;
            computeUserAbilities(user);
        }
    }
}


WebsRole *websAddRole(cchar *name, WebsHash abilities)
{
    WebsRole    *rp;

    if (!name) {
        error("Role is missing name");
        return 0;
    }
    if (hashLookup(roles, name)) {
        error("Role %s already exists", name);
        /* Already exists */
        return 0;
    }
    if ((rp = walloc(sizeof(WebsRole))) == 0) {
        return 0;
    }
    rp->abilities = abilities;
    if (hashEnter(roles, name, valueSymbol(rp), 0) == 0) {
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
    WebsRole    *rp;
    WebsKey     *sym;

    assert(name && *name);
    if (roles) {
        if ((sym = hashLookup(roles, name)) == 0) {
            return -1;
        }
        rp = sym->content.value.symbol;
        hashFree(rp->abilities);
        wfree(rp);
        return hashDelete(roles, name);
    }
    return -1;
}


PUBLIC WebsHash websGetUsers(void)
{
    return users;
}


PUBLIC WebsHash websGetRoles(void)
{
    return roles;
}


PUBLIC bool websLoginUser(Webs *wp, cchar *username, cchar *password)
{
    assert(wp);
    assert(wp->route);
    assert(username);
    assert(password);

    if (!wp->route || !wp->route->verify) {
        return 0;
    }
    wfree(wp->username);
    wp->username = sclone(username);
    wfree(wp->password);
    wp->password = sclone(password);

    if (!(wp->route->verify)(wp)) {
        trace(2, "Password does not match");
        return 0;
    }
    trace(2, "Login successful for %s", username);
    websCreateSession(wp);
    websSetSessionVar(wp, WEBS_SESSION_USERNAME, wp->username);
    return 1;
}


PUBLIC bool websLogoutUser(Webs *wp)
{
    assert(wp);
    websRemoveSessionVar(wp, WEBS_SESSION_USERNAME);
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
static void loginServiceProc(Webs *wp)
{
    WebsRoute   *route;

    assert(wp);
    route = wp->route;
    assert(route);

    if (websLoginUser(wp, websGetVar(wp, "username", ""), websGetVar(wp, "password", ""))) {
        /* If the application defines a referrer session var, redirect to that */
        cchar *referrer;
        if ((referrer = websGetSessionVar(wp, "referrer", 0)) != 0) {
            websRedirect(wp, referrer);
        } else {
            websRedirectByStatus(wp, HTTP_CODE_OK);
        }
        websSetSessionVar(wp, "loginStatus", "ok");
    } else {
        if (route->askLogin) {
            (route->askLogin)(wp);
        }
        websSetSessionVar(wp, "loginStatus", "failed");
        websRedirectByStatus(wp, HTTP_CODE_UNAUTHORIZED);
    }
}


static void logoutServiceProc(Webs *wp)
{
    websLogoutUser(wp);
}


static void basicLogin(Webs *wp)
{
    assert(wp);
    assert(wp->route);
    wfree(wp->authResponse);
    wp->authResponse = sfmt("Basic realm=\"%s\"", ME_GOAHEAD_REALM);
}


void websSetPasswordStoreVerify(WebsVerify verify)
{
    verifyPassword = verify;
}


WebsVerify websGetPasswordStoreVerify(void)
{
    return verifyPassword;
}


PUBLIC bool websVerifyPasswordFromFile(Webs *wp)
{
    char    passbuf[ME_GOAHEAD_LIMIT_PASSWORD * 3 + 3];
    bool    success;

    assert(wp);
    if (!wp->user && (wp->user = websLookupUser(wp->username)) == 0) {
        trace(5, "verifyUser: Unknown user \"%s\"", wp->username);
        return 0;
    }
    /*
        Verify the password. If using Digest auth, we compare the digest of the password.
        Otherwise we encode the plain-text password and compare that
     */
    if (!wp->encoded) {
        fmt(passbuf, sizeof(passbuf), "%s:%s:%s", wp->username, ME_GOAHEAD_REALM, wp->password);
        wfree(wp->password);
        wp->password = websMD5(passbuf);
        wp->encoded = 1;
    }
    if (wp->digest) {
        success = pmatch(wp->password, wp->digest);
    } else {
        success = pmatch(wp->password, wp->user->password);
    }
    if (success) {
        trace(5, "User \"%s\" authenticated", wp->username);
    } else {
        trace(5, "Password for user \"%s\" failed to authenticate", wp->username);
    }
    return success;
}


/*
    Constant time password match
*/
static bool pmatch(cchar *s1, cchar *s2)
{
    ssize   i, len1, len2;
    uchar   c;

    len1 = slen(s1);
    len2 = slen(s2);
    if (len1 != len2) {
        return 0;
    }
    for (i = 0, c = 0; i < len1; i++) {
        c |= (uchar) s1[i] ^ (uchar) s2[i];
    }
    return !c;
}


#if ME_COMPILER_HAS_PAM
/*
    Copy this routine if creating your own custom password store back end
 */
PUBLIC bool websVerifyPasswordFromPam(Webs *wp)
{
    WebsBuf             abilities;
    pam_handle_t        *pamh;
    UserInfo            info;
    struct pam_conv     conv = { pamChat, &info };
    struct group        *gp;
    int                 res, i;

    assert(wp);
    assert(wp->username && wp->username);
    assert(wp->password);
    assert(!wp->encoded);

    info.name = (char*) wp->username;
    info.password = (char*) wp->password;
    pamh = NULL;
    if ((res = pam_start("login", info.name, &conv, &pamh)) != PAM_SUCCESS) {
        return 0;
    }
    if ((res = pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK)) != PAM_SUCCESS) {
        pam_end(pamh, PAM_SUCCESS);
        trace(5, "httpPamVerifyUser failed to verify %s", wp->username);
        return 0;
    }
    pam_end(pamh, PAM_SUCCESS);
    trace(5, "httpPamVerifyUser verified %s", wp->username);

    if (!wp->user) {
        wp->user = websLookupUser(wp->username);
    }
    if (!wp->user) {
        Gid     groups[32];
        int     ngroups;
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
            if ((wp->user = websAddUser(wp->username, 0, abilities.servp)) == 0) {
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
    UserInfo                *info;
    struct pam_response     *reply;
    int                     i;

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
        case PAM_PROMPT_ECHO_ON:
            reply[i].resp = sclone(info->name);
            break;

        case PAM_PROMPT_ECHO_OFF:
            /* Retrieve the user password and pass onto pam */
            reply[i].resp = sclone(info->password);
            break;

        default:
            free(reply);
            return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}
#endif /* ME_COMPILER_HAS_PAM */


static bool parseBasicDetails(Webs *wp)
{
    char    *cp, *userAuth;

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
    wfree(wp->username);
    wfree(wp->password);
    if (cp) {
        wp->username = sclone(userAuth);
        wp->password = sclone(cp);
        wp->encoded = 0;
    } else {
        wp->username = sclone("");
        wp->password = sclone("");
    }
    wfree(userAuth);
    return 1;
}


#if ME_GOAHEAD_DIGEST
static void digestLogin(Webs *wp)
{
    char  *nonce, *opaque;

    assert(wp);
    assert(wp->route);

    nonce = createDigestNonce(wp);
    /* Opaque is unused. Set to anything */
    opaque = "5ccc069c403ebaf9f0171e9517f40e41";
    wfree(wp->authResponse);
    wp->authResponse = sfmt(
        "Digest realm=\"%s\", domain=\"%s\", qop=\"%s\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\", stale=\"%s\"",
        ME_GOAHEAD_REALM, websGetServerUrl(), "auth", nonce, opaque, "MD5", "FALSE");
    wfree(nonce);
}


static bool parseDigestDetails(Webs *wp)
{
    WebsTime    when;
    char        *decoded, *value, *tok, *key, *keyBuf, *dp, *sp, *secret, *realm;
    int         seenComma;

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
            value = ++tok;
            while (*tok != '\"' && *tok != '\0') {
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
                if (*sp == '\\') {
                    sp++;
                }
                *dp++ = *sp++;
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
                wfree(wp->cnonce);
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
                wfree(wp->nc);
                wp->nc = sclone(value);
            } else if (scaselesscmp(key, "nonce") == 0) {
                wfree(wp->nonce);
                wp->nonce = sclone(value);
            }
            break;

        case 'o':
            if (scaselesscmp(key, "opaque") == 0) {
                wfree(wp->opaque);
                wp->opaque = sclone(value);
            }
            break;

        case 'q':
            if (scaselesscmp(key, "qop") == 0) {
                wfree(wp->qop);
                wp->qop = sclone(value);
            }
            break;

        case 'r':
            if (scaselesscmp(key, "realm") == 0) {
                wfree(wp->realm);
                wp->realm = sclone(value);
            } else if (scaselesscmp(key, "response") == 0) {
                /*
                    Store the response digest in the password field. This is MD5(HA1:nonce:HA2) where
                    HA1 is MD5(user:realm:password) and HA2 is MD5(method:digestUri)
                 */
                wfree(wp->password);
                wp->password = sclone(value);
                wp->encoded = 1;
            }
            break;

        case 's':
            if (scaselesscmp(key, "stale") == 0) {
                break;
            }

        case 'u':
            if (scaselesscmp(key, "uri") == 0) {
                wfree(wp->digestUri);
                wp->digestUri = sclone(value);
            } else if (scaselesscmp(key, "username") == 0 || scaselesscmp(key, "user") == 0) {
                wfree(wp->username);
                wp->username = sclone(value);
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

    if (wp->username == 0 || wp->realm == 0 || wp->nonce == 0 || wp->route == 0 || wp->password == 0) {
        return 0;
    }
    if (wp->qop && (wp->cnonce == 0 || wp->nc == 0)) {
        return 0;
    }
    if (wp->qop == 0) {
        wp->qop = sclone("");
    }
    /*
        Validate the nonce value - prevents replay attacks
     */
    when = 0; secret = 0; realm = 0;
    decoded = parseDigestNonce(wp->nonce, &secret, &realm, &when);
    if (!smatch(masterSecret, secret)) {
        trace(2, "Access denied: Nonce mismatch");
        wfree(decoded);
        return 0;
    } else if (!smatch(realm, ME_GOAHEAD_REALM)) {
        trace(2, "Access denied: Realm mismatch");
        wfree(decoded);
        return 0;
    } else if (!smatch(wp->qop, "auth")) {
        trace(2, "Access denied: Bad qop");
        wfree(decoded);
        return 0;
    } else if ((when + ME_GOAHEAD_NONCE_DURATION) < time(0)) {
        trace(2, "Access denied: Nonce is stale");
        wfree(decoded);
        return 0;
    }
    if (!wp->user) {
        if ((wp->user = websLookupUser(wp->username)) == 0) {
            trace(2, "Access denied: user is unknown");
            wfree(decoded);
            return 0;
        }
    }
    wfree(decoded);
    wfree(wp->digest);
    wp->digest = calcDigest(wp, 0, wp->user->password);
    return 1;
}


/*
    Create a nonce value for digest authentication (RFC 2617)
 */
static char *createDigestNonce(Webs *wp)
{
    static int64 next = 0;
    char         nonce[256];

    assert(wp);
    assert(wp->route);

    fmt(nonce, sizeof(nonce), "%s:%s:%x:%x", masterSecret, ME_GOAHEAD_REALM, time(0), next++);
    return websEncode64(nonce);
}


static char *parseDigestNonce(char *nonce, char **secret, char **realm, WebsTime *when)
{
    char    *tok, *decoded, *whenStr;

    assert(nonce && *nonce);
    assert(secret);
    assert(realm);
    assert(when);

    if ((decoded = websDecode64(nonce)) == 0) {
        return 0;
    }
    *secret = stok(decoded, ":", &tok);
    *realm = stok(NULL, ":", &tok);
    whenStr = stok(NULL, ":", &tok);
    *when = hextoi(whenStr);
    return decoded;
}


/*
   Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
 */
static char *calcDigest(Webs *wp, char *username, char *password)
{
    char  a1Buf[256], a2Buf[256], digestBuf[256];
    char  *ha1, *ha2, *method, *result;

    assert(wp);
    assert(password);

    /*
        Compute HA1. If username == 0, then the password is already expected to be in the HA1 format
        (MD5(username:realm:password).
     */
    if (username == 0) {
        ha1 = sclone(password);
    } else {
        fmt(a1Buf, sizeof(a1Buf), "%s:%s:%s", username, wp->realm, password);
        ha1 = websMD5(a1Buf);
    }

    /*
        HA2
     */
    method = wp->method;
    fmt(a2Buf, sizeof(a2Buf), "%s:%s", method, wp->digestUri);
    ha2 = websMD5(a2Buf);

    /*
        H(HA1:nonce:HA2)
     */
    if (scmp(wp->qop, "auth") == 0) {
        fmt(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, wp->nonce, wp->nc, wp->cnonce, wp->qop, ha2);

    } else if (scmp(wp->qop, "auth-int") == 0) {
        fmt(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, wp->nonce, wp->nc, wp->cnonce, wp->qop, ha2);

    } else {
        fmt(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, wp->nonce, ha2);
    }
    result = websMD5(digestBuf);
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
        askLogin = digestLogin;
        parseAuth = parseDigestDetails;
#endif
    } else {
        auth = 0;
    }
    route->authType = sclone(auth);
    route->askLogin = askLogin;
    route->parseAuth = parseAuth;
    return 0;
}
#endif /* ME_GOAHEAD_AUTH */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
