/*
    goaheadLib.c -- GoAhead Library Source

    This file is a catenation of all the source code. Amalgamating into a
    single file makes embedding simpler and the resulting application faster,
    by using compiler optimization within the GoAhead library.

    Prepared by: buildLib.sh
 */

#include "goahead.h"

#if ME_COM_GOAHEAD


/********* Start of file src/action.c ************/

/*
    action.c -- Action handler

    This module implements the /action handler. It is a simple binding of URIs to C functions.
    This enables a very high performance implementation with easy parsing and decoding of query
    strings and posted data.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/************************************ Locals **********************************/

static WebsHash actionTable = -1;            /* Symbol table for actions */

/************************************* Code ***********************************/
/*
    Return a copy of the action name, the second segment of the path. The caller must free it.
 */
static char *getActionName(Webs *wp)
{
    char *name, *end;

    if ((name = strchr(&wp->path[1], '/')) == NULL) {
        return NULL;
    }
    name++;
    if ((end = strchr(name, '/')) == NULL) {
        end = name + slen(name);
    }
    return snclone(name, end - name);
}


/*
    Decline actions outside this route so later routes enforce their own policies.
 */
static bool actionMatch(Webs *wp)
{
    char *name;
    bool served;

    if ((name = getActionName(wp)) == NULL) {
        return 1;
    }
    served = websRouteServesAction(wp->route, name);
    wfree(name);
    return served;
}


/*
    Process an action request. Returns 1 always to indicate it handled the URL
    Return true to indicate the request was handled, even for errors.
 */
static bool actionHandler(Webs *wp)
{
    WebsKey    *sp;
    char       *actionName;
    WebsAction fn;

#if ME_GOAHEAD_LEGACY
    WebsProc proc;
#endif

    assert(websValid(wp));
    assert(actionTable >= 0);

    if ((actionName = getActionName(wp)) == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Missing action name");
        return 1;
    }
    /*
        Default state-changing actions to POST.
     */
    if (wp->route && wp->route->methods < 0 && !smatch(wp->method, "POST")) {
        websSetStatus(wp, HTTP_CODE_BAD_METHOD);
        websWriteHeaders(wp, 0, 0);
        websWriteHeader(wp, "Allow", "POST");
        websWriteEndHeaders(wp);
        websDone(wp);
        wfree(actionName);
        return 1;
    }
    sp = hashLookup(actionTable, actionName);
    if (sp == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Action %s is not defined", actionName);
    } else {
        /*
            Routes choose methods; application handlers must validate CSRF tokens.
            The key's arg records whether the action was defined with the legacy signature.
         */
#if ME_GOAHEAD_LEGACY
        if (sp->arg) {
            proc = (WebsProc) sp->content.value.symbol;
            (*proc)(wp, actionName, wp->query);
            return 1;
        }
#endif
        fn = (WebsAction) sp->content.value.symbol;
        (*fn)(wp);
    }
    wfree(actionName);
    return 1;
}


static int defineAction(cchar *name, void *fn, int legacy)
{
    assert(name && *name);
    assert(fn);

    if (fn == NULL) {
        return -1;
    }
    hashEnter(actionTable, name, valueSymbol(fn), legacy);
    return 0;
}


/*
    Define a function in the "action" map space. Legacy builds call it with the WebsProc signature.
 */
PUBLIC int websDefineAction(cchar *name, void *fn)
{
    return defineAction(name, fn, ME_GOAHEAD_LEGACY);
}


PUBLIC int websDefineNativeAction(cchar *name, WebsAction fn)
{
    return defineAction(name, fn, 0);
}


static void closeAction(void)
{
    if (actionTable != -1) {
        hashFree(actionTable);
        actionTable = -1;
    }
}


PUBLIC void websActionOpen(void)
{
    actionTable = hashCreate(WEBS_HASH_INIT);
    websDefineHandler("action", actionMatch, actionHandler, closeAction, 0);
}


#if ME_GOAHEAD_LEGACY
/*
    Don't use these routines. Use websWriteHeaders, websWriteEndHeaders instead.

    Write a webs header. This is a convenience routine to write a common header for an action back to the browser.
 */
PUBLIC void websHeader(Webs *wp)
{
    assert(websValid(wp));

    websWriteHeaders(wp, -1, 0);
    websWriteEndHeaders(wp);
    websWrite(wp, "<html>\n");
}


PUBLIC void websFooter(Webs *wp)
{
    assert(websValid(wp));
    websWrite(wp, "</html>\n");
}
#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/alloc.c ************/

/*
    alloc.c -- Optional WebServer memory allocator

    This file implements a fast block allocation scheme suitable for operating systems whose malloc suffers from
    fragmentation.  This allocator maintains block class queues for rapid allocation and minimal fragmentation. This
    allocator does not coalesce blocks. The storage space may be populated statically or via the traditional malloc
    mechanisms. Large blocks greater than the maximum class size may be allocated from the O/S or run-time system via
    malloc. To permit the use of malloc, call wopenAlloc with flags set to WEBS_USE_MALLOC (this is the default).  It is
    recommended that wopenAlloc be called first thing in the application.  If it is not, it will be called with default
    values on the first call to walloc(). Note that this code is not designed for multi-threading purposes and it
    depends on newly declared variables being initialized to zero.

    An allocation failure calls the memory notifier and then aborts the process, so callers never receive NULL. The
    notifier may report the failure but cannot recover from it. You must set WEBS_DEFAULT_MEM to be sufficient memory
    for your needs.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************** Locals ************************************/

static WebsMemNotifier memNotifier;

PUBLIC void websSetMemNotifier(WebsMemNotifier cback)
{
    memNotifier = cback;
}


static void defaultMemNotifier(ssize size)
{
    fprintf(stderr, "Cannot allocate required block of %lld bytes, terminating.\n", (long long) size);
}


/*
    Abort after notifying: callers rely on walloc never returning NULL.
 */
static void memFailure(ssize size)
{
    if (memNotifier) {
        (memNotifier) (size);
    } else {
        defaultMemNotifier(size);
    }
    abort();
}

/********************************* Defines ************************************/
#if ME_GOAHEAD_REPLACE_MALLOC
/*
    ROUNDUP4(size) returns the next higher integer value of size that is divisible by 4, or the value of size if size is
    divisible by 4. ROUNDUP4() is used in aligning memory allocations on 4-byte boundaries.
    Note:  ROUNDUP4() is only required on some operating systems.
 */
#define ROUNDUP4(size) ((size) % 4) ? (size) + (4 - ((size) % 4)) : (size)

/*
    qhead blocks are created as the original memory allocation is freed up. See wfree.
 */
static WebsAlloc *qhead[WEBS_MAX_CLASS];                /* Per class block q head */
static char      *freeBuf;                              /* Pointer to free memory */
static char      *freeNext;                             /* Pointer to next free mem */
static int       freeSize;                              /* Size of free memory */
static int       freeLeft;                              /* Size of free left for use */
static int       controlFlags = WEBS_USE_MALLOC;        /* Default to auto-malloc */
static int       wopenCount = 0;                        /* Num tasks using walloc */

static int wallocGetSize(ssize size, int *q);

#define WEBS_MAX_BLOCK (1 << 30)

#endif /* ME_GOAHEAD_REPLACE_MALLOC */

/********************************** Code **************************************/
/*
    Initialize the walloc module. wopenAlloc should be called the very first thing after the application starts and
    wcloseAlloc should be called the last thing before exiting. If wopenAlloc is not called, it will be called on the
       first
    allocation with default values. "buf" points to memory to use of size "bufsize". If buf is NULL, memory is allocated
    using malloc. flags may be set to WEBS_USE_MALLOC if using malloc is okay. This routine will allocate *  an initial
    buffer of size bufsize for use by the application.
 */
PUBLIC int wopenAlloc(void *buf, int bufsize, int flags)
{
#if ME_GOAHEAD_REPLACE_MALLOC
    controlFlags = flags;

    /*
        If wopenAlloc already called by a shared process, just increment the count and return
     */
    if (++wopenCount > 1) {
        return 0;
    }
    if (buf == NULL) {
        if (bufsize == 0) {
            bufsize = WEBS_DEFAULT_MEM;
        }
        bufsize = ROUNDUP4(bufsize);
        if ((buf = malloc(bufsize)) == NULL) {
            /*
                Resetting wopenCount so client code can decide to call wopenAlloc() again with a smaller memory request.
             */
            --wopenCount;
            return -1;
        }
    } else {
        controlFlags |= WEBS_USER_BUF;
    }
    freeSize = freeLeft = bufsize;
    freeBuf = freeNext = buf;
    memset(qhead, 0, sizeof(qhead));

#endif /* ME_GOAHEAD_REPLACE_MALLOC */
    return 0;
}


PUBLIC void wcloseAlloc(void)
{
#if ME_GOAHEAD_REPLACE_MALLOC
    if (--wopenCount <= 0 && !(controlFlags & WEBS_USER_BUF)) {
        free(freeBuf);
        wopenCount = 0;
    }
#endif /* ME_GOAHEAD_REPLACE_MALLOC */
}


#if ME_GOAHEAD_REPLACE_MALLOC
/*
    Allocate a block of the requested size. First check the block queues for a suitable one.
 */
PUBLIC void *walloc(ssize size)
{
    WebsAlloc *bp;
    int       q, memSize;

    /*
        Call wopenAlloc with default values if the application has not yet done so
     */
    if (freeBuf == NULL) {
        if (wopenAlloc(NULL, WEBS_DEFAULT_MEM, WEBS_USE_MALLOC) < 0) {
            memFailure(WEBS_DEFAULT_MEM);
            return NULL;
        }
    }
    if (size < 0) {
        memFailure(size);
        return NULL;
    }
    if (size > WEBS_MAX_BLOCK) {
        memFailure(size);
        return NULL;
    }
    memSize = wallocGetSize(size, &q);

    if (q >= WEBS_MAX_CLASS) {
        /*
            Size if bigger than the maximum class. Malloc if use has been okayed
         */
        if (controlFlags & WEBS_USE_MALLOC) {
            memSize = ROUNDUP4(memSize);
            bp = (WebsAlloc*) malloc(memSize);
            if (bp == NULL) {
                memFailure(memSize);
                return NULL;
            }
        } else {
            memFailure(memSize);
            return NULL;
        }
        /*
            the u.size is the actual size allocated for data
         */
        bp->u.size = memSize - sizeof(WebsAlloc);
        bp->flags = WEBS_MALLOCED;

    } else if ((bp = qhead[q]) != NULL) {
        /*
            Take first block off the relevant q if non-empty
         */
        qhead[q] = bp->u.next;
        bp->u.size = memSize - sizeof(WebsAlloc);
        bp->flags = 0;

    } else {
        if (freeLeft > memSize) {
            /*
                The q was empty, and the free list has spare memory so create a new block out of the primary free block
             */
            bp = (WebsAlloc*) freeNext;
            freeNext += memSize;
            freeLeft -= memSize;
            bp->u.size = memSize - sizeof(WebsAlloc);
            bp->flags = 0;

        } else if (controlFlags & WEBS_USE_MALLOC) {
            /*
                Nothing left on the primary free list, so malloc a new block
             */
            memSize = ROUNDUP4(memSize);
            if ((bp = (WebsAlloc*) malloc(memSize)) == NULL) {
                memFailure(memSize);
                return NULL;
            }
            bp->u.size = memSize - sizeof(WebsAlloc);
            bp->flags = WEBS_MALLOCED;

        } else {
            memFailure(memSize);
            return NULL;
        }
    }
    bp->flags |= WEBS_INTEGRITY;
    return (void*) ((char*) bp + sizeof(WebsAlloc));
}


/*
    Free a block back to the relevant free q. We don't free back to the O/S or run time system unless the block is
    greater than the maximum class size. We also do not coalesce blocks.
 */
PUBLIC void wfree(void *mp)
{
    WebsAlloc *bp;
    int       q;

    if (mp == 0) {
        return;
    }
    bp = (WebsAlloc*) ((char*) mp - sizeof(WebsAlloc));
    assert((bp->flags & WEBS_INTEGRITY_MASK) == WEBS_INTEGRITY);
    if ((bp->flags & WEBS_INTEGRITY_MASK) != WEBS_INTEGRITY) {
        return;
    }
    wallocGetSize(bp->u.size, &q);
    if (bp->flags & WEBS_MALLOCED) {
        free(bp);
        return;
    }
    /*
        Simply link onto the head of the relevant q
     */
    bp->u.next = qhead[q];
    qhead[q] = bp;
    bp->flags = WEBS_FILL_WORD;
}


/*
    Reallocate; NULL allocates a new block. Failure aborts.
 */
PUBLIC void *wrealloc(void *mp, ssize newsize)
{
    WebsAlloc *bp;
    void      *newbuf;

    if (mp == NULL) {
        return walloc(newsize);
    }
    bp = (WebsAlloc*) ((char*) mp - sizeof(WebsAlloc));
    assert((bp->flags & WEBS_INTEGRITY_MASK) == WEBS_INTEGRITY);

    /*
        If the allocated memory already has enough room just return the previously allocated address.
     */
    if (bp->u.size >= newsize) {
        return mp;
    }
    if ((newbuf = walloc(newsize)) != NULL) {
        memcpy(newbuf, mp, bp->u.size);
        wfree(mp);
    }
    return newbuf;
}


/*
    Find the size of the block to be walloc'ed.  It takes in a size, finds the smallest binary block it fits into, adds
    an overhead amount and returns.  q is the binary size used to keep track of block sizes in use.  Called from both
    walloc and wfree.
 */
static int wallocGetSize(ssize size, int *q)
{
    ssize mask;

    mask = (size == 0) ? 0 : (size - 1) >> WEBS_SHIFT;
    for (*q = 0; mask; mask >>= 1) {
        *q = *q + 1;
    }
    return (1 << (WEBS_SHIFT + *q)) + sizeof(WebsAlloc);
}


PUBLIC void wvalid(cvoid *mp)
{
    WebsAlloc *bp;

    bp = (WebsAlloc*) ((char*) mp - sizeof(WebsAlloc));
    assert(bp->flags & WEBS_INTEGRITY);
}

#else /* !ME_GOAHEAD_REPLACE_MALLOC */

PUBLIC void *walloc(ssize num)
{
    void *mem;

    if ((mem = malloc(num)) == 0 && num != 0) {
        memFailure(num);
    }
    return mem;
}


PUBLIC void wfree(void *mem)
{
    if (mem) {
        free(mem);
    }
}


PUBLIC void *wrealloc(void *mem, ssize num)
{
    void *grown;

    if ((grown = realloc(mem, num)) == 0 && num != 0) {
        memFailure(num);
    }
    return grown;
}
#endif /* ME_GOAHEAD_REPLACE_MALLOC */


PUBLIC void *wdup(cvoid *ptr, ssize usize)
{
    char *newp;

    if ((newp = walloc(usize)) != 0) {
        memcpy(newp, ptr, usize);
    }
    return newp;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/auth.c ************/

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


/********* Start of file src/cgi.c ************/

/*
    cgi.c -- CGI processing

    This module implements the /cgi-bin handler. CGI processing differs from
    goforms processing in that each CGI request is executed as a separate
    process, rather than within the webserver process. For each CGI request the
    environment of the new process must be set to include all the CGI variables
    and its standard input and output must be directed to the socket.  This
    is done using temporary files.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/
#if ME_GOAHEAD_CGI

#if ME_WIN_LIKE
typedef HANDLE CgiPid;
#elif VXWORKS && (_WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 9))
/* A task ID holds a pointer on 64-bit VxWorks. (CgiPid) -1 is TASK_ID_ERROR */
typedef TASK_ID CgiPid;
#else
typedef pid_t CgiPid;
#endif

#if WINDOWS
    #define CGI_OUTPUT_GRACE 1000   /* Msec to wait for the output of an exited CGI to appear */
#endif

typedef struct Cgi {            /* Struct for CGI tasks which have completed */
    Webs *wp;                   /* Connection object */
    char *stdIn;                /* File desc. for task's temp input fd */
    char *stdOut;               /* File desc. for task's temp output fd */
    char *cgiPath;              /* Path to executable process file */
    char *query;                /* Backing store for the argp strings */
    char **argp;                /* Pointer to buf containing argv tokens */
    char **envp;                /* Pointer to array of environment strings */
    CgiPid handle;              /* Process handle of the task */
    off_t fplacemark;           /* Seek location for CGI output file */
    ssize txLen;                /* Content length the CGI declared, or -1 for chunked framing */
    ssize txBytes;              /* Body bytes forwarded to the client against the declared length */
    ssize txExcess;             /* Body bytes dropped beyond the declared content length */
    bool txSuppress;            /* Response carries no body at all, whatever the CGI writes */
#if WINDOWS
    uint64 outputDeadline;      /* Tick after which an exited CGI that wrote nothing has no output coming */
#endif
#if VXWORKS
    char *vxName;               /* Unique task name, so a reused task ID is not mistaken for this task */
    int fdin;                   /* Task stdin, owned by the server so an aborted task cannot leak it */
    int fdout;                  /* Task stdout, owned by the server */
    struct CgiModule *module;   /* Loaded module the task executes, or NULL for an entry linked into the image */
#endif
} Cgi;

static Cgi **cgiList;           /* walloc chain list of wp's to be closed */
static int cgiMax;              /* Size of walloc list */

/*
    Prefix client environment variables with CGI_ and deny execution-control names. Windows names compare case-
    insensitively.
 */
static cchar *envBlackList[] = {
    "AUTHSTATE",
    "BASHOPTS",
    "BASH_ENV",
    "CDPATH",
    "CLASSPATH",
    "COMSPEC",
    "ENV",
    "FPATH",
    "GEM_HOME",
    "GEM_PATH",
    "GLIBC_TUNABLES",
    "GLOBIGNORE",
    "GOPATH",
    "HOSTALIASES",
    "HTTP_AUTHORIZATION",
    "HTTP_PROXY",
    "IFS",
    "JAVA_HOME",
    "JAVA_TOOL_OPTIONS",
    "LIBPATH",
    "LOCALDOMAIN",
    "LUA_CPATH",
    "LUA_INIT",
    "LUA_PATH",
    "NLSPATH",
    "NODE_OPTIONS",
    "NODE_PATH",
    "NULLCMD",
    "PATH",
    "PATHEXT",
    "PATH_LOCALE",
    "PERL5DB",
    "PERL5LIB",
    "PERL5OPT",
    "PERLIO_DEBUG",
    "PERLLIB",
    "PHPRC",
    "PHP_INI_SCAN_DIR",
    "PS4",
    "PYTHONHOME",
    "PYTHONINSPECT",
    "PYTHONPATH",
    "PYTHONUSERBASE",
    "READNULLCMD",
    "RES_OPTIONS",
    "RUBYLIB",
    "RUBYOPT",
    "SHELLOPTS",
    "SHLIB_PATH",
    "SYSTEMROOT",
    "TERMCAP",
    "TERMINFO",
    "TERMINFO_DIRS",
    "TEMP",
    "TERMPATH",
    "TMP",
    "TMPPREFIX",
    "WINDIR",
    "ZDOTDIR",
    "__COMPAT_LAYER",
    NULL
};

static bool envNameMatch(cchar *name, cchar *other)
{
#if ME_WIN_LIKE
    return scaselessmatch(name, other);
#else
    return smatch(name, other);
#endif
}


static bool envNameStarts(cchar *name, cchar *prefix)
{
#if ME_WIN_LIKE
    return sncaselesscmp(name, prefix, slen(prefix)) == 0;
#else
    return sstarts(name, prefix);
#endif
}


static bool inBlackList(cchar *name)
{
    cchar **bp;

    for (bp = envBlackList; *bp; bp++) {
        if (envNameMatch(name, *bp)) {
            return 1;
        }
    }
    return 0;
}


/*
    Check for names that would alias server environment variables on this platform.
 */
static bool shadowsServerVar(Webs *wp, cchar *name)
{
    WebsKey *s;

    for (s = hashFirst(wp->vars); s; s = hashNext(wp->vars, s)) {
        if (s->arg == 0 && envNameMatch(s->name.value.string, name)) {
            return 1;
        }
    }
    return 0;
}

/************************************ Forwards ********************************/

static void cgiCleanup(Cgi *cgip, int cid);
static int checkCgi(Cgi *cgip);
static void killCgi(Cgi *cgip);
static CgiPid launchCgi(Cgi *cgip, char *dir);
#if VXWORKS
static void releaseVxTask(Cgi *cgip);
#endif

/************************************* Code ***********************************/
/*
    Windows CGI launch helpers. All three are platform-independent C string logic
    (no Windows types or APIs) so the production Windows path and the macOS unit
    tests share a single implementation.
 */

PUBLIC bool isCgiExeOnWindows(cchar *cgiPath)
{
    ssize n;

    if (cgiPath == NULL) {
        return 0;
    }
    n = slen(cgiPath);
    if (n < 4) {
        return 0;
    }
    return scaselessmatch(cgiPath + n - 4, ".exe");
}


PUBLIC ssize appendWinArg(char *dp, ssize destMax, cchar *arg)
{
    cchar *cp;
    char  *start, *end;
    ssize len;
    int   bs;

    if (dp == NULL) {
        return 0;
    }
    start = dp;
    end = dp + destMax;
    /*
        Check capacity before each write.
     */
    if (arg == NULL || *arg == '\0') {
        /*
            Empty arg must be represented as "" so the receiving CRT recovers a
            zero-length argv element rather than nothing.
         */
        if (end - dp < 2) {
            return -1;
        }
        *dp++ = '"';
        *dp++ = '"';
        return dp - start;
    }
    if (!strpbrk(arg, " \t\n\v\r\f\"")) {
        if ((len = slen(arg)) > end - dp) {
            return -1;
        }
        memcpy(dp, arg, len);
        return len;
    }
    if (end - dp < 1) {
        return -1;
    }
    *dp++ = '"';
    for (cp = arg; *cp; ) {
        bs = 0;
        while (*cp == '\\') {
            bs++;
            cp++;
        }
        if (*cp == '\0') {
            /* Trailing backslash run abuts the closing quote: double it. */
            if (end - dp < 2 * bs) {
                return -1;
            }
            for (; bs > 0; bs--) {
                *dp++ = '\\';
                *dp++ = '\\';
            }
            break;
        } else if (*cp == '"') {
            /* Backslash run before embedded quote: double-and-add-one. */
            if (end - dp < 2 * bs + 2) {
                return -1;
            }
            for (; bs > 0; bs--) {
                *dp++ = '\\';
                *dp++ = '\\';
            }
            *dp++ = '\\';
            *dp++ = '"';
            cp++;
        } else {
            if (end - dp < bs + 1) {
                return -1;
            }
            for (; bs > 0; bs--) {
                *dp++ = '\\';
            }
            *dp++ = *cp++;
        }
    }
    if (end - dp < 1) {
        return -1;
    }
    *dp++ = '"';
    return dp - start;
}


PUBLIC ssize buildWinCmdLine(char **argp, char *out, ssize outMax)
{
    char  **pArgs;
    char  *p, *end;
    ssize n;

    if (argp == NULL || out == NULL) {
        return 0;
    }
    if (outMax < 1) {
        return -1;
    }
    p = out;
    //  Reserve the terminator
    end = out + outMax - 1;
    for (pArgs = argp; *pArgs; pArgs++) {
        if ((n = appendWinArg(p, end - p, *pArgs)) < 0) {
            *out = '\0';
            return -1;
        }
        p += n;
        if (pArgs[1]) {
            if (p >= end) {
                *out = '\0';
                return -1;
            }
            *p++ = ' ';
        }
    }
    *p = '\0';
    return p - out;
}


static bool validEnvName(cchar *name)
{
    cchar *cp;

    if (!name || !(isalpha((uchar) *name) || *name == '_')) {
        return 0;
    }
    for (cp = name + 1; *cp; cp++) {
        if (!(isalnum((uchar) *cp) || *cp == '_')) {
            return 0;
        }
    }
    return 1;
}


static bool hasControlChar(cchar *str)
{
    cchar *cp;

    for (cp = str; *cp; cp++) {
        if (iscntrl((uchar) *cp)) {
            return 1;
        }
    }
    return 0;
}


/*
    ISINDEX terms must start with a letter or digit and contain no controls or decoded =.
 */
static bool validIsindexArg(cchar *arg)
{
    return arg && isalnum((uchar) *arg) && !hasControlChar(arg) && !strchr(arg, '=');
}


/*
    Process a CGI request.
    Return true to indicate the request was handled, even for errors.
 */
/*
    Check the executable's extension, not the last PATH_INFO segment, against the route.
 */
static bool cgiExtensionAllowed(WebsRoute *route, cchar *path)
{
    cchar *base, *ext;
    char  *lower;
    bool  allowed;

    if (route == 0 || route->extensions < 0) {
        return 1;
    }
    base = (base = strrchr(path, '/')) != 0 ? base + 1 : path;
#if ME_WIN_LIKE
    base = strrchr(base, '\\') ? strrchr(base, '\\') + 1 : base;
#endif
    if ((ext = strrchr(base, '.')) == 0 || ext[1] == '\0') {
        return 0;
    }
    lower = slower(sclone(&ext[1]));
    allowed = hashLookup(route->extensions, lower) != 0;
    wfree(lower);
    return allowed;
}


/*
    Split ISINDEX queries only without a literal =, using & for POST and + otherwise.
    The caller owns the returned vector and *backing; element zero borrows cgiPath.
    On failure, free allocations and return NULL with the HTTP status in *status.
 */
static char **buildCgiArgs(cchar *cgiPath, cchar *query, char **backing, int *status)
{
    char **argp, *cp, *tok, *copy;
    int  n, argpsize;

    *backing = 0;
    *status = HTTP_CODE_INTERNAL_SERVER_ERROR;
    argpsize = 10;
    if ((argp = walloc(argpsize * sizeof(char*))) == 0) {
        *status = HTTP_CODE_NOT_FOUND;
        return 0;
    }
    *argp = (char*) cgiPath;
    n = 1;
    copy = 0;

    if (query && strchr(query, '=') == NULL) {
        /*
            RFC 3875 4.4: the search string is split at each '+' and each term is then decoded, so an
            encoded space or '+' stays inside its term.
         */
        copy = sclone(query);
        for (cp = stok(copy, "+", &tok); cp != NULL; ) {
            websDecodeUrl(cp, cp, -1);
            /*
                Require word-like terms to exclude command options, response files, and redirections.
             */
            if (!validIsindexArg(cp)) {
                *status = HTTP_CODE_BAD_REQUEST;
                wfree(argp);
                wfree(copy);
                return 0;
            }
            if (n == 1) {
                /*
                    SECURITY (CWE-88): Precede ISINDEX search terms with a POSIX
                    end-of-options marker so a conformant CGI program treats them
                    as positional operands, never as options. Defence in depth on
                    top of the '-' rejection above.
                 */
                *(argp + n++) = "--";
            }
            *(argp + n++) = cp;
            trace(5, "ARG[%d] %s", n - 1, argp[n - 1]);
            if (n >= argpsize) {
                argpsize *= 2;
                if (argpsize > ME_GOAHEAD_LIMIT_CGI_ARGS) {
                    *status = HTTP_CODE_REQUEST_TOO_LARGE;
                    wfree(argp);
                    wfree(copy);
                    return 0;
                }
                argp = wrealloc(argp, argpsize * sizeof(char*));
            }
            cp = stok(NULL, "+", &tok);
        }
    }
    *(argp + n) = NULL;
    *backing = copy;
    return argp;
}


/*
    Return a caller-owned array of allocated key=value strings, growing as needed.
 */
static char **buildCgiEnv(Webs *wp)
{
    WebsKey *s;
    char    **envp;
    cchar   *name;
    int     n, envpsize;

    envpsize = 64;
    envp = walloc(envpsize * sizeof(char*));
    n = 0;
    if (wp->vars >= 0) {
        for (s = hashFirst(wp->vars); s != NULL; s = hashNext(wp->vars, s)) {
            if (s->content.valid && s->content.type == string) {
                name = s->name.value.string;
                /*
                    Export only portable environment names; delimiters or padding could inject assignments or alias
                    server variables.
                 */
                if (!validEnvName(name)) {
                    trace(5, "cgi: Skip request variable with a non-portable name");
                    continue;
                }
                if (inBlackList(name)) {
                    continue;
                }
                if (envNameStarts(name, "LD_") || envNameStarts(name, "LDR_") || envNameStarts(name, "_RLD") ||
                    envNameStarts(name, "DYLD_")) {
                    continue;
                }
                /*
                    SECURITY (CVE-2014-6271): bash imports a value that begins with "() {" as a
                    function definition. The payload is carried in the value, never the name.
                 */
                if (sstarts(s->content.value.string, "() {")) {
                    continue;
                }
                if (s->arg != 0 && *ME_GOAHEAD_CGI_VAR_PREFIX != '\0') {
                    envp[n++] = sfmt("%s%s=%s", ME_GOAHEAD_CGI_VAR_PREFIX, name, s->content.value.string);
                } else if (s->arg != 0 && shadowsServerVar(wp, name)) {
                    trace(5, "cgi: Skip request variable named like a server variable");
                    continue;
                } else {
                    envp[n++] = sfmt("%s=%s", name, s->content.value.string);
                }
                trace(5, "Env[%d] %s (value omitted)", n, s->name.value.string);
                if (n >= envpsize) {
                    envpsize *= 2;
                    envp = wrealloc(envp, envpsize * sizeof(char*));
                }
            }
        }
    }
    *(envp + n) = NULL;
    return envp;
}


PUBLIC bool cgiHandler(Webs *wp)
{
    Cgi   *cgip;
    char  cgiPrefix[ME_GOAHEAD_LIMIT_FILENAME], *stdIn, *stdOut, cwd[ME_GOAHEAD_LIMIT_FILENAME];
    char  *cp, *cgiName, *cgiPath, **argp, **envp, *query, *dir, *extraPath, *exe;
    ssize len, stdinLen;
    int   status, cid;

    assert(websValid(wp));

    websSetEnv(wp);

    /* Stage buffered input when rerouting after the body has already been read. */
    if (wp->cgiStdin == NULL && bufLen(&wp->input) > 0) {
        if ((wp->cgiStdin = websGetCgiCommName()) == 0 ||
            (wp->cgifd = open(wp->cgiStdin, O_WRONLY | O_BINARY | O_TRUNC | O_NOFOLLOW, 0600)) < 0) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create CGI file");
            return 1;
        }
        websProcessCgiData(wp);
        if (wp->finalized) {
            return 1;
        }
    }
    /*
        Use the bytes sent to stdin for CONTENT_LENGTH; multipart file extraction changes that size.
     */
    stdinLen = 0;
    if (wp->cgifd >= 0) {
        stdinLen = (ssize) lseek(wp->cgifd, 0, SEEK_END);
        close(wp->cgifd);
        wp->cgifd = -1;
    }
    websSetVarFmt(wp, "CONTENT_LENGTH", "%zd", stdinLen > 0 ? stdinLen : 0);

    /*
        Reject overflow before scanning; scopy clears the destination on failure.
     */
    if (scopy(cgiPrefix, sizeof(cgiPrefix), wp->path) < 0) {
        websError(wp, HTTP_CODE_REQUEST_URL_TOO_LARGE, "CGI path too long");
        return 1;
    }
    /*
        Resolve the program from the first segment after the route prefix, inside its directory.
     */
    for (len = slen(wp->route->prefix); len > 0 && wp->route->prefix[len - 1] == '/'; len--) { }
    if (cgiPrefix[len] != '/' || cgiPrefix[len + 1] == '\0') {
        websError(wp, HTTP_CODE_NOT_FOUND, "Missing CGI name");
        return 1;
    }
    cgiPrefix[len] = '\0';
    cgiName = &cgiPrefix[len + 1];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get working directory");
        return 1;
    }
    /*
        Make dir absolute because the program path is resolved against it and the child starts in it.
     */
    dir = wp->route->dir ? websAbsolutePath(wp->route->dir) : sclone(cwd);

    extraPath = 0;
    if ((cp = strchr(cgiName, '/')) != NULL) {
        extraPath = sclone(cp);
        *cp = '\0';
        websSetVar(wp, "PATH_INFO", extraPath);
        websSetVarFmt(wp, "PATH_TRANSLATED", "%s%s%s", dir, cgiPrefix, extraPath);
        wfree(extraPath);
    } else {
        websSetVar(wp, "PATH_INFO", "");
        websSetVar(wp, "PATH_TRANSLATED", "");
    }
    cgiPath = sfmt("%s%s/%s", dir, cgiPrefix, cgiName);
    websSetVarFmt(wp, "SCRIPT_NAME", "%s/%s", cgiPrefix, cgiName);
    websSetVar(wp, "SCRIPT_FILENAME", cgiPath);

/*
    See if the file exists and is executable.  If not error out.  Don't do this step for VxWorks, since the module
    may already be part of the OS image, rather than in the file system.
 */
#if !VXWORKS
    {
        WebsStat sbuf;
        /*
            SECURITY Acceptable: TOCTOU race condition between stat/access and execution.
            Embedded system file system security is out of scope for this project.
         */
        if (stat(cgiPath, &sbuf) != 0 || !S_ISREG(sbuf.st_mode)) {
            exe = sfmt("%s.exe", cgiPath);
            if (stat(exe, &sbuf) == 0 && S_ISREG(sbuf.st_mode)) {
                wfree(cgiPath);
                cgiPath = exe;
            } else {
                trace(5, "Cannot find CGI program: %s", cgiPath);
                websError(wp, HTTP_CODE_NOT_FOUND, "CGI program file does not exist");
                wfree(exe);
                wfree(cgiPath);
                goto done;
            }
        }
#if ME_WIN_LIKE
        /*
            Windows CGI: only .exe is supported. .bat / .cmd are deliberately not
            supported because kernel32's CreateProcessInternal transparently routes
            script-extension targets through cmd.exe, exposing the spawned process
            to OS command injection from any caller that does not exhaustively escape
            for cmd.exe parsing - which is not mechanically possible in the general
            case (notably %VAR% expansion). The gate is a true suffix match, not a
            substring match, so paths like 'payload.exe.bat' cannot smuggle a .bat
            past it.
         */
        if (!isCgiExeOnWindows(cgiPath))
#else
        if (cgiPath == NULL || access(cgiPath, X_OK) != 0)
#endif
        {
            websError(wp, HTTP_CODE_NOT_FOUND, "CGI process file is not executable");
            wfree(cgiPath);
            goto done;
        }
    }
#endif /* ! VXWORKS */
    if (!cgiExtensionAllowed(wp->route, cgiPath)) {
        trace(5, "CGI program %s does not have an extension the route permits", cgiPath);
        websError(wp, HTTP_CODE_NOT_FOUND, "CGI program file does not exist");
        wfree(cgiPath);
        goto done;
    }
    if ((argp = buildCgiArgs(cgiPath, wp->query, &query, &status)) == NULL) {
        websError(wp, status, status == HTTP_CODE_BAD_REQUEST ? "Bad ISINDEX argument" :
                  status == HTTP_CODE_REQUEST_TOO_LARGE ? "Too many arguments" : "Cannot allocate CGI args");
        wfree(cgiPath);
        goto done;
    }
    envp = buildCgiEnv(wp);

    if (wp->cgiStdin == NULL) {
        wp->cgiStdin = websGetCgiCommName();
    }
    stdIn = wp->cgiStdin;
    stdOut = websGetCgiCommName();

    /*
        Register before launch for shared failure cleanup. Retain query storage with argp because
        taskSpawn reads the arguments after launchCgi returns.
     */
    cid = wallocObject(&cgiList, &cgiMax, sizeof(Cgi));
    cgip = cgiList[cid];
    cgip->stdIn = stdIn;
    cgip->stdOut = stdOut;
    cgip->cgiPath = cgiPath;
    cgip->query = query;
    cgip->argp = argp;
    cgip->envp = envp;
    cgip->wp = wp;
    cgip->fplacemark = 0;
    cgip->txLen = -1;
    cgip->txBytes = 0;
    cgip->txExcess = 0;
    cgip->txSuppress = 0;
#if VXWORKS
    cgip->fdin = cgip->fdout = -1;
#endif
    if (!stdIn || !stdOut || (cgip->handle = launchCgi(cgip, dir)) == (CgiPid) - 1) {
        /*
            Release the registry slot before error handling can tear down the request. cgiCleanup removes staging files.
         */
        cgip->handle = 0;
        cgiCleanup(cgip, cid);
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "failed to spawn CGI task");
    }
done:
    wfree(dir);
    return 1;
}


PUBLIC int websCgiOpen(void)
{
    websDefineHandler("cgi", 0, cgiHandler, 0, 0);
    return 0;
}


/*
    Release a CGI registry slot and everything it owns. The child must already have been reaped.
    cgip->stdIn is borrowed from wp->cgiStdin and is freed by the Webs teardown, so unlink it here
    but do not free it.
 */
static void cgiCleanup(Cgi *cgip, int cid)
{
    char **ep;

#if VXWORKS
    releaseVxTask(cgip);
#endif
    if (cgip->stdIn) {
        unlink(cgip->stdIn);
    }
    if (cgip->stdOut) {
        unlink(cgip->stdOut);
    }
    cgiMax = wfreeHandle(&cgiList, cid);
    for (ep = cgip->envp; ep != NULL && *ep != NULL; ep++) {
        wfree(*ep);
    }
    wfreeset(cgip->cgiPath);
    wfreeset(cgip->stdOut);
    wfreeset(cgip->query);
    wfreeset(cgip->argp);
    wfreeset(cgip->envp);
    wfree(cgip);
}


/*
    Detach a request from the CGI registry. cgiList publishes a Webs pointer and the request's temp
    file names, but the registry does not own them. The request can die by timeout, client close or
    error while its child is still running, so the owner must unregister before it releases itself,
    otherwise websCgiPoll reads and writes through freed memory on its next pass. There is nobody
    left to deliver output to, so kill and reap the orphaned child rather than leaking it.
 */
PUBLIC void websCgiDetach(Webs *wp)
{
    Cgi *cgip;
    int cid;

    for (cid = 0; cid < cgiMax; cid++) {
        if ((cgip = cgiList[cid]) != NULL && cgip->wp == wp) {
            killCgi(cgip);
            cgiCleanup(cgip, cid);
            break;
        }
    }
}


PUBLIC bool websProcessCgiData(Webs *wp)
{
    ssize nbytes;

    nbytes = bufLen(&wp->input);
    trace(5, "cgi: write %zd bytes to CGI program", nbytes);
    if (write(wp->cgifd, bufStart(&wp->input), (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot write to CGI gateway");
    } else {
        trace(5, "cgi: write %zd bytes to CGI program", nbytes);
    }
    websConsumeInput(wp, nbytes);
    return 1;
}


static void writeCgiHeaders(Webs *wp, int status, ssize contentLength, cchar *location, cchar *contentType)
{
    trace(5, "cgi: Start response headers");
    websSetStatus(wp, status);
    websWriteHeaders(wp, contentLength, location);
    websWriteHeader(wp, "Pragma", "no-cache");
    // Use the program's output type, not the script path's extension.
    websWriteHeader(wp, "Content-Type", "%s", contentType ? contentType : "text/plain");
}


/* The server owns connection and framing headers. Keys are lowercase. */
static bool isHopHeader(cchar *key)
{
    static cchar *hop[] = {
        "connection", "keep-alive", "proxy-connection", "te", "trailer", "transfer-encoding", "upgrade", 0
    };
    cchar        **cp;

    for (cp = hop; *cp; cp++) {
        if (strcmp(key, *cp) == 0) {
            return 1;
        }
    }
    return 0;
}


/*
    Parse a Content-Length declared by the CGI program. The value is a claim by an untrusted child,
    not a fact, so admit it only as a plain non-negative decimal that fits an ssize. A sign, trailing
    junk or an overflowing value yields -1, which selects chunked framing so the byte count actually
    forwarded frames the response instead.
 */
static ssize parseCgiContentLength(cchar *value)
{
    cchar *cp;
    ssize len;

    if (value == NULL || *value == '\0') {
        return -1;
    }
    len = 0;
    for (cp = value; *cp; cp++) {
        if (!isdigit((uchar) *cp)) {
            return -1;
        }
        if (len > (MAXSSIZE - (*cp - '0')) / 10) {
            return -1;
        }
        len = (len * 10) + (*cp - '0');
    }
    return len;
}


/*
    Parse a status code declared by the CGI. RFC 3875 defines the value as three digits optionally
    followed by a space and a reason phrase; anything else is a malformed response, not a status.
    Return 0 for a value that cannot be a status so the caller answers on its own behalf rather than
    passing the CGI's claim through.
 */
static int parseCgiStatus(cchar *value)
{
    int code, i;

    if (value == NULL) {
        return 0;
    }
    code = 0;
    for (i = 0; i < 3; i++) {
        if (!isdigit((uchar) value[i])) {
            return 0;
        }
        code = (code * 10) + (value[i] - '0');
    }
    if (value[3] != '\0' && value[3] != ' ' && value[3] != '\t') {
        return 0;
    }
    /*
        Refuse 1XX. An interim status as the final response stalls the client waiting for a second
        response the gateway will never send. Codes above 5XX misframe nothing, so three digits is
        the only bound applied above.
     */
    if (code < 200) {
        return 0;
    }
    return code;
}


/*
    One tokenized line of the CGI response header block.
 */
typedef struct CgiHeader {
    char *key;                              /* Lower case header name. Zero once consumed as a control header */
    char *value;                            /* Trimmed header value */
} CgiHeader;


/* Split CGI headers only at LF. Strip preceding CRs, and reject CRs elsewhere in a line. */
static char *getCgiLine(char **next, int *malformed)
{
    char  *line, *nl;
    ssize len;

    line = *next;
    if (*line == '\0') {
        return NULL;
    }
    if ((nl = strchr(line, '\n')) != NULL) {
        *nl = '\0';
        *next = nl + 1;
    } else {
        *next = &line[slen(line)];
    }
    len = slen(line);
    while (len > 0 && line[len - 1] == '\r') {
        line[--len] = '\0';
    }
    if (strchr(line, '\r') != NULL) {
        trace(5, "cgi: bare CR in response header line");
        *malformed = 1;
    }
    return line;
}


/*
    Parse CGI headers. Return bytes consumed, zero if incomplete, or -1 to defer.
    Deferral leaves state unchanged; staged counts unread output bytes on disk.
 */
static ssize parseCgiHeaders(Cgi *cgip, char *buf, ssize staged)
{
    CgiHeader *headers;
    Webs      *wp;
    char      *end, *cp, *line, *code, *location, *contentType;
    ssize     len, contentLength, bodyLimit, availableBody;
    int       count, i, nlines, status, statusSeen, seenLength, malformed;

    wp = cgip->wp;
    status = HTTP_CODE_OK;
    statusSeen = 0;
    seenLength = 0;
    malformed = 0;
    contentLength = -1;
    contentType = 0;
    location = 0;

    /* The first blank line ends the headers, including an empty header block. */
    for (cp = buf; ; cp = end + 1) {
        if (cp[0] == '\n' || (cp[0] == '\r' && cp[1] == '\n')) {
            end = cp;
            len = (cp[0] == '\n') ? 1 : 2;
            break;
        }
        if ((end = strchr(cp, '\n')) == NULL) {
            return 0;
        }
    }
    /*
        Test for a header block before terminating it. Returning zero after writing the NUL would hand
        the caller a buffer with the first byte of the separator destroyed, and the caller emits that
        buffer verbatim as the response body.
     */
    if (memchr(buf, ':', end - buf) == NULL) {
        /* No headers found */
        return 0;
    }
    *end = '\0';
    end += len;
    cp = buf;
    if (strncmp(cp, "HTTP/1.", 7) == 0) {
        /*
            Apply an nph status line using the same validation as Status.
         */
        line = getCgiLine(&cp, &malformed);
        if ((code = strchr(line, ' ')) == NULL || (status = parseCgiStatus(code + 1)) == 0) {
            trace(5, "cgi: bad status line \"%s\"", line);
            status = HTTP_CODE_OK;
            malformed = 1;
        }
        statusSeen = 1;
    }
    /* Capture lines before the two header passes because tokenization is destructive. */
    nlines = 1;
    for (line = cp; (line = strchr(line, '\n')) != 0; line++) {
        nlines++;
    }
    headers = walloc(nlines * sizeof(CgiHeader));
    count = 0;
    while (count < nlines && (line = getCgiLine(&cp, &malformed)) != NULL) {
        /*
            Tokenize one complete header line before splitting the name from the value. Splitting
            on ":" first leaves the remainder of the header block inside "value", so a value
            containing CR/LF carries the following headers through to the client unchecked.
         */
        if (strchr(line, ':') == NULL) {
            trace(5, "cgi: bad response http header line: \"%s\"", line);
            continue;
        }
        headers[count].key = slower(ssplit(line, ":", &headers[count].value));
        headers[count].value = strim(headers[count].value, " \t", WEBS_TRIM_BOTH);
        count++;
    }
    /*
        Collect the CGI control headers before emitting anything. RFC 3875 places no ordering
        requirement on the response headers, so a control header may follow a pass-through header and
        must still shape the status line and standard headers.
     */
    for (i = 0; i < count; i++) {
        if (strcmp(headers[i].key, "location") == 0) {
            location = headers[i].value;
            if (!websValidHeaderValue(location)) {
                trace(5, "cgi: bad Location");
                malformed = 1;
            }
        } else if (strcmp(headers[i].key, "status") == 0) {
            if ((status = parseCgiStatus(headers[i].value)) == 0) {
                trace(5, "cgi: bad Status \"%s\"", headers[i].value);
                malformed = 1;
            }
            statusSeen = 1;
        } else if (strcmp(headers[i].key, "content-type") == 0) {
            contentType = headers[i].value;
            if (!websValidHeaderValue(contentType)) {
                trace(5, "cgi: bad Content-Type");
                malformed = 1;
            }
        } else if (strcmp(headers[i].key, "content-length") == 0) {
            len = parseCgiContentLength(headers[i].value);
            /*
                RFC 9110 makes a message carrying two different Content-Length values unrecoverably
                invalid. Taking the last one, as this did, makes the framing depend on the order the
                CGI happened to write them. Repeating the same value is permitted and is collapsed.
             */
            if (seenLength && len != contentLength) {
                trace(5, "cgi: conflicting Content-Length \"%s\"", headers[i].value);
                malformed = 1;
            } else if (len < 0) {
                trace(5, "cgi: bad Content-Length \"%s\" - using chunked", headers[i].value);
            }
            contentLength = len;
            seenLength = 1;
        } else {
            continue;
        }
        /*
            Consumed by writeCgiHeaders below, so do not pass it through as well.
         */
        headers[i].key = 0;
    }
    /*
        Location without Status implies 302, including local paths (RFC 3875 6.2).
     */
    if (location && !statusSeen) {
        status = HTTP_CODE_MOVED_TEMPORARILY;
    }
    /*
        A response this gateway cannot interpret is answered on the gateway's own behalf. Forward
        none of it, neither body nor remaining headers, which may carry cookies or cache directives
        meant to accompany a response the client is not going to receive.
     */
    if (malformed) {
        status = HTTP_CODE_INTERNAL_SERVER_ERROR;
        contentLength = 0;
        location = 0;
        contentType = 0;
    }

    /*
        Decide whether this response may carry a body at all, before any of it is written. Body bytes
        after a bodyless status are unframed data the client reads as the next response. txSuppress
        marks honest suppression, distinct from a CGI over-writing its declared length, so the caller
        neither reports it nor closes. A HEAD keeps the length an equivalent GET would return; every
        other case declares zero, never unknown, as unknown selects chunked framing whose trailer is
        itself a body byte.
     */
    bodyLimit = contentLength;
    if (websStatusHasNoBody(status) || smatch(wp->method, "HEAD") || malformed) {
        cgip->txSuppress = 1;
        bodyLimit = 0;
        if (!smatch(wp->method, "HEAD") || contentLength < 0) {
            contentLength = 0;
        }
    }
    /*
        Defer headers while a running CGI may still meet its declared length.
        Decide keep-alive before writing Connection; unknown-length responses can stream.
     */
    availableBody = staged - (end - buf);
    if (!cgip->txSuppress && contentLength >= 0) {
        if (cgip->handle && availableBody <= contentLength) {
            trace(5, "cgi: defer response headers until the declared length %zd is settled (%zd staged)",
                  contentLength, availableBody);
            wfree(headers);
            return -1;
        }
        if (availableBody != contentLength) {
            /*
                Clear keep-alive before headers when the declared length cannot be delivered.
             */
            trace(5, "cgi: declared Content-Length %zd but staged %zd bytes - closing after the response",
                  contentLength, availableBody);
            wp->flags &= ~WEBS_KEEP_ALIVE;
        }
    }
    writeCgiHeaders(wp, status, contentLength, location, contentType);
    cgip->txLen = bodyLimit;

    /*
        Now pass all other headers back to the client
     */
    for (i = 0; i < count && !malformed; i++) {
        if (headers[i].key == 0) {
            continue;
        }
        if (isHopHeader(headers[i].key)) {
            trace(5, "cgi: dropping hop-by-hop response header \"%s\"", headers[i].key);
        } else if (websWriteHeader(wp, headers[i].key, "%s", headers[i].value) < 0) {
            trace(5, "cgi: dropped response header \"%s\"", headers[i].key);
        }
    }
    wfree(headers);
    websWriteEndHeaders(wp);
    return end - buf;
}


PUBLIC void websCgiGatherOutput(Cgi *cgip)
{
    Webs     *wp;
    WebsStat sbuf;
    char     *buf;
    ssize    bufSize, nbytes, skip, nbody, room, excess;
    int      fdout;

    /*
        OPT - currently polling and doing a stat each poll. Also doing open/close each chunk.
        If the CGI process writes partial headers, this repeatedly reads the data until complete
        headers are written or more than ME_GOAHEAD_LIMIT_HEADERS of data is received.
     */
    if ((stat(cgip->stdOut, &sbuf) == 0) && (sbuf.st_size > cgip->fplacemark)) {
        if ((fdout = open(cgip->stdOut, O_RDONLY | O_BINARY | O_NOFOLLOW, 0444)) >= 0) {
            /*
                Check to see if any data is available in the output file and send its contents to the socket.
                Write the HTTP header on our first pass. The header must fit into ME_GOAHEAD_LIMIT_BUFFER.
             */
            wp = cgip->wp;
            bufSize = ME_GOAHEAD_LIMIT_HEADERS + 2;
            buf = walloc(bufSize);
            lseek(fdout, cgip->fplacemark, SEEK_SET);
            while ((nbytes = read(fdout, buf, (uint) (bufSize - 1))) > 0) {
                buf[nbytes] = 0;
                skip = 0;
                if (!(wp->flags & WEBS_HEADERS_CREATED)) {
                    skip = parseCgiHeaders(cgip, buf, (ssize) (sbuf.st_size - cgip->fplacemark));
                    if (skip < 0) {
                        /*
                            Retry the same staged bytes once CGI length is known.
                         */
                        break;
                    }
                    if (skip == 0) {
                        if (cgip->handle && sbuf.st_size < ME_GOAHEAD_LIMIT_HEADERS) {
                            trace(5, "cgi: waiting for http headers");
                            break;
                        } else {
                            /*
                                The CGI emitted no recognizable header block. Emit a default set and
                                close it, otherwise the program output below lands inside the header
                                block and any "Name: value" line it writes becomes a real header.
                                Closing also sets WEBS_HEADERS_CREATED so the output is not re-parsed
                                as headers, and enables the promised chunked framing. Default the
                                RFC 3875 required Content-Type to text/plain so headerless output
                                cannot be sniffed as HTML.
                             */
                            trace(5, "cgi: missing http headers - create default headers");
                            writeCgiHeaders(wp, HTTP_CODE_OK, -1, 0, "text/plain");
                            websWriteEndHeaders(wp);
                        }
                    }
                }
                nbody = nbytes - skip;
                /*
                    Surplus over the declared Content-Length is read by the client as the start of the
                    next response on a keep-alive connection. Drop it rather than emit it and take the
                    connection down. Only accumulate the count here, as this branch is re-entered on
                    every later read; the total is reported once when the child exits.
                 */
                if (cgip->txSuppress) {
                    /*
                        The response was framed as carrying no body, so dropping one is the framing
                        working, not a fault. Say nothing and leave the connection reusable.
                     */
                    nbody = 0;

                } else if (cgip->txLen >= 0) {
                    /*
                        Count against remaining length to avoid overflow; saturate the surplus count.
                     */
                    room = cgip->txLen - cgip->txBytes;
                    if (nbody > room) {
                        excess = nbody - room;
                        cgip->txExcess = (cgip->txExcess > MAXSSIZE - excess) ? MAXSSIZE : cgip->txExcess + excess;
                        nbody = room;
                        wp->flags &= ~WEBS_KEEP_ALIVE;
                    }
                    cgip->txBytes += nbody;
                }
                if (nbody > 0) {
                    trace(5, "cgi: write %zd bytes to client", nbody);
                    websWriteBlock(wp, &buf[skip], nbody);
                }
                cgip->fplacemark += (off_t) nbytes;
            }
            wfree(buf);
            close(fdout);
        } else {
            trace(5, "cgi: open failed");
        }
    }
}


/*
    Any entry in the cgiList need to be checked to see if it has completed, and if so, process its output and clean up.
    Return time till next poll.
 */
int websCgiPoll(void)
{
    Webs *wp;
    Cgi  *cgip;
    int  cid;

    for (cid = 0; cid < cgiMax; cid++) {
        if ((cgip = cgiList[cid]) != NULL) {
            wp = cgip->wp;
            websCgiGatherOutput(cgip);
            if (cgip->handle && checkCgi(cgip) == 0) {
                /*
                    We get here if the CGI process has terminated. Clean up.
                 */
                cgip->handle = 0;
                websCgiGatherOutput(cgip);
            }
            if (cgip->handle == 0) {
#if WINDOWS
                /*
                    Windows can have delayed notification through the file system after process exit. Look again
                    on later polls rather than block the event loop.
                 */
                if (cgip->fplacemark == 0) {
                    if (cgip->outputDeadline == 0) {
                        cgip->outputDeadline = GetTickCount64() + CGI_OUTPUT_GRACE;
                    }
                    if (GetTickCount64() < cgip->outputDeadline) {
                        continue;
                    }
                }
#endif
                if (cgip->fplacemark == 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "CGI generated no output");
                } else {
                    /*
                        Report a length the CGI could not honour, once per request. Truncating the
                        surplus leaves txBytes equal to txLen, so only the accumulated excess records
                        it. A shortfall is still unhandled here: the headers are on the wire and the
                        framing cannot be corrected, so close rather than keep a desynchronised
                        stream alive.
                     */
                    if (cgip->txExcess > 0) {
                        trace(2, "cgi: declared Content-Length %zd and dropped %zd surplus bytes",
                              cgip->txLen, cgip->txExcess);

                    } else if (cgip->txLen >= 0 && cgip->txBytes != cgip->txLen) {
                        trace(2, "cgi: declared Content-Length %zd but wrote %zd bytes",
                              cgip->txLen, cgip->txBytes);
                        wp->flags &= ~WEBS_KEEP_ALIVE;
                    }
                    trace(5, "cgi: Request complete - calling websDone");
                    websDone(wp);
                }
                /*
                    Remove the temporary re-direction files and free the registry slot. This must
                    happen before the Webs object is released below, as the slot borrows wp->cgiStdin.
                 */
                cgiCleanup(cgip, cid);
                /* Completion clears WEBS_KEEP_ALIVE even on reused connections; free only closed connections. */
                websPump(wp);
                if (wp->flags & WEBS_CLOSED) {
                    websFree(wp);
                    /* wp no longer valid */
                }
            }
        }
    }
    return cgiMax ? 10 : MAXINT;
}


/*
    Returns a pointer to an allocated qualified unique temporary file name. This filename must eventually be deleted
       with
    wfree().
 */
PUBLIC char *websGetCgiCommName(void)
{
    return websTempFile(NULL, "cgi");
}


#if WINCE
/*
     Launch the CGI process and return a handle to it.  CE note: This function is not complete.  The missing piece is
     the ability to redirect stdout.
 */
static CgiPid launchCgi(Cgi *cgip, char *dir)
{
    PROCESS_INFORMATION procinfo;       /*  Information about created proc   */
    DWORD               dwCreateFlags;
    char                *fulldir;
    BOOL                bReturn;
    char                *cgiPath;
    int                 i, nLen;

    /*
        Replace directory delimiters with Windows-friendly delimiters
     */
    cgiPath = cgip->cgiPath;
    nLen = strlen(cgiPath);
    for (i = 0; i < nLen; i++) {
        if (cgiPath[i] == '/') {
            cgiPath[i] = '\\';
        }
    }
    fulldir = NULL;
    dwCreateFlags = CREATE_NEW_CONSOLE;

    /*
         CreateProcess returns errors sometimes, even when the process was started correctly.  The cause is not evident.
         For now: we detect an error by checking the value of procinfo.hProcess after the call.
     */
    procinfo.hThread = NULL;
    bReturn = CreateProcess(
        cgiPath,            /*  Name of executable module        */
        NULL,               /*  Command line string              */
        NULL,               /*  Process security attributes      */
        NULL,               /*  Thread security attributes       */
        0,                  /*  Handle inheritance flag          */
        dwCreateFlags,      /*  Creation flags                   */
        NULL,               /*  New environment block            */
        dir,                /*  Current directory name           */
        NULL,               /*  STARTUPINFO                      */
        &procinfo);         /*  PROCESS_INFORMATION              */
    if (bReturn == 0) {
        return -1;
    } else {
        CloseHandle(procinfo.hThread);
    }
    return (int) procinfo.dwProcessId;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(Cgi *cgip)
{
    int   nReturn;
    DWORD exitCode;

    nReturn = GetExitCodeProcess((HANDLE) cgip->handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE) cgip->handle);
        return 0;
    }
    return 1;
}


/*
    Terminate an orphaned CGI process and release its handle.
 */
static void killCgi(Cgi *cgip)
{
    if (cgip->handle) {
        TerminateProcess((HANDLE) cgip->handle, 0);
        CloseHandle((HANDLE) cgip->handle);
    }
}
#endif /* WINCE */


#if ME_UNIX_LIKE || QNX
/* Use descriptor writes in the child to avoid flushing inherited parent stdio buffers. */
static void childError(cchar *msg)
{
    ssize len, written;

    for (len = slen(msg); len > 0; len -= written, msg += written) {
        if ((written = write(1, msg, len)) <= 0) {
            break;
        }
    }
}


/*
    Close non-stdio descriptors before exec so CGI cannot inherit server resources.
 */
static void closeInheritedFiles(void)
{
#if FREEBSD
    closefrom(3);
#else
    long max;
    int  fd;

#if LINUX || MACOSX
    {
        struct dirent *dp;
        DIR           *dir;

    #if LINUX
        dir = opendir("/proc/self/fd");
    #else
        dir = opendir("/dev/fd");
    #endif
        if (dir != NULL) {
            while ((dp = readdir(dir)) != NULL) {
                fd = atoi(dp->d_name);
                if (fd > 2 && fd != dirfd(dir)) {
                    close(fd);
                }
            }
            closedir(dir);
            return;
        }
    }
#endif
    max = sysconf(_SC_OPEN_MAX);
    for (fd = 3; fd < max; fd++) {
        close(fd);
    }
#endif
}


/*
    Launch the CGI process and return a handle to it.
 */
static CgiPid launchCgi(Cgi *cgip, char *dir)
{
    sigset_t mask;
    char     *cgiPath;
    int      fdin, fdout, pid, sig;

    cgiPath = cgip->cgiPath;
    trace(5, "cgi: run %s", cgiPath);

    /*
        websTempFile already created both files, private and empty, so these open an existing file.
        O_NOFOLLOW keeps that true if the path is replaced between the create and here.
     */
    if ((fdin = open(cgip->stdIn, O_RDWR | O_BINARY | O_NOFOLLOW, 0600)) < 0) {
        error("Cannot open CGI stdin: %s", cgiPath);
        return -1;
    }
    if ((fdout = open(cgip->stdOut, O_RDWR | O_TRUNC | O_BINARY | O_NOFOLLOW, 0600)) < 0) {
        error("Cannot open CGI stdout: %s", cgiPath);
        close(fdin);
        return -1;
    }

    pid = fork();
    if (pid == 0) {
        /*
            Set the CGI process group in both parent and child before either can signal it.
         */
        setpgid(0, 0);
        /*
            Reset ignored signals and the mask, which exec preserves.
         */
        for (sig = 1; sig < NSIG; sig++) {
            signal(sig, SIG_DFL);
        }
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);
        /*
            Move fdout away from descriptor 0 before replacing stdin.
         */
        if (fdout == 0) {
            fdout = fcntl(fdout, F_DUPFD, 3);
        }
        /*
            Emit an error Status so startup failures cannot become successful CGI responses.
         */
        if (dup2(fdin, 0) < 0) {
            childError("Status: 500 Internal Server Error\ncontent-type: text/html\n\nDup of stdin failed\n");
            _exit(1);

        } else if (dup2(fdout, 1) < 0) {
            childError("Status: 500 Internal Server Error\ncontent-type: text/html\n\nDup of stdout failed\n");
            _exit(1);
        }
        /*
            The directory is the child's own. The server's working directory never changes.
         */
        if (chdir(dir) < 0) {
            childError("Status: 500 Internal Server Error\ncontent-type: text/html\n\n"
                       "Cannot change working directory\n");
            _exit(127);
        }
        closeInheritedFiles();
        execve(cgiPath, cgip->argp, cgip->envp);
        childError("Status: 502 Bad Gateway\ncontent-type: text/html\n\nExecution of cgi process failed\n");
        _exit(0);
    }
    /*
        Parent
     */
    if (pid > 0) {
        setpgid(pid, pid);
    }
    if (fdout >= 0) {
        close(fdout);
    }
    if (fdin >= 0) {
        close(fdin);
    }
    return pid;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(Cgi *cgip)
{
    CgiPid handle;
    int    pid;

    handle = cgip->handle;
    /*
        Treat ECHILD as termination; an embedder may have reaped the child already.
     */
    if ((pid = waitpid((CgiPid) handle, NULL, WNOHANG)) == handle) {
        trace(5, "cgi: waited for pid %d", pid);
        return 0;
    } else if (pid < 0 && errno == ECHILD) {
        trace(5, "cgi: pid %d already reaped", (int) handle);
        return 0;
    } else {
        return 1;
    }
}


/*
    SIGKILL the CGI group before reaping so an ignored signal cannot block the event loop.
    The unreaped leader prevents group-ID reuse.
 */
static void killCgi(Cgi *cgip)
{
    CgiPid handle;

    handle = cgip->handle;
    if (handle > 0) {
        trace(5, "cgi: kill orphaned pid %d", (int) handle);
        if (kill(-handle, SIGKILL) < 0) {
            kill(handle, SIGKILL);
        }
        waitpid(handle, NULL, 0);
    }
}
#endif /* LINUX || LYNX || MACOSX || QNX4 */


#if VXWORKS
#if _WRS_VXWORKS_MAJOR < 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 9)
static int findVxSym(SYMTAB_ID sid, char *name, char **pvalue)
{
    SYM_TYPE type;

    return symFindByName(sid, name, pvalue, &type);
}
#else

static int findVxSym(SYMTAB_ID sid, char *name, char **pvalue)
{
    SYMBOL_DESC symDesc;

    memset(&symDesc, 0, sizeof(SYMBOL_DESC));
    symDesc.mask = SYM_FIND_BY_NAME;
    symDesc.name = name;

    if (symFind(sid, &symDesc) == ERROR) {
        return ERROR;
    }
    if (pvalue != NULL) {
        *pvalue = (char*) symDesc.value;
    }
    return OK;
}
#endif

/*
    Keep modules loaded while tasks execute them; reload after the last task exits.
 */
typedef struct CgiModule {
    char *path;
    MODULE_ID id;
    int tasks;
} CgiModule;

static CgiModule **cgiModules;
static int       cgiModuleMax;
static int       cgiTaskSeq;

static void vxWebsCgiEntry(void*entryAddr(int argc, char **argv), char **argp, char **envp, int fdin, int fdout);

/*
    taskSpawn takes its task arguments as _Vx_usr_arg_t, which holds a pointer on 64-bit VxWorks. Releases before 6.9
    take int and are 32-bit only.
 */
#if _WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 9)
    #define VX_ARG(x) ((_Vx_usr_arg_t) (x))
#else
    #define VX_ARG(x) ((int) (x))
#endif

static char *findVxEntry(char *pEntry)
{
    char *entryAddr, *pname;

    entryAddr = 0;
    if (findVxSym(sysSymTbl, pEntry, &entryAddr) == ERROR) {
        pname = sfmt("_%s", pEntry);
        findVxSym(sysSymTbl, pname, &entryAddr);
        wfree(pname);
    }
    return entryAddr;
}


static CgiModule *findVxModule(cchar *path)
{
    int mid;

    for (mid = 0; mid < cgiModuleMax; mid++) {
        if (cgiModules[mid] && smatch(cgiModules[mid]->path, path)) {
            return cgiModules[mid];
        }
    }
    return NULL;
}


static CgiModule *loadVxModule(cchar *path)
{
    CgiModule *mp;
    MODULE_ID id;
    int       fd, mid;

    if ((fd = open(path, O_RDONLY | O_BINARY, 0666)) < 0) {
        return NULL;
    }
    id = loadModule(fd, LOAD_GLOBAL_SYMBOLS);
    close(fd);
    if (id == NULL) {
        error("Cannot load the CGI module %s", path);
        return NULL;
    }
    mid = wallocObject(&cgiModules, &cgiModuleMax, sizeof(CgiModule));
    mp = cgiModules[mid];
    mp->path = sclone(path);
    mp->id = id;
    return mp;
}


static void unloadVxModule(CgiModule *mp)
{
    int mid;

    unldByModuleId(mp->id, 0);
    for (mid = 0; mid < cgiModuleMax; mid++) {
        if (cgiModules[mid] == mp) {
            cgiModuleMax = wfreeHandle(&cgiModules, mid);
            break;
        }
    }
    wfreeset(mp->path);
    wfree(mp);
}


/*
    Run basename_cgientry as a VxWorks task. The server owns stdio because taskDelete does not close it.
 */
static CgiPid launchCgi(Cgi *cgip, char *dir)
{
    CgiModule *mp;
    CgiPid    rc;
    char      *p, *basename, *pEntry, *entryAddr;
    int       priority;

    /*
        VxWorks shares one working directory across all tasks; do not change it for CGI.
     */
    (void) dir;

    /*
        Determine the basename, which is without path or the extension.
     */
    if ((p = strrchr(cgip->cgiPath, '/')) != NULL) {
        p++;
    } else {
        p = cgip->cgiPath;
    }
    basename = sclone(p);
    if ((p = strrchr(basename, '.')) != NULL) {
        *p = '\0';
    }
    /*
        Run below the server, so a busy CGI cannot starve the event loop that times it out. Numerically higher is lower
        priority.
     */
    taskPriorityGet(taskIdSelf(), &priority);
    priority = ME_GOAHEAD_CGI_PRIORITY ? ME_GOAHEAD_CGI_PRIORITY : min(priority + 1, 255);
    rc = (CgiPid) - 1;

    /*
        Reuse modules with active tasks; otherwise prefer linked entry points before loading.
     */
    pEntry = sfmt("%s_cgientry", basename);
    entryAddr = 0;
    if ((mp = findVxModule(cgip->cgiPath)) == NULL && (entryAddr = findVxEntry(pEntry)) == 0) {
        mp = loadVxModule(cgip->cgiPath);
    }
    if (mp) {
        entryAddr = findVxEntry(pEntry);
    }
    if (entryAddr == 0) {
        error("Cannot find the CGI entry point %s", pEntry);

    } else if ((cgip->fdout = open(cgip->stdOut, O_RDWR | O_NOFOLLOW, 0600)) < 0 ||
               (cgip->fdin = open(cgip->stdIn, O_RDONLY | O_NOFOLLOW, 0600)) < 0) {
        error("Cannot open CGI stdio for %s", cgip->cgiPath);

    } else {
        cgip->vxName = sfmt("cgi-%d", ++cgiTaskSeq);
        rc = taskSpawn(cgip->vxName, priority, 0, 20000, (void*) vxWebsCgiEntry, VX_ARG(entryAddr),
                       VX_ARG(cgip->argp), VX_ARG(cgip->envp), VX_ARG(cgip->fdin), VX_ARG(cgip->fdout),
                       0, 0, 0, 0, 0);
    }
    if (mp) {
        if (rc != (CgiPid) - 1) {
            mp->tasks++;
            cgip->module = mp;
        } else if (mp->tasks == 0) {
            unloadVxModule(mp);
        }
    }
    wfree(basename);
    wfree(pEntry);
    return rc;
}


/*
    Set task stdio, argc/argv, and private environment before calling the entry point.
    envLib's delete hook frees the environment if the task is aborted.
 */
static void vxWebsCgiEntry(void*entryAddr(int argc, char **argv), char **argp, char **envp, int fdin, int fdout)
{
    char   **p;
    CgiPid taskId;
    int    argc;

    taskId = taskIdSelf();
    ioTaskStdSet(taskId, 0, fdin);
    ioTaskStdSet(taskId, 1, fdout);

    for (argc = 0, p = argp; p != NULL && *p != NULL; p++, argc++) { }

    if (envPrivateCreate(taskId, -1) != OK) {
        printf("content-type: text/html\n\n" "Can not create CGI environment space\n");
        exit(0);
    }
    for (p = envp; p != NULL && *p != NULL; p++) {
        putenv(*p);
    }
    (*entryAddr)(argc, argp);

    envPrivateDestroy(taskId);
    exit(0);
}


/*
    Check both ID and unique name under the scheduler lock to exclude task-ID reuse.
 */
static bool isCgiTask(Cgi *cgip)
{
    char *name;

    return cgip->handle && cgip->vxName && taskIdVerify(cgip->handle) == OK &&
           (name = taskName(cgip->handle)) != NULL && smatch(name, cgip->vxName);
}



static int checkCgi(Cgi *cgip)
{
    bool alive;

    taskLock();
    alive = isCgiTask(cgip);
    taskUnlock();
    return alive;
}


/*
    Check and delete atomically to avoid deleting a reused task ID.
 */
static void killCgi(Cgi *cgip)
{
    taskLock();
    if (isCgiTask(cgip)) {
        taskDelete(cgip->handle);
    }
    taskUnlock();
}


/*
    Release task stdio, name, and module reference after exit or failed launch.
 */
static void releaseVxTask(Cgi *cgip)
{
    CgiModule *mp;

    if (cgip->fdin >= 0) {
        close(cgip->fdin);
        cgip->fdin = -1;
    }
    if (cgip->fdout >= 0) {
        close(cgip->fdout);
        cgip->fdout = -1;
    }
    if ((mp = cgip->module) != NULL) {
        cgip->module = NULL;
        if (--mp->tasks == 0) {
            unloadVxModule(mp);
        }
    }
    wfreeset(cgip->vxName);
}
#endif /* VXWORKS */

#if WINDOWS
/*
    Convert a table of strings into a single block of memory. The input table consists of an array of null-terminated
    strings, terminated in a null pointer.  Returns the address of a block of memory allocated using the walloc()
    function.  The returned pointer must be deleted using wfree().  Returns NULL on error.
 */
static uchar *tableToBlock(char **table)
{
    uchar  *pBlock;         /*  Allocated block */
    char   *pEntry;         /*  Pointer into block */
    size_t sizeBlock;       /*  Size of table */
    int    index;           /*  Index into string table */

    assert(table);

    /*
        Calculate the size of the data block.  Allow for final null byte.
     */
    sizeBlock = 1;
    for (index = 0; table[index]; index++) {
        sizeBlock += strlen(table[index]) + 1;
    }
    if (sizeBlock < 2) {
        sizeBlock = 2;
    }
    /*
        Allocate the data block and fill it with the strings
     */
    pBlock = walloc(sizeBlock);

    if (pBlock != NULL) {
        pEntry = (char*) pBlock;
        for (index = 0; table[index]; index++) {
            strcpy(pEntry, table[index]);
            pEntry += strlen(pEntry) + 1;
        }
        /*
            Terminate the data block with an extra null string
         */
        *pEntry = '\0';
        pBlock[sizeBlock - 1] = '\0';
    }
    return pBlock;
}


/*
    Close a Windows handle if, and only if, it is a real handle. The two "not a handle" sentinels differ
    by origin: CreateFile returns INVALID_HANDLE_VALUE ((HANDLE) -1) on failure, while a zeroed
    STARTUPINFO or PROCESS_INFORMATION field is NULL. Passing either to CloseHandle raises
    STATUS_INVALID_HANDLE under the strict-handle-check mitigation policy, which terminates the process.
 */
static void closeWinHandle(HANDLE h)
{
    if (h != NULL && h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }
}


/*
    Windows launchCgi
    Create a temporary stdout file and launch the CGI process. Returns a handle to the spawned CGI process.
 */
static CgiPid launchCgi(Cgi *cgip, char *dir)
{
    STARTUPINFOEX                newinfo;
    SECURITY_ATTRIBUTES          security;
    PROCESS_INFORMATION          procinfo;       /*  Information about created proc   */
    LPPROC_THREAD_ATTRIBUTE_LIST attrs;
    HANDLE                       inherit[3], serverErr;
    SIZE_T                       attrSize;
    DWORD                        dwCreateFlags;
    char                         *cgiPath, *cmdLine, **pArgs;
    CgiPid                       handle;
    BOOL                         bReturn, attrsReady;
    ssize                        nLen;
    int                          i;
    uchar                        *pEnvData;

    /*
        One exit for the whole function. The command line and the environment block are freed there,
        so a failure to open either stdio file cannot orphan them. Zero both structures up front:
        the exit closes their handles unconditionally and CreateProcess does not guarantee it
        populates PROCESS_INFORMATION on failure.
     */
    handle = (CgiPid) - 1;
    cgiPath = cgip->cgiPath;
    cmdLine = 0;
    pEnvData = 0;
    attrs = 0;
    attrsReady = 0;
    memset(&newinfo, 0, sizeof(newinfo));
    memset(&procinfo, 0, sizeof(procinfo));

    /*
        Replace directory delimiters with Windows-friendly delimiters
     */
    nLen = strlen(cgiPath);
    for (i = 0; i < nLen; i++) {
        if (cgiPath[i] == '/') {
            cgiPath[i] = '\\';
        }
    }
    /*
        Calculate worst-case length of the quoted command line. Each argv element
        may grow to 2 * strlen + 3 bytes (opening / closing quote plus a doubled
        backslash run that fully precedes a quote), plus one separator per gap,
        plus one byte for the null terminator. See appendWinArg() for the exact
        quoting rules.
     */
    nLen = 1; /* null terminator */
    pArgs = cgip->argp;
    while (pArgs && *pArgs) {
        nLen += (ssize) (2 * slen(*pArgs) + 3);
        if (pArgs[1]) {
            nLen += 1; /* space between args */
        }
        pArgs++;
    }

    cmdLine = walloc(sizeof(char) * nLen);
    if (buildWinCmdLine(cgip->argp, cmdLine, nLen) < 0) {
        goto done;
    }

    /*
        Create the process start-up information
     */
    newinfo.StartupInfo.cb = sizeof(newinfo);
    newinfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    newinfo.StartupInfo.lpTitle = NULL;

    /*
        Create file handles for the spawned processes stdin and stdout files
     */
    security.nLength = sizeof(SECURITY_ATTRIBUTES);
    security.lpSecurityDescriptor = NULL;
    security.bInheritHandle = TRUE;

    /*
        Stdin file should already exist.
     */
    newinfo.StartupInfo.hStdInput = CreateFile(cgip->stdIn, GENERIC_READ, FILE_SHARE_READ, &security, OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (newinfo.StartupInfo.hStdInput == INVALID_HANDLE_VALUE) {
        error("Cannot open CGI stdin file");
        goto done;
    }

    /*
        Stdout file should already exist.
     */
    newinfo.StartupInfo.hStdOutput = CreateFile(cgip->stdOut, GENERIC_READ | GENERIC_WRITE,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
                                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (newinfo.StartupInfo.hStdOutput == INVALID_HANDLE_VALUE) {
        error("Cannot open CGI stdout file");
        goto done;
    }
    SetFilePointer(newinfo.StartupInfo.hStdOutput, 0, NULL, FILE_BEGIN);
    SetEndOfFile(newinfo.StartupInfo.hStdOutput);

    /*
        Send CGI stderr to server stderr or the null device, never to the response.
     */
    serverErr = GetStdHandle(STD_ERROR_HANDLE);
    if (serverErr == NULL || serverErr == INVALID_HANDLE_VALUE ||
        !DuplicateHandle(GetCurrentProcess(), serverErr, GetCurrentProcess(), &newinfo.StartupInfo.hStdError, 0,
                         TRUE, DUPLICATE_SAME_ACCESS)) {
        newinfo.StartupInfo.hStdError = CreateFile("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (newinfo.StartupInfo.hStdError == INVALID_HANDLE_VALUE) {
            error("Cannot open CGI stderr");
            goto done;
        }
    }

    /*
        Inherit only CGI input/output handles, excluding server resources.
     */
    inherit[0] = newinfo.StartupInfo.hStdInput;
    inherit[1] = newinfo.StartupInfo.hStdOutput;
    inherit[2] = newinfo.StartupInfo.hStdError;
    attrSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrSize);
    attrs = walloc((ssize) attrSize);
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize)) {
        error("Cannot create the CGI handle list");
        goto done;
    }
    attrsReady = 1;
    if (!UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit), NULL,
                                   NULL)) {
        error("Cannot limit the handles the CGI program inherits");
        goto done;
    }
    newinfo.lpAttributeList = attrs;

    /*
        Redirected stdio needs no console; per-request consoles consume desktop heap.
     */
    dwCreateFlags = CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT;
    pEnvData = tableToBlock(cgip->envp);

    /*
        CreateProcess returns errors sometimes, even when the process was started correctly.  The cause is not evident.
        Detect an error by checking the value of procinfo.hProcess after the call.
     */
    /*
        lpApplicationName is set explicitly to the resolved cgiPath so program identity
        does not depend on how lpCommandLine tokenises. Combined with the extension gate
        (only .exe admitted on Windows), this guarantees no implicit cmd.exe routing.
     */
    bReturn = CreateProcess(
        cgiPath,            /*  lpApplicationName - explicit, .exe only */
        cmdLine,            /*  lpCommandLine - CRT-correctly quoted    */
        NULL,               /*  Process security attributes             */
        NULL,               /*  Thread security attributes              */
        TRUE,
        dwCreateFlags,      /*  Creation flags                          */
        pEnvData,           /*  New environment block                   */
        dir,                /*  Current directory name                  */
        &newinfo.StartupInfo, /*  STARTUPINFOEX                         */
        &procinfo);         /*  PROCESS_INFORMATION                     */

    if (bReturn != 0) {
        handle = procinfo.hProcess;
    }

done:
    /*
        hProcess is deliberately not closed: it is the return value and the caller owns it.
     */
    closeWinHandle(procinfo.hThread);
    closeWinHandle(newinfo.StartupInfo.hStdInput);
    closeWinHandle(newinfo.StartupInfo.hStdOutput);
    closeWinHandle(newinfo.StartupInfo.hStdError);
    if (attrsReady) {
        DeleteProcThreadAttributeList(attrs);
    }
    wfree(attrs);
    wfree(pEnvData);
    wfree(cmdLine);
    return handle;
}


/*
    Check the CGI process.  Return 0 if it does not exist; non 0 if it does.
 */
static int checkCgi(Cgi *cgip)
{
    DWORD exitCode;
    int   nReturn;

    nReturn = GetExitCodeProcess((HANDLE) cgip->handle, &exitCode);
    /*
        We must close process handle to free up the window resource, but only
        when we're done with it.
     */
    if ((nReturn == 0) || (exitCode != STILL_ACTIVE)) {
        CloseHandle((HANDLE) cgip->handle);
        return 0;
    }
    return 1;
}


/*
    Terminate an orphaned CGI process and release its handle.
 */
static void killCgi(Cgi *cgip)
{
    if (cgip->handle != NULL && cgip->handle != INVALID_HANDLE_VALUE) {
        TerminateProcess((HANDLE) cgip->handle, 0);
        closeWinHandle((HANDLE) cgip->handle);
    }
}
#endif /* WIN */
#endif /* ME_GOAHEAD_CGI */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/crypt.c ************/

/*
    crypt.c - Base-64 encoding/decoding, MD5 and SHA-1 digests, Blowfish password hashing and random bytes.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

#define BLOWFISH_SALT_LENGTH 16
#define BLOWFISH_ROUNDS      128

/*
    Constants for transform routine
 */
#define S11                  7
#define S12                  12
#define S13                  17
#define S14                  22
#define S21                  5
#define S22                  9
#define S23                  14
#define S24                  20
#define S31                  4
#define S32                  11
#define S33                  16
#define S34                  23
#define S41                  6
#define S42                  10
#define S43                  15
#define S44                  21

static uchar PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
   F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z)               (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z)               (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z)               ((x) ^ (y) ^ (z))
#define I(x, y, z)               ((y) ^ ((x) | (~z)))

/*
   ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n)        (((x) << (n)) | ((x) >> (32 - (n))))

/*
     FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
     Rotation is separate from addition to prevent recomputation.
 */

#define FF(a, b, c, d, x, s, ac) { \
        (a) += F((b), (c), (d)) + (x) + (uint) (ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
        (a) += G((b), (c), (d)) + (x) + (uint) (ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
        (a) += H((b), (c), (d)) + (x) + (uint) (ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
        (a) += I((b), (c), (d)) + (x) + (uint) (ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
}

typedef struct {
    uint state[4];
    uint count[2];
    uchar buffer[64];
} MD5CONTEXT;

/*
    SHA-1 is used only to compress arbitrary-length password material to a fixed-width key for the
    Blowfish schedule below, so it is not part of the public API.
 */
#define SHA_SIZE 20

#define shaShift(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))

typedef struct {
    uint hash[SHA_SIZE / 4];                /* Message digest */
    uint lowLength;                         /* Message length in bits */
    uint highLength;                        /* Message length in bits */
    int index;                              /* Index into the message block */
    uchar block[64];                        /* 512-bit message block */
} WebsSha;

/******************************* Base 64 Data *********************************/

#define CRYPT_HASH_SIZE 16

/*
    Encoding map lookup
 */
static char encodeMap[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
};


/*
    Decode map
 */
static signed char decodeMap[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/*************************** Forward Declarations *****************************/

static void decode(uint *output, uchar *input, uint len);
static void encode(uchar *output, uint *input, uint len);
static void finalizeMD5(uchar digest[16], MD5CONTEXT *context);
static void initMD5(MD5CONTEXT *context);
static void transform(uint state[4], uchar block[64]);
static void update(MD5CONTEXT *context, uchar *input, uint inputLen);

static void shaFinalize(uchar *digest, WebsSha *sha);
static void shaInit(WebsSha *sha);
static void shaPad(WebsSha *sha);
static void shaProcess(WebsSha *sha);
static void shaUpdate(WebsSha *sha, uchar *msg, ssize len);

/*********************************** Code *************************************/
/*
    Decode a null terminated string and returns a null terminated string.
    Stops decoding at the end of string or '='
 */
PUBLIC char *websDecode64(char *s)
{
    return websDecode64Block(s, NULL, WEBS_DECODE_TOKEQ);
}


/*
    Decode a null terminated string and return a block with length.
    Stops decoding at the end of the block or '=' if WEBS_DECODE_TOKEQ is specified.
 */
PUBLIC char *websDecode64Block(char *s, ssize *len, int flags)
{
    uint  bitBuf;
    char  *buffer, *bp;
    char  *end;
    ssize size;
    int   c, i, j, shift;

    size = strlen(s);
    if ((buffer = walloc(size + 1)) == 0) {
        return NULL;
    }
    bp = buffer;
    *bp = '\0';
    end = &s[size];
    while (s < end && (*s != '=' || !(flags & WEBS_DECODE_TOKEQ))) {
        bitBuf = 0;
        shift = 18;
        for (i = 0; i < 4 && (s < end && (*s != '=' || !(flags & WEBS_DECODE_TOKEQ))); i++, s++) {
            c = decodeMap[*s & 0xff];
            if (c == -1) {
                wfree(buffer);
                return NULL;
            }
            bitBuf = bitBuf | (c << shift);
            shift -= 6;
        }
        --i;
        assert((bp + i) < &buffer[size]);
        for (j = 0; j < i; j++) {
            *bp++ = (char) ((bitBuf >> (8 * (2 - j))) & 0xff);
        }
        *bp = '\0';
    }
    if (len) {
        *len = bp - buffer;
    }
    return buffer;
}


PUBLIC char *websMD5(cchar *s)
{
    return websMD5Block(s, strlen(s), NULL);
}


/*
    Return the MD5 hash of a block. Returns allocated string. A prefix for the result can be supplied.
 */
PUBLIC char *websMD5Block(cchar *buf, ssize length, cchar *prefix)
{
    MD5CONTEXT context;
    uchar      hash[CRYPT_HASH_SIZE];
    cchar      *hexDigits = "0123456789abcdef";
    char       *r, *str;
    char       result[(CRYPT_HASH_SIZE * 2) + 1];
    ssize      len;
    int        i;

    if (length < 0) {
        length = strlen(buf);
    }
    initMD5(&context);
    update(&context, (uchar*) buf, (uint) length);
    finalizeMD5(hash, &context);

    for (i = 0, r = result; i < 16; i++) {
        *r++ = hexDigits[hash[i] >> 4];
        *r++ = hexDigits[hash[i] & 0xF];
    }
    *r = '\0';
    len = (prefix) ? strlen(prefix) : 0;
    str = walloc(sizeof(result) + len);
    if (str) {
        if (prefix) {
            strcpy(str, prefix);
        }
        strcpy(str + len, result);
    }
    return str;
}


/*
    MD5 initialization. Begins an MD5 operation, writing a new context.
 */
static void initMD5(MD5CONTEXT *context)
{
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}


/*
    MD5 block update operation. Continues an MD5 message-digest operation, processing another message block,
    and updating the context.
 */
static void update(MD5CONTEXT *context, uchar *input, uint inputLen)
{
    uint i, index, partLen;

    index = (uint) ((context->count[0] >> 3) & 0x3F);

    if ((context->count[0] += ((uint) inputLen << 3)) < ((uint) inputLen << 3)) {
        context->count[1]++;
    }
    context->count[1] += ((uint) inputLen >> 29);
    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy((uchar*) &context->buffer[index], (uchar*) input, partLen);
        transform(context->state, context->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64) {
            transform(context->state, &input[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy((uchar*) &context->buffer[index], (uchar*) &input[i], inputLen - i);
}


/*
    MD5 finalization. Ends an MD5 message-digest operation, writing the message digest and zeroizing the context.
 */
static void finalizeMD5(uchar digest[16], MD5CONTEXT *context)
{
    uchar bits[8];
    uint  index, padLen;

    /* Save number of bits */
    encode(bits, context->count, 8);

    /* Pad out to 56 mod 64. */
    index = (uint) ((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    update(context, PADDING, padLen);

    /* Append length (before padding) */
    update(context, bits, 8);
    /* Store state in digest */
    encode(digest, context->state, 16);

    /* Zero sensitive information. */
    memset((uchar*) context, 0, sizeof(*context));
}


/*
    MD5 basic transformation. Transforms state based on block.
 */
static ME_NO_SANITIZE_WRAP void transform(uint state[4], uchar block[64])
{
    uint a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    decode(x, block, 64);

    /* Round 1 */
    FF(a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    FF(b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, x[10], S22,  0x2441453); /* 22 */
    GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH(a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II(a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    II(d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /* Zero sensitive information. */
    memset((uchar*) x, 0, sizeof(x));
}


/*
    Encodes input(uint) into output(uchar). Assumes len is a multiple of 4.
 */
static void encode(uchar *output, uint *input, uint len)
{
    uint i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = (uchar) (input[i] & 0xff);
        output[j + 1] = (uchar) ((input[i] >> 8) & 0xff);
        output[j + 2] = (uchar) ((input[i] >> 16) & 0xff);
        output[j + 3] = (uchar) ((input[i] >> 24) & 0xff);
    }
}


/*
    Decodes input(uchar) into output(uint). Assumes len is a multiple of 4.
 */
static void decode(uint *output, uchar *input, uint len)
{
    uint i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] = ((uint) input[j]) | (((uint) input[j + 1]) << 8) | (((uint) input[j + 2]) << 16) |
                    (((uint) input[j + 3]) << 24);
}


/*
    Encode a null terminated string.
    Returns a null terminated block
 */
PUBLIC char *websEncode64(char *s)
{
    return websEncode64Block(s, slen(s));
}


/*
    Encode a block of a given length
    Returns a null terminated block
 */
PUBLIC char *websEncode64Block(char *s, ssize len)
{
    uint  shiftbuf;
    char  *buffer, *bp;
    cchar *end;
    ssize size;
    int   i, j, shift;

    /* Allow four output bytes per input triplet, plus the terminator. */
    if (len < 0 || len / 3 > (SSIZE_MAX - 5) / 4) {
        return NULL;
    }
    size = (len / 3 + (len % 3 != 0)) * 4;
    if ((buffer = walloc(size + 1)) == 0) {
        return NULL;
    }
    bp = buffer;
    *bp = '\0';
    end = &s[len];
    while (s < end) {
        shiftbuf = 0;
        for (j = 2; j >= 0 && s < end; j--, s++) {
            shiftbuf |= ((*s & 0xff) << (j * 8));
        }
        shift = 18;
        for (i = ++j; i < 4 && bp < &buffer[size] ; i++) {
            *bp++ = encodeMap[(shiftbuf >> shift) & 0x3f];
            shift -= 6;
        }
        while (j-- > 0) {
            *bp++ = '=';
        }
        *bp = '\0';
    }
    return buffer;
}

/************************************* Sha1 **********************************/
/*
    Return the SHA-1 hash of a block as a null terminated hex string. Returns an allocated string of
    exactly SHA_SIZE * 2 characters whatever the length of the input.
 */
static char *shaHex(cchar *buf, ssize length)
{
    WebsSha sha;
    uchar   hash[SHA_SIZE];
    cchar   *hexDigits = "0123456789abcdef";
    char    *r, *str;
    char    result[(SHA_SIZE * 2) + 1];
    int     i;

    if (length < 0) {
        length = slen(buf);
    }
    shaInit(&sha);
    shaUpdate(&sha, (uchar*) buf, length);
    shaFinalize(hash, &sha);

    for (i = 0, r = result; i < SHA_SIZE; i++) {
        *r++ = hexDigits[hash[i] >> 4];
        *r++ = hexDigits[hash[i] & 0xF];
    }
    *r = '\0';
    str = walloc(sizeof(result));
    if (str) {
        strcpy(str, result);
    }
    memset(&sha, 0, sizeof(sha));
    memset(hash, 0, sizeof(hash));
    return str;
}


static void shaInit(WebsSha *sha)
{
    sha->lowLength = 0;
    sha->highLength = 0;
    sha->index = 0;
    sha->hash[0] = 0x67452301;
    sha->hash[1] = 0xEFCDAB89;
    sha->hash[2] = 0x98BADCFE;
    sha->hash[3] = 0x10325476;
    sha->hash[4] = 0xC3D2E1F0;
}


static void shaUpdate(WebsSha *sha, uchar *msg, ssize len)
{
    while (len--) {
        sha->block[sha->index++] = (*msg & 0xFF);
        sha->lowLength += 8;
        if (sha->lowLength == 0) {
            sha->highLength++;
        }
        if (sha->index == 64) {
            shaProcess(sha);
        }
        msg++;
    }
}


static void shaFinalize(uchar *digest, WebsSha *sha)
{
    int i;

    shaPad(sha);
    memset(sha->block, 0, 64);
    sha->lowLength = 0;
    sha->highLength = 0;
    for (i = 0; i < SHA_SIZE; i++) {
        digest[i] = (uchar) (sha->hash[i >> 2] >> 8 * (3 - (i & 0x03)));
    }
}


static ME_NO_SANITIZE_WRAP void shaProcess(WebsSha *sha)
{
    uint K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
    uint temp, W[80], A, B, C, D, E;
    int  t;

    for (t = 0; t < 16; t++) {
        // Widen before shifting to avoid signed-int overflow.
        W[t] = (uint) sha->block[t * 4] << 24;
        W[t] |= (uint) sha->block[t * 4 + 1] << 16;
        W[t] |= (uint) sha->block[t * 4 + 2] << 8;
        W[t] |= sha->block[t * 4 + 3];
    }
    for (t = 16; t < 80; t++) {
        W[t] = shaShift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
    }
    A = sha->hash[0];
    B = sha->hash[1];
    C = sha->hash[2];
    D = sha->hash[3];
    E = sha->hash[4];

    for (t = 0; t < 20; t++) {
        temp = shaShift(5, A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = shaShift(30, B);
        B = A;
        A = temp;
    }
    for (t = 20; t < 40; t++) {
        temp = shaShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = shaShift(30, B);
        B = A;
        A = temp;
    }
    for (t = 40; t < 60; t++) {
        temp = shaShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = shaShift(30, B);
        B = A;
        A = temp;
    }
    for (t = 60; t < 80; t++) {
        temp = shaShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = shaShift(30, B);
        B = A;
        A = temp;
    }
    sha->hash[0] += A;
    sha->hash[1] += B;
    sha->hash[2] += C;
    sha->hash[3] += D;
    sha->hash[4] += E;
    sha->index = 0;
}


static void shaPad(WebsSha *sha)
{
    if (sha->index > 55) {
        sha->block[sha->index++] = 0x80;
        while (sha->index < 64) {
            sha->block[sha->index++] = 0;
        }
        shaProcess(sha);
        while (sha->index < 56) {
            sha->block[sha->index++] = 0;
        }
    } else {
        sha->block[sha->index++] = 0x80;
        while (sha->index < 56) {
            sha->block[sha->index++] = 0;
        }
    }
    sha->block[56] = (uchar) (sha->highLength >> 24);
    sha->block[57] = (uchar) (sha->highLength >> 16);
    sha->block[58] = (uchar) (sha->highLength >> 8);
    sha->block[59] = (uchar) (sha->highLength);
    sha->block[60] = (uchar) (sha->lowLength >> 24);
    sha->block[61] = (uchar) (sha->lowLength >> 16);
    sha->block[62] = (uchar) (sha->lowLength >> 8);
    sha->block[63] = (uchar) (sha->lowLength);
    shaProcess(sha);
}

/************************************ Blowfish *******************************/

/*
    The key schedule below consumes exactly (BF_ROUNDS + 2) * 4 == WEBS_BLOWFISH_KEY_WINDOW key
    bytes and ignores everything after them. This is a property of Blowfish, not of this
    implementation - real bcrypt has the same window.
 */
#define BF_ROUNDS 16

typedef struct {
    uint P[16 + 2];
    uint S[4][256];
} WebsBlowfish;

static const uint ORIG_P[16 + 2] = {
    0x243F6A88L, 0x85A308D3L, 0x13198A2EL, 0x03707344L,
    0xA4093822L, 0x299F31D0L, 0x082EFA98L, 0xEC4E6C89L,
    0x452821E6L, 0x38D01377L, 0xBE5466CFL, 0x34E90C6CL,
    0xC0AC29B7L, 0xC97C50DDL, 0x3F84D5B5L, 0xB5470917L,
    0x9216D5D9L, 0x8979FB1BL
};

/*
    Digits of PI
 */
static const uint ORIG_S[4][256] = {
    {   0xD1310BA6L, 0x98DFB5ACL, 0x2FFD72DBL, 0xD01ADFB7L,
        0xB8E1AFEDL, 0x6A267E96L, 0xBA7C9045L, 0xF12C7F99L,
        0x24A19947L, 0xB3916CF7L, 0x0801F2E2L, 0x858EFC16L,
        0x636920D8L, 0x71574E69L, 0xA458FEA3L, 0xF4933D7EL,
        0x0D95748FL, 0x728EB658L, 0x718BCD58L, 0x82154AEEL,
        0x7B54A41DL, 0xC25A59B5L, 0x9C30D539L, 0x2AF26013L,
        0xC5D1B023L, 0x286085F0L, 0xCA417918L, 0xB8DB38EFL,
        0x8E79DCB0L, 0x603A180EL, 0x6C9E0E8BL, 0xB01E8A3EL,
        0xD71577C1L, 0xBD314B27L, 0x78AF2FDAL, 0x55605C60L,
        0xE65525F3L, 0xAA55AB94L, 0x57489862L, 0x63E81440L,
        0x55CA396AL, 0x2AAB10B6L, 0xB4CC5C34L, 0x1141E8CEL,
        0xA15486AFL, 0x7C72E993L, 0xB3EE1411L, 0x636FBC2AL,
        0x2BA9C55DL, 0x741831F6L, 0xCE5C3E16L, 0x9B87931EL,
        0xAFD6BA33L, 0x6C24CF5CL, 0x7A325381L, 0x28958677L,
        0x3B8F4898L, 0x6B4BB9AFL, 0xC4BFE81BL, 0x66282193L,
        0x61D809CCL, 0xFB21A991L, 0x487CAC60L, 0x5DEC8032L,
        0xEF845D5DL, 0xE98575B1L, 0xDC262302L, 0xEB651B88L,
        0x23893E81L, 0xD396ACC5L, 0x0F6D6FF3L, 0x83F44239L,
        0x2E0B4482L, 0xA4842004L, 0x69C8F04AL, 0x9E1F9B5EL,
        0x21C66842L, 0xF6E96C9AL, 0x670C9C61L, 0xABD388F0L,
        0x6A51A0D2L, 0xD8542F68L, 0x960FA728L, 0xAB5133A3L,
        0x6EEF0B6CL, 0x137A3BE4L, 0xBA3BF050L, 0x7EFB2A98L,
        0xA1F1651DL, 0x39AF0176L, 0x66CA593EL, 0x82430E88L,
        0x8CEE8619L, 0x456F9FB4L, 0x7D84A5C3L, 0x3B8B5EBEL,
        0xE06F75D8L, 0x85C12073L, 0x401A449FL, 0x56C16AA6L,
        0x4ED3AA62L, 0x363F7706L, 0x1BFEDF72L, 0x429B023DL,
        0x37D0D724L, 0xD00A1248L, 0xDB0FEAD3L, 0x49F1C09BL,
        0x075372C9L, 0x80991B7BL, 0x25D479D8L, 0xF6E8DEF7L,
        0xE3FE501AL, 0xB6794C3BL, 0x976CE0BDL, 0x04C006BAL,
        0xC1A94FB6L, 0x409F60C4L, 0x5E5C9EC2L, 0x196A2463L,
        0x68FB6FAFL, 0x3E6C53B5L, 0x1339B2EBL, 0x3B52EC6FL,
        0x6DFC511FL, 0x9B30952CL, 0xCC814544L, 0xAF5EBD09L,
        0xBEE3D004L, 0xDE334AFDL, 0x660F2807L, 0x192E4BB3L,
        0xC0CBA857L, 0x45C8740FL, 0xD20B5F39L, 0xB9D3FBDBL,
        0x5579C0BDL, 0x1A60320AL, 0xD6A100C6L, 0x402C7279L,
        0x679F25FEL, 0xFB1FA3CCL, 0x8EA5E9F8L, 0xDB3222F8L,
        0x3C7516DFL, 0xFD616B15L, 0x2F501EC8L, 0xAD0552ABL,
        0x323DB5FAL, 0xFD238760L, 0x53317B48L, 0x3E00DF82L,
        0x9E5C57BBL, 0xCA6F8CA0L, 0x1A87562EL, 0xDF1769DBL,
        0xD542A8F6L, 0x287EFFC3L, 0xAC6732C6L, 0x8C4F5573L,
        0x695B27B0L, 0xBBCA58C8L, 0xE1FFA35DL, 0xB8F011A0L,
        0x10FA3D98L, 0xFD2183B8L, 0x4AFCB56CL, 0x2DD1D35BL,
        0x9A53E479L, 0xB6F84565L, 0xD28E49BCL, 0x4BFB9790L,
        0xE1DDF2DAL, 0xA4CB7E33L, 0x62FB1341L, 0xCEE4C6E8L,
        0xEF20CADAL, 0x36774C01L, 0xD07E9EFEL, 0x2BF11FB4L,
        0x95DBDA4DL, 0xAE909198L, 0xEAAD8E71L, 0x6B93D5A0L,
        0xD08ED1D0L, 0xAFC725E0L, 0x8E3C5B2FL, 0x8E7594B7L,
        0x8FF6E2FBL, 0xF2122B64L, 0x8888B812L, 0x900DF01CL,
        0x4FAD5EA0L, 0x688FC31CL, 0xD1CFF191L, 0xB3A8C1ADL,
        0x2F2F2218L, 0xBE0E1777L, 0xEA752DFEL, 0x8B021FA1L,
        0xE5A0CC0FL, 0xB56F74E8L, 0x18ACF3D6L, 0xCE89E299L,
        0xB4A84FE0L, 0xFD13E0B7L, 0x7CC43B81L, 0xD2ADA8D9L,
        0x165FA266L, 0x80957705L, 0x93CC7314L, 0x211A1477L,
        0xE6AD2065L, 0x77B5FA86L, 0xC75442F5L, 0xFB9D35CFL,
        0xEBCDAF0CL, 0x7B3E89A0L, 0xD6411BD3L, 0xAE1E7E49L,
        0x00250E2DL, 0x2071B35EL, 0x226800BBL, 0x57B8E0AFL,
        0x2464369BL, 0xF009B91EL, 0x5563911DL, 0x59DFA6AAL,
        0x78C14389L, 0xD95A537FL, 0x207D5BA2L, 0x02E5B9C5L,
        0x83260376L, 0x6295CFA9L, 0x11C81968L, 0x4E734A41L,
        0xB3472DCAL, 0x7B14A94AL, 0x1B510052L, 0x9A532915L,
        0xD60F573FL, 0xBC9BC6E4L, 0x2B60A476L, 0x81E67400L,
        0x08BA6FB5L, 0x571BE91FL, 0xF296EC6BL, 0x2A0DD915L,
        0xB6636521L, 0xE7B9F9B6L, 0xFF34052EL, 0xC5855664L,
        0x53B02D5DL, 0xA99F8FA1L, 0x08BA4799L, 0x6E85076AL }, {
        0x4B7A70E9L, 0xB5B32944L, 0xDB75092EL, 0xC4192623L,
        0xAD6EA6B0L, 0x49A7DF7DL, 0x9CEE60B8L, 0x8FEDB266L,
        0xECAA8C71L, 0x699A17FFL, 0x5664526CL, 0xC2B19EE1L,
        0x193602A5L, 0x75094C29L, 0xA0591340L, 0xE4183A3EL,
        0x3F54989AL, 0x5B429D65L, 0x6B8FE4D6L, 0x99F73FD6L,
        0xA1D29C07L, 0xEFE830F5L, 0x4D2D38E6L, 0xF0255DC1L,
        0x4CDD2086L, 0x8470EB26L, 0x6382E9C6L, 0x021ECC5EL,
        0x09686B3FL, 0x3EBAEFC9L, 0x3C971814L, 0x6B6A70A1L,
        0x687F3584L, 0x52A0E286L, 0xB79C5305L, 0xAA500737L,
        0x3E07841CL, 0x7FDEAE5CL, 0x8E7D44ECL, 0x5716F2B8L,
        0xB03ADA37L, 0xF0500C0DL, 0xF01C1F04L, 0x0200B3FFL,
        0xAE0CF51AL, 0x3CB574B2L, 0x25837A58L, 0xDC0921BDL,
        0xD19113F9L, 0x7CA92FF6L, 0x94324773L, 0x22F54701L,
        0x3AE5E581L, 0x37C2DADCL, 0xC8B57634L, 0x9AF3DDA7L,
        0xA9446146L, 0x0FD0030EL, 0xECC8C73EL, 0xA4751E41L,
        0xE238CD99L, 0x3BEA0E2FL, 0x3280BBA1L, 0x183EB331L,
        0x4E548B38L, 0x4F6DB908L, 0x6F420D03L, 0xF60A04BFL,
        0x2CB81290L, 0x24977C79L, 0x5679B072L, 0xBCAF89AFL,
        0xDE9A771FL, 0xD9930810L, 0xB38BAE12L, 0xDCCF3F2EL,
        0x5512721FL, 0x2E6B7124L, 0x501ADDE6L, 0x9F84CD87L,
        0x7A584718L, 0x7408DA17L, 0xBC9F9ABCL, 0xE94B7D8CL,
        0xEC7AEC3AL, 0xDB851DFAL, 0x63094366L, 0xC464C3D2L,
        0xEF1C1847L, 0x3215D908L, 0xDD433B37L, 0x24C2BA16L,
        0x12A14D43L, 0x2A65C451L, 0x50940002L, 0x133AE4DDL,
        0x71DFF89EL, 0x10314E55L, 0x81AC77D6L, 0x5F11199BL,
        0x043556F1L, 0xD7A3C76BL, 0x3C11183BL, 0x5924A509L,
        0xF28FE6EDL, 0x97F1FBFAL, 0x9EBABF2CL, 0x1E153C6EL,
        0x86E34570L, 0xEAE96FB1L, 0x860E5E0AL, 0x5A3E2AB3L,
        0x771FE71CL, 0x4E3D06FAL, 0x2965DCB9L, 0x99E71D0FL,
        0x803E89D6L, 0x5266C825L, 0x2E4CC978L, 0x9C10B36AL,
        0xC6150EBAL, 0x94E2EA78L, 0xA5FC3C53L, 0x1E0A2DF4L,
        0xF2F74EA7L, 0x361D2B3DL, 0x1939260FL, 0x19C27960L,
        0x5223A708L, 0xF71312B6L, 0xEBADFE6EL, 0xEAC31F66L,
        0xE3BC4595L, 0xA67BC883L, 0xB17F37D1L, 0x018CFF28L,
        0xC332DDEFL, 0xBE6C5AA5L, 0x65582185L, 0x68AB9802L,
        0xEECEA50FL, 0xDB2F953BL, 0x2AEF7DADL, 0x5B6E2F84L,
        0x1521B628L, 0x29076170L, 0xECDD4775L, 0x619F1510L,
        0x13CCA830L, 0xEB61BD96L, 0x0334FE1EL, 0xAA0363CFL,
        0xB5735C90L, 0x4C70A239L, 0xD59E9E0BL, 0xCBAADE14L,
        0xEECC86BCL, 0x60622CA7L, 0x9CAB5CABL, 0xB2F3846EL,
        0x648B1EAFL, 0x19BDF0CAL, 0xA02369B9L, 0x655ABB50L,
        0x40685A32L, 0x3C2AB4B3L, 0x319EE9D5L, 0xC021B8F7L,
        0x9B540B19L, 0x875FA099L, 0x95F7997EL, 0x623D7DA8L,
        0xF837889AL, 0x97E32D77L, 0x11ED935FL, 0x16681281L,
        0x0E358829L, 0xC7E61FD6L, 0x96DEDFA1L, 0x7858BA99L,
        0x57F584A5L, 0x1B227263L, 0x9B83C3FFL, 0x1AC24696L,
        0xCDB30AEBL, 0x532E3054L, 0x8FD948E4L, 0x6DBC3128L,
        0x58EBF2EFL, 0x34C6FFEAL, 0xFE28ED61L, 0xEE7C3C73L,
        0x5D4A14D9L, 0xE864B7E3L, 0x42105D14L, 0x203E13E0L,
        0x45EEE2B6L, 0xA3AAABEAL, 0xDB6C4F15L, 0xFACB4FD0L,
        0xC742F442L, 0xEF6ABBB5L, 0x654F3B1DL, 0x41CD2105L,
        0xD81E799EL, 0x86854DC7L, 0xE44B476AL, 0x3D816250L,
        0xCF62A1F2L, 0x5B8D2646L, 0xFC8883A0L, 0xC1C7B6A3L,
        0x7F1524C3L, 0x69CB7492L, 0x47848A0BL, 0x5692B285L,
        0x095BBF00L, 0xAD19489DL, 0x1462B174L, 0x23820E00L,
        0x58428D2AL, 0x0C55F5EAL, 0x1DADF43EL, 0x233F7061L,
        0x3372F092L, 0x8D937E41L, 0xD65FECF1L, 0x6C223BDBL,
        0x7CDE3759L, 0xCBEE7460L, 0x4085F2A7L, 0xCE77326EL,
        0xA6078084L, 0x19F8509EL, 0xE8EFD855L, 0x61D99735L,
        0xA969A7AAL, 0xC50C06C2L, 0x5A04ABFCL, 0x800BCADCL,
        0x9E447A2EL, 0xC3453484L, 0xFDD56705L, 0x0E1E9EC9L,
        0xDB73DBD3L, 0x105588CDL, 0x675FDA79L, 0xE3674340L,
        0xC5C43465L, 0x713E38D8L, 0x3D28F89EL, 0xF16DFF20L,
        0x153E21E7L, 0x8FB03D4AL, 0xE6E39F2BL, 0xDB83ADF7L
    }, {
        0xE93D5A68L, 0x948140F7L, 0xF64C261CL, 0x94692934L,
        0x411520F7L, 0x7602D4F7L, 0xBCF46B2EL, 0xD4A20068L,
        0xD4082471L, 0x3320F46AL, 0x43B7D4B7L, 0x500061AFL,
        0x1E39F62EL, 0x97244546L, 0x14214F74L, 0xBF8B8840L,
        0x4D95FC1DL, 0x96B591AFL, 0x70F4DDD3L, 0x66A02F45L,
        0xBFBC09ECL, 0x03BD9785L, 0x7FAC6DD0L, 0x31CB8504L,
        0x96EB27B3L, 0x55FD3941L, 0xDA2547E6L, 0xABCA0A9AL,
        0x28507825L, 0x530429F4L, 0x0A2C86DAL, 0xE9B66DFBL,
        0x68DC1462L, 0xD7486900L, 0x680EC0A4L, 0x27A18DEEL,
        0x4F3FFEA2L, 0xE887AD8CL, 0xB58CE006L, 0x7AF4D6B6L,
        0xAACE1E7CL, 0xD3375FECL, 0xCE78A399L, 0x406B2A42L,
        0x20FE9E35L, 0xD9F385B9L, 0xEE39D7ABL, 0x3B124E8BL,
        0x1DC9FAF7L, 0x4B6D1856L, 0x26A36631L, 0xEAE397B2L,
        0x3A6EFA74L, 0xDD5B4332L, 0x6841E7F7L, 0xCA7820FBL,
        0xFB0AF54EL, 0xD8FEB397L, 0x454056ACL, 0xBA489527L,
        0x55533A3AL, 0x20838D87L, 0xFE6BA9B7L, 0xD096954BL,
        0x55A867BCL, 0xA1159A58L, 0xCCA92963L, 0x99E1DB33L,
        0xA62A4A56L, 0x3F3125F9L, 0x5EF47E1CL, 0x9029317CL,
        0xFDF8E802L, 0x04272F70L, 0x80BB155CL, 0x05282CE3L,
        0x95C11548L, 0xE4C66D22L, 0x48C1133FL, 0xC70F86DCL,
        0x07F9C9EEL, 0x41041F0FL, 0x404779A4L, 0x5D886E17L,
        0x325F51EBL, 0xD59BC0D1L, 0xF2BCC18FL, 0x41113564L,
        0x257B7834L, 0x602A9C60L, 0xDFF8E8A3L, 0x1F636C1BL,
        0x0E12B4C2L, 0x02E1329EL, 0xAF664FD1L, 0xCAD18115L,
        0x6B2395E0L, 0x333E92E1L, 0x3B240B62L, 0xEEBEB922L,
        0x85B2A20EL, 0xE6BA0D99L, 0xDE720C8CL, 0x2DA2F728L,
        0xD0127845L, 0x95B794FDL, 0x647D0862L, 0xE7CCF5F0L,
        0x5449A36FL, 0x877D48FAL, 0xC39DFD27L, 0xF33E8D1EL,
        0x0A476341L, 0x992EFF74L, 0x3A6F6EABL, 0xF4F8FD37L,
        0xA812DC60L, 0xA1EBDDF8L, 0x991BE14CL, 0xDB6E6B0DL,
        0xC67B5510L, 0x6D672C37L, 0x2765D43BL, 0xDCD0E804L,
        0xF1290DC7L, 0xCC00FFA3L, 0xB5390F92L, 0x690FED0BL,
        0x667B9FFBL, 0xCEDB7D9CL, 0xA091CF0BL, 0xD9155EA3L,
        0xBB132F88L, 0x515BAD24L, 0x7B9479BFL, 0x763BD6EBL,
        0x37392EB3L, 0xCC115979L, 0x8026E297L, 0xF42E312DL,
        0x6842ADA7L, 0xC66A2B3BL, 0x12754CCCL, 0x782EF11CL,
        0x6A124237L, 0xB79251E7L, 0x06A1BBE6L, 0x4BFB6350L,
        0x1A6B1018L, 0x11CAEDFAL, 0x3D25BDD8L, 0xE2E1C3C9L,
        0x44421659L, 0x0A121386L, 0xD90CEC6EL, 0xD5ABEA2AL,
        0x64AF674EL, 0xDA86A85FL, 0xBEBFE988L, 0x64E4C3FEL,
        0x9DBC8057L, 0xF0F7C086L, 0x60787BF8L, 0x6003604DL,
        0xD1FD8346L, 0xF6381FB0L, 0x7745AE04L, 0xD736FCCCL,
        0x83426B33L, 0xF01EAB71L, 0xB0804187L, 0x3C005E5FL,
        0x77A057BEL, 0xBDE8AE24L, 0x55464299L, 0xBF582E61L,
        0x4E58F48FL, 0xF2DDFDA2L, 0xF474EF38L, 0x8789BDC2L,
        0x5366F9C3L, 0xC8B38E74L, 0xB475F255L, 0x46FCD9B9L,
        0x7AEB2661L, 0x8B1DDF84L, 0x846A0E79L, 0x915F95E2L,
        0x466E598EL, 0x20B45770L, 0x8CD55591L, 0xC902DE4CL,
        0xB90BACE1L, 0xBB8205D0L, 0x11A86248L, 0x7574A99EL,
        0xB77F19B6L, 0xE0A9DC09L, 0x662D09A1L, 0xC4324633L,
        0xE85A1F02L, 0x09F0BE8CL, 0x4A99A025L, 0x1D6EFE10L,
        0x1AB93D1DL, 0x0BA5A4DFL, 0xA186F20FL, 0x2868F169L,
        0xDCB7DA83L, 0x573906FEL, 0xA1E2CE9BL, 0x4FCD7F52L,
        0x50115E01L, 0xA70683FAL, 0xA002B5C4L, 0x0DE6D027L,
        0x9AF88C27L, 0x773F8641L, 0xC3604C06L, 0x61A806B5L,
        0xF0177A28L, 0xC0F586E0L, 0x006058AAL, 0x30DC7D62L,
        0x11E69ED7L, 0x2338EA63L, 0x53C2DD94L, 0xC2C21634L,
        0xBBCBEE56L, 0x90BCB6DEL, 0xEBFC7DA1L, 0xCE591D76L,
        0x6F05E409L, 0x4B7C0188L, 0x39720A3DL, 0x7C927C24L,
        0x86E3725FL, 0x724D9DB9L, 0x1AC15BB4L, 0xD39EB8FCL,
        0xED545578L, 0x08FCA5B5L, 0xD83D7CD3L, 0x4DAD0FC4L,
        0x1E50EF5EL, 0xB161E6F8L, 0xA28514D9L, 0x6C51133CL,
        0x6FD5C7E7L, 0x56E14EC4L, 0x362ABFCEL, 0xDDC6C837L,
        0xD79A3234L, 0x92638212L, 0x670EFA8EL, 0x406000E0L
    }, {
        0x3A39CE37L, 0xD3FAF5CFL, 0xABC27737L, 0x5AC52D1BL,
        0x5CB0679EL, 0x4FA33742L, 0xD3822740L, 0x99BC9BBEL,
        0xD5118E9DL, 0xBF0F7315L, 0xD62D1C7EL, 0xC700C47BL,
        0xB78C1B6BL, 0x21A19045L, 0xB26EB1BEL, 0x6A366EB4L,
        0x5748AB2FL, 0xBC946E79L, 0xC6A376D2L, 0x6549C2C8L,
        0x530FF8EEL, 0x468DDE7DL, 0xD5730A1DL, 0x4CD04DC6L,
        0x2939BBDBL, 0xA9BA4650L, 0xAC9526E8L, 0xBE5EE304L,
        0xA1FAD5F0L, 0x6A2D519AL, 0x63EF8CE2L, 0x9A86EE22L,
        0xC089C2B8L, 0x43242EF6L, 0xA51E03AAL, 0x9CF2D0A4L,
        0x83C061BAL, 0x9BE96A4DL, 0x8FE51550L, 0xBA645BD6L,
        0x2826A2F9L, 0xA73A3AE1L, 0x4BA99586L, 0xEF5562E9L,
        0xC72FEFD3L, 0xF752F7DAL, 0x3F046F69L, 0x77FA0A59L,
        0x80E4A915L, 0x87B08601L, 0x9B09E6ADL, 0x3B3EE593L,
        0xE990FD5AL, 0x9E34D797L, 0x2CF0B7D9L, 0x022B8B51L,
        0x96D5AC3AL, 0x017DA67DL, 0xD1CF3ED6L, 0x7C7D2D28L,
        0x1F9F25CFL, 0xADF2B89BL, 0x5AD6B472L, 0x5A88F54CL,
        0xE029AC71L, 0xE019A5E6L, 0x47B0ACFDL, 0xED93FA9BL,
        0xE8D3C48DL, 0x283B57CCL, 0xF8D56629L, 0x79132E28L,
        0x785F0191L, 0xED756055L, 0xF7960E44L, 0xE3D35E8CL,
        0x15056DD4L, 0x88F46DBAL, 0x03A16125L, 0x0564F0BDL,
        0xC3EB9E15L, 0x3C9057A2L, 0x97271AECL, 0xA93A072AL,
        0x1B3F6D9BL, 0x1E6321F5L, 0xF59C66FBL, 0x26DCF319L,
        0x7533D928L, 0xB155FDF5L, 0x03563482L, 0x8ABA3CBBL,
        0x28517711L, 0xC20AD9F8L, 0xABCC5167L, 0xCCAD925FL,
        0x4DE81751L, 0x3830DC8EL, 0x379D5862L, 0x9320F991L,
        0xEA7A90C2L, 0xFB3E7BCEL, 0x5121CE64L, 0x774FBE32L,
        0xA8B6E37EL, 0xC3293D46L, 0x48DE5369L, 0x6413E680L,
        0xA2AE0810L, 0xDD6DB224L, 0x69852DFDL, 0x09072166L,
        0xB39A460AL, 0x6445C0DDL, 0x586CDECFL, 0x1C20C8AEL,
        0x5BBEF7DDL, 0x1B588D40L, 0xCCD2017FL, 0x6BB4E3BBL,
        0xDDA26A7EL, 0x3A59FF45L, 0x3E350A44L, 0xBCB4CDD5L,
        0x72EACEA8L, 0xFA6484BBL, 0x8D6612AEL, 0xBF3C6F47L,
        0xD29BE463L, 0x542F5D9EL, 0xAEC2771BL, 0xF64E6370L,
        0x740E0D8DL, 0xE75B1357L, 0xF8721671L, 0xAF537D5DL,
        0x4040CB08L, 0x4EB4E2CCL, 0x34D2466AL, 0x0115AF84L,
        0xE1B00428L, 0x95983A1DL, 0x06B89FB4L, 0xCE6EA048L,
        0x6F3F3B82L, 0x3520AB82L, 0x011A1D4BL, 0x277227F8L,
        0x611560B1L, 0xE7933FDCL, 0xBB3A792BL, 0x344525BDL,
        0xA08839E1L, 0x51CE794BL, 0x2F32C9B7L, 0xA01FBAC9L,
        0xE01CC87EL, 0xBCC7D1F6L, 0xCF0111C3L, 0xA1E8AAC7L,
        0x1A908749L, 0xD44FBD9AL, 0xD0DADECBL, 0xD50ADA38L,
        0x0339C32AL, 0xC6913667L, 0x8DF9317CL, 0xE0B12B4FL,
        0xF79E59B7L, 0x43F5BB3AL, 0xF2D519FFL, 0x27D9459CL,
        0xBF97222CL, 0x15E6FC2AL, 0x0F91FC71L, 0x9B941525L,
        0xFAE59361L, 0xCEB69CEBL, 0xC2A86459L, 0x12BAA8D1L,
        0xB6C1075EL, 0xE3056A0CL, 0x10D25065L, 0xCB03A442L,
        0xE0EC6E0EL, 0x1698DB3BL, 0x4C98A0BEL, 0x3278E964L,
        0x9F1F9532L, 0xE0D392DFL, 0xD3A0342BL, 0x8971F21EL,
        0x1B0A7441L, 0x4BA3348CL, 0xC5BE7120L, 0xC37632D8L,
        0xDF359F8DL, 0x9B992F2EL, 0xE60B6F47L, 0x0FE3F11DL,
        0xE54CDA54L, 0x1EDAD891L, 0xCE6279CFL, 0xCD3E7E6FL,
        0x1618B166L, 0xFD2C1D05L, 0x848FD2C5L, 0xF6FB2299L,
        0xF523F357L, 0xA6327623L, 0x93A83531L, 0x56CCCD02L,
        0xACF08162L, 0x5A75EBB5L, 0x6E163697L, 0x88D273CCL,
        0xDE966292L, 0x81B949D0L, 0x4C50901BL, 0x71C65614L,
        0xE6C6C7BDL, 0x327A140AL, 0x45E1D006L, 0xC3F27B9AL,
        0xC9AA53FDL, 0x62A80F00L, 0xBB25BFE2L, 0x35BDD2F6L,
        0x71126905L, 0xB2040222L, 0xB6CBCF7CL, 0xCD769C2BL,
        0x53113EC0L, 0x1640E3D3L, 0x38ABBD60L, 0x2547ADF0L,
        0xBA38209CL, 0xF746CE76L, 0x77AFA1C5L, 0x20756060L,
        0x85CBFE4EL, 0x8AE88DD8L, 0x7AAAF9B0L, 0x4CF9AA7EL,
        0x1948C25CL, 0x02FB8A8CL, 0x01C36AE4L, 0xD6EBE1F9L,
        0x90D4F869L, 0xA65CDEA0L, 0x3F09252DL, 0xC208E69FL,
        0xB74E6132L, 0xCE77E25BL, 0x578FDFE3L, 0x3AC372E6L
    }
};


static ME_NO_SANITIZE_WRAP uint BF(WebsBlowfish *bp, uint x)
{
    ushort a, b, c, d;
    uint   y;

    d = x & 0x00FF;
    x >>= 8;
    c = x & 0x00FF;
    x >>= 8;
    b = x & 0x00FF;
    x >>= 8;
    a = x & 0x00FF;

    y = bp->S[0][a] + bp->S[1][b];
    y = y ^ bp->S[2][c];
    y = y + bp->S[3][d];
    return y;
}


static void bencrypt(WebsBlowfish *bp, uint *xl, uint *xr)
{
    uint Xl, Xr, temp;
    int  i;

    Xl = *xl;
    Xr = *xr;

    for (i = 0; i < BF_ROUNDS; ++i) {
        Xl = Xl ^ bp->P[i];
        Xr = BF(bp, Xl) ^ Xr;
        temp = Xl;
        Xl = Xr;
        Xr = temp;
    }
    temp = Xl;
    Xl = Xr;
    Xr = temp;
    Xr = Xr ^ bp->P[BF_ROUNDS];
    Xl = Xl ^ bp->P[BF_ROUNDS + 1];
    *xl = Xl;
    *xr = Xr;
}


static void binit(WebsBlowfish *bp, uchar *key, ssize keylen)
{
    uint data, datal, datar;
    int  i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 256; j++) {
            bp->S[i][j] = ORIG_S[i][j];
        }
    }
    for (j = i = 0; i < BF_ROUNDS + 2; ++i) {
        for (data = 0, k = 0; k < 4; ++k) {
            data = (data << 8) | key[j];
            j = j + 1;
            if (j >= keylen) {
                j = 0;
            }
        }
        bp->P[i] = ORIG_P[i] ^ data;
    }
    datal = datar = 0;

    for (i = 0; i < BF_ROUNDS + 2; i += 2) {
        bencrypt(bp, &datal, &datar);
        bp->P[i] = datal;
        bp->P[i + 1] = datar;
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 256; j += 2) {
            bencrypt(bp, &datal, &datar);
            bp->S[i][j] = datal;
            bp->S[i][j + 1] = datar;
        }
    }
}


/*
    Text: "OrpheanBeholderScryDoubt"
 */
static uint cipherText[6] = {
    0x4f727068, 0x65616e42, 0x65686f6c,
    0x64657253, 0x63727944, 0x6f756274
};


PUBLIC int websGetRandomBytes(char *buf, ssize length, bool block)
{
#if ME_UNIX_LIKE
    ssize sofar, rc;
    int   fd;

    if ((fd = open((block) ? "/dev/random" : "/dev/urandom", O_RDONLY, 0666)) < 0) {
        return -1;
    }
    sofar = 0;
    do {
        rc = read(fd, &buf[sofar], length);
        if (rc < 0) {
            assert(0);
            close(fd);
            return -1;
        }
        length -= rc;
        sofar += rc;
    } while (length > 0);
    close(fd);
    return 0;
#elif ME_WIN_LIKE
    HCRYPTPROV prov;
    int        rc;

    rc = 0;
    if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | 0x40)) {
        return -1;
    }
    if (!CryptGenRandom(prov, (wsize) length, (BYTE*) buf)) {
        rc = -1;
    }
    CryptReleaseContext(prov, 0);
    return rc;
#else
/*
    for (int i = 0; i < length; i++) {
        buf[i] = (char) (rand() & 0xff);
    }
 */
    #error \
    "websGetRandomBytes must be implemented for your platform using a cryptographically secure random number generator."
#endif
}


/*
    Run the Blowfish schedule over "salt:key" and return the encoded cipher text. The caller decides
    what "key" is: BF1 passes the password through unchanged, BF2 passes its SHA-1 digest.
 */
static char *cryptBlock(cchar *key, cchar *salt, int rounds)
{
    WebsBlowfish *bf;
    char         *result, *composite;
    uint         *text;
    ssize        len, limit;
    int          i, j;

    composite = sfmt("%s:%s", salt, key);
    bf = walloc(sizeof(WebsBlowfish));
    binit(bf, (uchar*) composite, slen(composite));
    len = sizeof(cipherText);
    text = wdup(cipherText, len);

    for (i = 0; i < rounds; i++) {
        limit = len / sizeof(uint);
        for (j = 0; j < limit; j += 2) {
            bencrypt(bf, &text[j], &text[j + 1]);
        }
    }
    result = websEncode64Block((char*) text, len);
    memset(bf, 0, sizeof(WebsBlowfish));
    wfree(bf);
    memset(text, 0, len);
    wfree(text);
    wfree(composite);
    return result;
}


/*
    BF1. Retained with its original semantics so hashes already in the field keep verifying. New
    hashes are not written in this format - see websMakePassword.
 */
PUBLIC char *websCryptPassword(cchar *password, cchar *salt, int rounds)
{
    /*
        Defense-in-depth: callers are expected to enforce the plaintext-password limit
        (WEBS_MAX_PASSWORD). This bound accommodates "username:realm:password" inputs
        produced by auth.c and gopass without penalising reasonable username/realm lengths.
     */
    if (slen(password) > WEBS_MAX_PASSWORD_BUFFER) {
        return 0;
    }
    return cryptBlock(password, salt, rounds);
}


/*
    BF2. The SHA-1 digest is a fixed SHA_SIZE * 2 characters, so with the default salt the key is 57
    bytes and the schedule window cannot be reached however long the caller's input is.
 */
static char *cryptPassword2(cchar *password, cchar *salt, int rounds)
{
    char *digest, *result;

    if (slen(password) > WEBS_MAX_PASSWORD_BUFFER) {
        return 0;
    }
    if ((digest = shaHex(password, slen(password))) == 0) {
        return 0;
    }
    result = cryptBlock(digest, salt, rounds);
    wfree(digest);
    return result;
}


PUBLIC char *websMakeSalt(ssize size)
{
    char  *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char  *rp, *result, *random;
    ssize clen, i;

    size = (size + sizeof(int) - 1) & ~(sizeof(int) - 1);
    random = walloc(size + 1);
    result = walloc(size + 1);
    if (websGetRandomBytes(random, size, 0) < 0) {
        wfree(random);
        wfree(result);
        return 0;
    }
    clen = slen(chars);
    for (i = 0, rp = result; i < size; i++) {
        *rp++ = chars[(random[i] & 0x7F) % clen];
    }
    *rp = '\0';
    wfree(random);
    return result;
}


/*
    Format of hashed password is:

    Algorithm: Rounds: Salt: Hash
 */
PUBLIC char *websMakePassword(cchar *password, int saltLength, int rounds)
{
    char *salt, *hash, *result;

    if (slen(password) > WEBS_MAX_PASSWORD_BUFFER) {
        return 0;
    }
    if (saltLength <= 0) {
        saltLength = BLOWFISH_SALT_LENGTH;
    }
    if (rounds <= 0) {
        rounds = BLOWFISH_ROUNDS;
    }
    /*
        A null salt would be formatted into the hash as the literal "null", giving every password on
        the device the same salt. Fail rather than issue one.
     */
    if ((salt = websMakeSalt(saltLength)) == 0) {
        return 0;
    }
    /*
        Refuse a salt long enough to push the fixed-width digest out of the schedule window. The
        default salt leaves ample room; a caller-supplied one must not reintroduce truncation.
     */
    if ((slen(salt) + 1 + (SHA_SIZE * 2)) >= WEBS_BLOWFISH_KEY_WINDOW) {
        wfree(salt);
        return 0;
    }
    if ((hash = cryptPassword2(password, salt, rounds)) == 0) {
        wfree(salt);
        return 0;
    }
    result = sfmt("%s:%05d:%s:%s", WEBS_CRYPT_BLOWFISH2, rounds, salt, hash);
    wfree(hash);
    wfree(salt);
    return result;
}


PUBLIC bool websCheckPassword(cchar *plainTextPassword, cchar *passwordHash)
{
    char  *algorithm, *given, *rounds, *salt, *s1, *s2, *tok, *hash, *ph;
    ssize match;

    if (!passwordHash || !plainTextPassword) {
        return 0;
    }
    if (slen(plainTextPassword) > WEBS_MAX_PASSWORD_BUFFER) {
        return 0;
    }
    ph = sclone(passwordHash);
    algorithm = stok(ph, ":", &tok);
    rounds = stok(NULL, ":", &tok);
    salt = stok(NULL, ":", &tok);
    hash = stok(NULL, ":", &tok);
    if (!algorithm || !rounds || !salt || !hash) {
        wfree(ph);
        return 0;
    }
    /*
        Verify in the format the stored hash was written in. A BF1 hash must keep verifying the BF1
        way or every credential already deployed stops working - it cannot be upgraded in place,
        because the stored value never depended on the password it is supposed to check.
     */
    if (smatch(algorithm, WEBS_CRYPT_BLOWFISH2)) {
        given = cryptPassword2(plainTextPassword, salt, atoi(rounds));

    } else if (smatch(algorithm, WEBS_CRYPT_BLOWFISH)) {
        /*
            BF1, fail closed at or past the key window. A key that reaches the window was truncated
            when the hash was stored, so the stored value no longer depends on the password and every
            password matches. The test is >= because a candidate filling the window exactly matches a
            longer stored key truncated to the same bytes. A residual below the window is not closed
            here: this routine sees one composite string and cannot tell a long realm from a long
            password, so websAddUser reports that band instead.
         */
        if ((slen(salt) + 1 + slen(plainTextPassword)) >= WEBS_BLOWFISH_KEY_WINDOW) {
            wfree(ph);
            return 0;
        }
        given = websCryptPassword(plainTextPassword, salt, atoi(rounds));

    } else {
        wfree(ph);
        return 0;
    }
    if (given == 0) {
        wfree(ph);
        return 0;
    }
    match = slen(given) ^ slen(hash);
    for (s1 = given, s2 = hash; *s1 && *s2; s1++, s2++) {
        match |= (*s1 & 0xFF) ^ (*s2 & 0xFF);
    }
    wfree(given);
    wfree(ph);
    return !match;
}


PUBLIC char *websReadPassword(cchar *prompt)
{
    char *cp, *password, *result;

#if ME_BSD_LIKE
    char passbuf[WEBS_MAX_PASSWORD];

    if (!prompt || !*prompt) {
        prompt = "Password: ";
    }
    if ((password = readpassphrase(prompt, passbuf, sizeof(passbuf), 0)) == 0) {
        return 0;
    }
#elif ME_UNIX_LIKE
    if (!prompt || !*prompt) {
        prompt = "Password: ";
    }
    if ((password = getpass(prompt)) == 0) {
        return 0;
    }
#elif ME_WIN_LIKE || VXWORKS
    char passbuf[WEBS_MAX_PASSWORD];
    int  c, i;

    if (!prompt || !*prompt) {
        prompt = "Password: ";
    }
    fputs(prompt, stdout);
    for (i = 0; i < (int) sizeof(passbuf) - 1; i++) {
#if VXWORKS
        c = getchar();
#else
        c = _getch();
#endif
        if (c == '\r' || c == EOF) {
            break;
        }
        if ((c == '\b' || c == 127) && i > 0) {
            passbuf[--i] = '\0';
            fputs("\b \b", stdout);
            i--;
        } else if (c == 26) {           /* Control Z */
            c = EOF;
            break;
        } else if (c == 3) {            /* Control C */
            fputs("^C\n", stdout);
            exit(255);
        } else if (!iscntrl((uchar) c) && (i < (int) sizeof(passbuf) - 1)) {
            passbuf[i] = (char) c;
            fputc('*', stdout);
        } else {
            fputc('', stdout);
            i--;
        }
    }
    if (c == EOF) {
        return "";
    }
    fputc('\n', stdout);
    passbuf[i] = '\0';
    password = passbuf;
#else
    return 0;
#endif
    result = sclone(password);
    for (cp = password; *cp; cp++) {
        *cp = 0;
    }
    return result;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/file.c ************/

/*
    file.c -- File handler

    This module serves static file documents

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char *websIndex;                     /* Default page name */
static char *websDocuments;                 /* Default Web page directory */

/**************************** Forward Declarations ****************************/

#if !ME_ROM
static int commitPut(Webs *wp);
#if !ME_WIN_LIKE
static int copyFile(cchar *from, cchar *to);
#endif
#endif
static void fileWriteEvent(Webs *wp);
static bool isTemplate(cchar *ext);
static char *makeEtag(WebsFileInfo *info);
static bool matchEtag(cchar *list, cchar *etag);
static bool notModified(Webs *wp, WebsFileInfo *info, cchar *etag);

/*********************************** Code *************************************/
/*
    Serve static files
    Return true to indicate the request was handled, even for errors.
 */
static bool fileHandler(Webs *wp)
{
    WebsFileInfo info;
    char         *tmp, *date, *path, *encoded, *etag;
    ssize        nchars;
    int          code;

#if !ME_ROM
    bool existed;
#endif

    assert(websValid(wp));
    assert(wp->method);
    assert(wp->filename && wp->filename[0]);

#if !ME_ROM
    if (smatch(wp->method, "DELETE")) {
        if (!websPageAllowed(wp) || unlink(wp->filename) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot delete the URI");
        } else {
            /* No content */
            websResponse(wp, 204, 0);
        }
    } else if (smatch(wp->method, "PUT")) {
        //  A staged body that is not committed here is removed when the request ends
        if (!websPageAllowed(wp)) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot put the URI");
            return 1;
        }
        existed = websPageStat(wp, &info) >= 0;
        if (commitPut(wp) < 0) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot save the put URI");
            return 1;
        }
        websResponse(wp, existed ? HTTP_CODE_NO_CONTENT : HTTP_CODE_CREATED, 0);

    } else
#endif /* !ME_ROM */
    {
        if (isTemplate(wp->ext)) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot serve template source for: %s", wp->path);
            return 1;
        }
        /*
            If the file is a directory, redirect using the nominated default page
         */
        if (websPageIsDirectory(wp)) {
            /*
                Copy the authorized path before trimming, then URI-encode it for Location.
             */
            path = sclone(wp->path);
            nchars = slen(path);
            if (nchars > 0 && (path[nchars - 1] == '/' || path[nchars - 1] == '\\')) {
                path[--nchars] = '\0';
            }
            encoded = websEscapeUri(path);
            tmp = sfmt("%s/%s", encoded, websIndex);
            websRedirect(wp, tmp);
            wfree(tmp);
            wfree(encoded);
            wfree(path);
            return 1;
        }
        if (websPageOpen(wp, O_RDONLY | O_BINARY, 0666) < 0) {
#if ME_DEBUG
            if (wp->referrer) {
                trace(1, "From %s", wp->referrer);
            }
#endif
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot open document for: %s", wp->path);
            return 1;
        }
        if (websPageStat(wp, &info) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot stat page for URL");
            return 1;
        }
        code = 200;
        etag = makeEtag(&info);
        if (notModified(wp, &info, etag)) {
            code = 304;
            info.size = 0;
        }
        websSetStatus(wp, code);
        wp->flags |= WEBS_CACHEABLE;
        websWriteHeaders(wp, info.size, 0);
        if (info.mtime && (date = websGetDateString(&info)) != NULL) {
            websWriteHeader(wp, "Last-Modified", "%s", date);
            wfree(date);
        }
        websWriteHeader(wp, "ETag", "%s", etag);
        wfree(etag);
        websWriteEndHeaders(wp);

        /*
            All done if the browser did a HEAD request
         */
        if (smatch(wp->method, "HEAD")) {
            websDone(wp);
            return 1;
        }
        if (info.size > 0) {
            wp->txRemaining = info.size;
            websSetBackgroundWriter(wp, fileWriteEvent);
        } else {
            websDone(wp);
        }
    }
    return 1;
}


/*
    Template pages are run by the template handler, never served as source, whatever the route table or build
 */
static bool isTemplate(cchar *ext)
{
    return smatch(ext, ".jst") || smatch(ext, ".asp");
}


/*
    Use weak timestamp tags for files; hash ROM content so equal size and time cannot hide changes.
 */
static char *makeEtag(WebsFileInfo *info)
{
    char   version[24], size[24];
    uint64 hash;
    int    i;

    if (info->hash) {
        for (hash = info->hash, i = 15; i >= 0; i--, hash >>= 4) {
            version[i] = "0123456789abcdef"[hash & 0xF];
        }
        version[16] = '\0';
    } else {
        itosbuf(version, sizeof(version), (int64) info->mtime, 16);
    }
    itosbuf(size, sizeof(size), (int64) info->size, 16);
    return sfmt("W/\"%s-%s\"", version, size);
}


/*
    Weakly compare listed opaque tags; * matches any representation.
 */
static bool matchEtag(cchar *list, cchar *etag)
{
    char *buf, *tag, *next;
    bool match;

    if (sstarts(etag, "W/")) {
        etag += 2;
    }
    buf = sclone(list);
    match = 0;
    for (tag = stok(buf, ",", &next); tag && !match; tag = stok(NULL, ",", &next)) {
        tag = strim(tag, " \t", WEBS_TRIM_BOTH);
        if (sstarts(tag, "W/")) {
            tag += 2;
        }
        match = smatch(tag, "*") || smatch(tag, etag);
    }
    wfree(buf);
    return match;
}


/*
    If-None-Match takes precedence over If-Modified-Since (RFC 9110 13.2.2).
 */
static bool notModified(Webs *wp, WebsFileInfo *info, cchar *etag)
{
    if (!smatch(wp->method, "GET") && !smatch(wp->method, "HEAD")) {
        return 0;
    }
    if (wp->ifNoneMatch) {
        return matchEtag(wp->ifNoneMatch, etag);
    }
    return wp->since && info->mtime && info->mtime <= wp->since;
}


/*
    Do output back to the browser in the background. This is a socket write handler.
    This bypasses the output buffer and writes directly to the socket.
 */
static void fileWriteEvent(Webs *wp)
{
    char  *buf;
    ssize size, len, wrote;
    int   err;

    assert(wp);
    assert(websValid(wp));

    if ((buf = walloc(ME_GOAHEAD_LIMIT_BUFFER)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get memory");
        return;
    }
    while (wp->txRemaining > 0) {
        size = (ssize) min(wp->txRemaining, ME_GOAHEAD_LIMIT_BUFFER);
        len = websPageReadData(wp, buf, size);
        if (len <= 0) {
            /*
                Close a truncated response; appending an error page would corrupt its declared body.
             */
            error("Cannot read %s after the response was committed, closing the connection", wp->filename);
            wfree(buf);
            wp->flags &= ~WEBS_KEEP_ALIVE;
            wp->state = WEBS_COMPLETE;
            websDone(wp);
            return;
        }
        if ((wrote = websWriteSocket(wp, buf, len)) < 0) {
            err = socketGetError(wp->sid);
            if (err == EWOULDBLOCK || err == EAGAIN) {
                websPageSeek(wp, -len, SEEK_CUR);
            } else {
                /* Will call websDone below */
                wp->state = WEBS_COMPLETE;
            }
            break;
        }
        wp->txRemaining -= wrote;
        if (wrote != len) {
            websPageSeek(wp, -(len - wrote), SEEK_CUR);
            break;
        }
    }
    wfree(buf);
    if (wp->txRemaining <= 0) {
        websDone(wp);
    }
}


#if !ME_ROM
#if !ME_WIN_LIKE
/*
    Copy privately across filesystems and remove partial copies.
 */
static int copyFile(cchar *from, cchar *to)
{
    char  buf[ME_GOAHEAD_LIMIT_BUFFER];
    ssize nbytes;
    int   in, out, rc;

    if ((in = open(from, O_RDONLY | O_BINARY)) < 0) {
        return -1;
    }
    if ((out = open(to, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_NOFOLLOW, 0600)) < 0) {
        close(in);
        return -1;
    }
    rc = 0;
    while ((nbytes = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, (size_t) nbytes) != nbytes) {
            rc = -1;
            break;
        }
    }
    if (nbytes < 0) {
        rc = -1;
    }
    close(in);
    if (close(out) < 0) {
        rc = -1;
    }
    if (rc < 0) {
        unlink(to);
    }
    return rc;
}
#endif


/*
    Commit the staged PUT before reporting success.
 */
static int commitPut(Webs *wp)
{
    if (wp->putfd < 0 || !wp->putname || !wp->filename) {
        error("Cannot complete PUT: no staged file or target");
        return -1;
    }
    close(wp->putfd);
    wp->putfd = -1;
#if ME_WIN_LIKE
    //  rename cannot replace an existing file on Windows, and cannot move one between volumes
    if (!MoveFileExA(wp->putname, wp->filename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        error("Cannot move PUT file from %s to %s", wp->putname, wp->filename);
        return -1;
    }
#else
    if (rename(wp->putname, wp->filename) < 0) {
        if (errno != EXDEV || copyFile(wp->putname, wp->filename) < 0) {
            error("Cannot rename PUT file from %s to %s, errno %d", wp->putname, wp->filename, errno);
            return -1;
        }
        unlink(wp->putname);
    }
#endif
    wfreeset(wp->putname);
    return 0;
}


PUBLIC bool websProcessPutData(Webs *wp)
{
    ssize nbytes;

    assert(wp);
    assert(wp->putfd >= 0);
    assert(wp->input.buf);

    nbytes = bufLen(&wp->input);
    //  Tested against the room left, as the running total plus a buffer can overflow
    if (nbytes > ME_GOAHEAD_LIMIT_PUT - wp->putLen) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Put file too large");

    } else if (write(wp->putfd, bufStart(&wp->input), (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot write to file");

    } else {
        wp->putLen += nbytes;
    }
    websConsumeInput(wp, nbytes);
    return 1;
}
#endif


static void fileClose(void)
{
    wfree(websIndex);
    websIndex = NULL;
    wfree(websDocuments);
    websDocuments = NULL;
}


PUBLIC void websFileOpen(void)
{
    websIndex = sclone("index.html");
    websDefineHandler("file", 0, fileHandler, fileClose, 0);
}


/*
    Get the default page for URL requests ending in "/"
 */
PUBLIC cchar *websGetIndex(void)
{
    return websIndex;
}


PUBLIC char *websGetDocuments(void)
{
    return websDocuments;
}


/*
    Set the default page for URL requests ending in "/"
 */
PUBLIC void websSetIndex(cchar *page)
{
    assert(page && *page);

    if (websIndex) {
        wfree(websIndex);
    }
    websIndex = sclone(page);
}


/*
    Set the default web directory
 */
PUBLIC void websSetDocuments(cchar *dir)
{
    /*
        Reject an empty root, which would expose the filesystem root.
     */
    if (dir == 0 || *dir == '\0') {
        error("Documents directory must not be empty");
        return;
    }
    if (websDocuments) {
        wfree(websDocuments);
    }
    websDocuments = sclone(dir);
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/fs.c ************/

/*
    fs.c -- File System support and support for ROM-based file systems.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/******************************** Local Data **********************************/

#if ME_ROM
/*
    Symbol table for web pages and files
 */
static WebsHash romFs;

/*
    Each ROM open owns its read position.
 */
typedef struct RomFile {
    WebsRomIndex *wip;
    Offset pos;
} RomFile;

static RomFile **romFiles;
static int     romFileMax;

static WebsRomIndex *lookup(WebsHash fs, cchar *path);
static RomFile *getRomFile(int fd);
#endif

#if !ME_ROM && ME_WIN_LIKE
/* Refuse Windows device names before opening: opening a console or serial device can block the event loop. */
static cchar *reservedDevices[] = {
    "con", "prn", "aux", "nul", "conin$", "conout$",
    "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
    "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
    //  Superscript one to three, as UTF-8 and as the single byte of the Windows-1252 code page
    "com\xc2\xb9", "com\xc2\xb2", "com\xc2\xb3", "lpt\xc2\xb9", "lpt\xc2\xb2", "lpt\xc2\xb3",
    "com\xb9", "com\xb2", "com\xb3", "lpt\xb9", "lpt\xb2", "lpt\xb3",
    0
};
#endif

/*********************************** Code *************************************/

/*
    Match DOS device stems before dots or colons, ignoring trailing spaces and dots.
 */
PUBLIC bool websIsReservedDevice(cchar *path)
{
#if !ME_ROM && ME_WIN_LIKE
    cchar *start, *cp;
    char  stem[ME_GOAHEAD_LIMIT_FILENAME];
    ssize len;
    int   i;

    for (start = path; start && *start; start = cp) {
        for (cp = start; *cp && *cp != '/' && *cp != '\\'; cp++) { }
        for (len = 0; &start[len] < cp && start[len] != '.' && start[len] != ':'; len++) { }
        while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '.')) {
            len--;
        }
        if (0 < len && len < (ssize) sizeof(stem)) {
            memcpy(stem, start, len);
            stem[len] = '\0';
            for (i = 0; reservedDevices[i]; i++) {
                if (scaselessmatch(stem, reservedDevices[i])) {
                    return 1;
                }
            }
        }
        if (*cp) {
            cp++;
        }
    }
#else
    (void) path;
#endif
    return 0;
}


PUBLIC int websFsOpen(void)
{
#if ME_ROM
    WebsRomIndex *wip;
    char         name[ME_GOAHEAD_LIMIT_FILENAME];
    ssize        len;

    romFs = hashCreate(WEBS_HASH_INIT);
    for (wip = websRomIndex; wip->path; wip++) {
        strncpy(name, wip->path, ME_GOAHEAD_LIMIT_FILENAME - 1);
        name[ME_GOAHEAD_LIMIT_FILENAME - 1] = '\0';
        len = strlen(name) - 1;
        if (len > 0 && (name[len] == '/' || name[len] == '\\')) {
            name[len] = '\0';
        }
        hashEnter(romFs, name, valueSymbol(wip), 0);
    }
#endif
    return 0;
}


PUBLIC void websFsClose(void)
{
#if ME_ROM
    hashFree(romFs);
#endif
}


PUBLIC int websOpenFile(cchar *path, int flags, int mode)
{
#if ME_ROM
    WebsRomIndex *wip;
    int          fd;

    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
    if ((fd = wallocObject(&romFiles, &romFileMax, sizeof(RomFile))) < 0) {
        return -1;
    }
    romFiles[fd]->wip = wip;
    return fd;
#else
    WebsStat sbuf;
    int      fd;

#if !ME_WIN_LIKE
    /*
        O_NONBLOCK makes open() on a FIFO or device return immediately instead of blocking in the
        kernel. GoAhead has one event loop, so a blocking open stalls every connection and the timeout
        machinery with it. The flag must be on the open itself, as a prior stat() tests a different
        object than the one open() resolves.
     */
    flags |= O_NONBLOCK;
#endif
    if ((fd = open(path, flags, mode)) < 0) {
        return -1;
    }
    /*
        Only regular files are servable. Test the descriptor rather than the path so the type cannot
        change between the check and the use.
     */
    if (fstat(fd, &sbuf) < 0 || !S_ISREG(sbuf.st_mode)) {
        close(fd);
        errno = ENOENT;
        return -1;
    }
#if !ME_WIN_LIKE
    /*
        Restore blocking semantics now that the type is known. Reads and writes on a regular file
        cannot block indefinitely and the transfer paths expect blocking behavior.
     */
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
#endif
    return fd;
#endif
}


PUBLIC void websCloseFile(int fd)
{
#if ME_ROM
    RomFile *rf;

    if ((rf = getRomFile(fd)) != NULL) {
        wfree(rf);
        romFileMax = wfreeHandle(&romFiles, fd);
    }
#else
    if (fd >= 0) {
        close(fd);
    }
#endif
}


PUBLIC int websStatFile(cchar *path, WebsFileInfo *sbuf)
{
#if ME_ROM
    WebsRomIndex *wip;
#else
    WebsStat s;
    int      rc;
#endif

    if (path == 0 || *path == '\0') {
        return -1;
    }
#if ME_ROM
    if ((wip = lookup(romFs, path)) == NULL) {
        return -1;
    }
    memset(sbuf, 0, sizeof(WebsFileInfo));
    sbuf->size = wip->size;
    sbuf->mtime = wip->mtime;
    sbuf->hash = wip->hash;
    if (wip->page == NULL) {
        sbuf->isDir = 1;
    }
    return 0;
#else
#if ME_WIN_LIKE
    {
        /*
            Strip trailing separators only for directories so Windows stat agrees with open.
         */
        ssize len = slen(path) - 1;
        char  *p = sclone(path);
        bool  trailing = p[len] == '/' || p[len] == '\\';

        if (trailing) {
            p[len] = '\0';
        }
        rc = stat(p, &s);
        wfree(p);
        if (rc == 0 && trailing && !S_ISDIR(s.st_mode)) {
            errno = ENOENT;
            return -1;
        }
    }
#else
    rc = stat(path, &s);
#endif
    if (rc < 0) {
        return -1;
    }
    sbuf->size = (Offset) s.st_size;
    sbuf->mtime = s.st_mtime;
    sbuf->isDir = S_ISDIR(s.st_mode);
    sbuf->hash = 0;
    return 0;
#endif
}


/*
    Read metadata from the same open file that supplies the response bytes.
 */
PUBLIC int websStatOpenFile(int fd, WebsFileInfo *sbuf)
{
#if ME_ROM
    RomFile      *rf;
    WebsRomIndex *wip;

    assert(fd >= 0);
    if ((rf = getRomFile(fd)) == NULL || rf->wip->page == NULL) {
        return -1;
    }
    wip = rf->wip;
    memset(sbuf, 0, sizeof(WebsFileInfo));
    sbuf->size = wip->size;
    sbuf->mtime = wip->mtime;
    sbuf->hash = wip->hash;
    return 0;
#else
    WebsStat s;

    assert(fd >= 0);
    if (fd < 0 || fstat(fd, &s) < 0) {
        return -1;
    }
    sbuf->size = (Offset) s.st_size;
    sbuf->mtime = s.st_mtime;
    sbuf->isDir = S_ISDIR(s.st_mode);
    sbuf->hash = 0;
    return 0;
#endif
}


PUBLIC ssize websReadFile(int fd, char *buf, ssize size)
{
#if ME_ROM
    RomFile *rf;
    ssize   len;

    assert(buf);
    assert(fd >= 0);

    if ((rf = getRomFile(fd)) == NULL) {
        errno = EBADF;
        return -1;
    }
    if (rf->pos >= rf->wip->size) {
        return 0;
    }
    len = (ssize) min(rf->wip->size - rf->pos, size);
    memcpy(buf, &rf->wip->page[rf->pos], len);
    rf->pos += len;
    return len;
#else
    return read(fd, buf, (uint) size);
#endif
}


PUBLIC char *websReadWholeFile(cchar *path)
{
    WebsFileInfo sbuf;
    char         *buf;
    ssize        nbytes;
    int          fd;

    if (websStatFile(path, &sbuf) < 0 || sbuf.size >= MAXSSIZE) {
        return 0;
    }
    buf = walloc((ssize) sbuf.size + 1);
    /*
        O_BINARY prevents Windows text-mode CRLF->LF translation, which shrinks the read below sbuf.size
        and would otherwise leave uninitialized heap between the data and the NUL terminator. The flag is
        a no-op on Unix (defined to 0 in osdep.h).
     */
    if ((fd = websOpenFile(path, O_RDONLY | O_BINARY, 0)) < 0) {
        wfree(buf);
        return 0;
    }
    nbytes = websReadFile(fd, buf, (ssize) sbuf.size);
    buf[nbytes > 0 ? nbytes : 0] = '\0';
    websCloseFile(fd);
    return buf;
}


Offset websSeekFile(int fd, Offset offset, int origin)
{
#if ME_ROM
    RomFile *rf;
    Offset  pos;

    assert(origin == SEEK_SET || origin == SEEK_CUR || origin == SEEK_END);
    assert(fd >= 0);

    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    if ((rf = getRomFile(fd)) == NULL) {
        errno = EBADF;
        return -1;
    }
    pos = offset;
    switch (origin) {
    case SEEK_CUR:
        pos = rf->pos + offset;
        break;
    case SEEK_END:
        pos = rf->wip->size + offset;
        break;
    default:
        break;
    }
    if (pos < 0) {
        errno = EBADF;
        return -1;
    }
    return rf->pos = pos;
#else
    return lseek(fd, (long) offset, origin);
#endif
}


PUBLIC ssize websWriteFile(int fd, cchar *buf, ssize size)
{
#if ME_ROM
    error("Cannot write to a rom file system");
    return -1;
#else
    return write(fd, buf, (uint) size);
#endif
}


#if ME_ROM
static RomFile *getRomFile(int fd)
{
    if (fd < 0 || fd >= romFileMax) {
        return NULL;
    }
    return romFiles[fd];
}


static WebsRomIndex *lookup(WebsHash fs, cchar *path)
{
    WebsKey *sp;
    char    *name;
    ssize   len;

    if ((sp = hashLookup(fs, path)) == NULL) {
        if (path[0] != '/') {
            name = sfmt("/%s", path);
        } else {
            name = sclone(path);
        }
        len = slen(name) - 1;
        if (name[len] == '/') {
            name[len] = '\0';
        }
        sp = hashLookup(fs, name);
        wfree(name);
        if (sp == NULL) {
            return 0;
        }
    }
    return (WebsRomIndex*) sp->content.value.symbol;
}
#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/http.c ************/

/*
    http.c -- GoAhead HTTP engine

    This module implements an embedded HTTP/1.1 web server. It supports
    loadable URL handlers that define the nature of URL processing performed.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************* Defines ************************************/

#define WEBS_TIMEOUT           (ME_GOAHEAD_LIMIT_TIMEOUT * 1000)
#define PARSE_TIMEOUT          (ME_GOAHEAD_LIMIT_PARSE_TIMEOUT * 1000)
#define REQUEST_TIMEOUT        (ME_GOAHEAD_LIMIT_REQUEST_TIMEOUT * 1000)
#define FLUSH_SLICE            1000                /* Max msec to block in one send before retesting the deadline */
#define CHUNK_LOW              128                 /* Low water mark for chunking */
#define MAX_CHUNK_HEADER       80                  /* Max bytes in a "\r\nSIZE[;ext]\r\n" chunk header */
#define MAX_PROLOGUE           8                   /* Max leading CR/LF bytes before a request line (4 blank lines) */

#define TOKEN_HEADER_KEY       0x1                 /* Validate token as a header key */
#define TOKEN_HEADER_VALUE     0x2                 /* Validate token as a header value */
#define TOKEN_URI_VALUE        0x4                 /* Validate token as a URI value */
#define TOKEN_NUMBER           0x8                 /* Validate token as a number */
#define TOKEN_WORD             0x10                /* Validate token as single word with no spaces */
#define TOKEN_LINE             0x20                /* Validate token as line with no newlines */
#define TOKEN_STRICT           0x40                /* Modifier: no leading white space before the token */

#define MAX_METHOD             24                  /* Max request method name length */

/*
    Single-valued request headers already seen. See singleValuedHeader.
 */
#define SEEN_CONTENT_LENGTH    0x1
#define SEEN_TRANSFER_ENCODING 0x2
#define SEEN_HOST              0x4
#define SEEN_AUTHORIZATION     0x8
#define SEEN_CONTENT_TYPE      0x10

/************************************ Locals **********************************/

static int      websBackground;                    /* Run as a daemon */
static int      websDebug;                         /* Run in debug mode and defeat timeouts */
static int      defaultHttpPort;                   /* Default port number for http */
static int      defaultSslPort;                    /* Default port number for https */
static int      listens[WEBS_MAX_LISTEN]; /* Listen endpoints */;
static int      listenMax;                         /* Max entry in listens */
static Webs     **webs;                            /* Open connection list head */
static WebsHash websMime;                          /* Set of mime types */
static WebsHash responseHeaders = -1;              /* Headers added to every response */
static int      websMax;                           /* List size */
static char     websHost[ME_MAX_IP];               /* Host name for the server */
static char     websIpAddr[ME_MAX_IP];             /* IP address for the server */
static char     *websHostUrl = NULL;               /* URL to access server */
static char     *trustedProxies = NULL;
static char     *websIpAddrUrl = NULL;             /* URL to access server */
static ssize    postLimit = ME_GOAHEAD_LIMIT_POST; /* Runtime-settable POST body cap */
#define FILE_RESERVE 64
static int      connLimit = ME_GOAHEAD_LIMIT_CONNECTIONS; /* Runtime-settable concurrent connection cap */
static int      connCount;                         /* Live accepted connections */
static int      connLimitPerIp = ME_GOAHEAD_LIMIT_CONNECTIONS_PER_IP; /* Runtime-settable per-peer cap */
static WebsHash connIps = -1;                      /* Live connection count keyed by peer address */

#define WEBS_ENCODE_HTML 0x1                       /* Bit setting in charMatch[] */
#define WEBS_ENCODE_URI  0x4                       /* Encode URI characters */

/*
    Character escape/descape matching codes. Generated by mprEncodeGenerate in appweb.
 */
static uchar charMatch[256] = {
    0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x7e, 0x3c, 0x3c, 0x7c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x7c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x00, 0x7f, 0x28, 0x2a, 0x3c, 0x2b, 0x43, 0x02, 0x02, 0x02, 0x28, 0x28, 0x00, 0x00, 0x28,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x2a, 0x3f, 0x28, 0x3f, 0x2a,
    0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a, 0x7e, 0x3a, 0x3e, 0x00,
    0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x3e, 0x3e, 0x02, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c
};

/*
    Add entries to the MimeList as required for your content
 */
static WebsMime websMimeList[] = {
    { "application/java", ".class" },
    { "application/java", ".jar" },
    { "text/html", ".asp" },
    { "text/html", ".htm" },
    { "text/html", ".jst" },
    { "text/html", ".html" },
    { "text/xml", ".xml" },
    { "image/gif", ".gif" },
    { "image/jpeg", ".jpg" },
    { "image/png", ".png" },
    { "image/svg+xml", ".svg" },
    { "image/vnd.microsoft.icon", ".ico" },
    { "text/css", ".css" },
    { "text/plain", ".txt" },
    { "application/x-javascript", ".js" },
    { "application/x-shockwave-flash", ".swf" },
    { "application/json", ".json" },

    { "application/binary", ".exe" },
    { "application/compress", ".z" },
    { "application/gzip", ".gz" },
    { "application/octet-stream", ".bin" },
    { "application/oda", ".oda" },
    { "application/pdf", ".pdf" },
    { "application/postscript", ".ai" },
    { "application/postscript", ".eps" },
    { "application/postscript", ".ps" },
    { "application/rtf", ".rtf" },
    { "application/x-bcpio", ".bcpio" },
    { "application/x-cpio", ".cpio" },
    { "application/x-csh", ".csh" },
    { "application/x-dvi", ".dvi" },
    { "application/x-gtar", ".gtar" },
    { "application/x-hdf", ".hdf" },
    { "application/x-latex", ".latex" },
    { "application/x-mif", ".mif" },
    { "application/x-netcdf", ".nc" },
    { "application/x-netcdf", ".cdf" },
    { "application/x-ns-proxy-autoconfig", ".pac" },
    { "application/x-patch", ".patch" },
    { "application/x-sh", ".sh" },
    { "application/x-shar", ".shar" },
    { "application/x-sv4cpio", ".sv4cpio" },
    { "application/x-sv4crc", ".sv4crc" },
    { "application/x-tar", ".tar" },
    { "application/x-tgz", ".tgz" },
    { "application/x-tcl", ".tcl" },
    { "application/x-tex", ".tex" },
    { "application/x-texinfo", ".texinfo" },
    { "application/x-texinfo", ".texi" },
    { "application/x-troff", ".t" },
    { "application/x-troff", ".tr" },
    { "application/x-troff", ".roff" },
    { "application/x-troff-man", ".man" },
    { "application/x-troff-me", ".me" },
    { "application/x-troff-ms", ".ms" },
    { "application/x-ustar", ".ustar" },
    { "application/x-wais-source", ".src" },
    { "application/zip", ".zip" },
    { "audio/basic", ".au snd" },
    { "audio/x-aiff", ".aif" },
    { "audio/x-aiff", ".aiff" },
    { "audio/x-aiff", ".aifc" },
    { "audio/x-wav", ".wav" },
    { "audio/x-wav", ".ram" },
    { "image/ief", ".ief" },
    { "image/jpeg", ".jpeg" },
    { "image/jpeg", ".jpe" },
    { "image/tiff", ".tiff" },
    { "image/tiff", ".tif" },
    { "image/x-cmu-raster", ".ras" },
    { "image/x-portable-anymap", ".pnm" },
    { "image/x-portable-bitmap", ".pbm" },
    { "image/x-portable-graymap", ".pgm" },
    { "image/x-portable-pixmap", ".ppm" },
    { "image/x-rgb", ".rgb" },
    { "image/x-xbitmap", ".xbm" },
    { "image/x-xpixmap", ".xpm" },
    { "image/x-xwindowdump", ".xwd" },
    { "text/html", ".cfm" },
    { "text/html", ".shtm" },
    { "text/html", ".shtml" },
    { "text/richtext", ".rtx" },
    { "text/tab-separated-values", ".tsv" },
    { "text/x-setext", ".etx" },
    { "video/mpeg", ".mpeg" },
    { "video/mpeg", ".mpg" },
    { "video/mpeg", ".mpe" },
    { "video/quicktime", ".qt" },
    { "video/quicktime", ".mov" },
    { "video/mp4", ".mp4" },
    { "video/x-msvideo", ".avi" },
    { "video/x-sgi-movie", ".movie" },
    { NULL, NULL },
};

/*
    Standard HTTP error codes
 */
static WebsError websErrors[] = {
    { 200, "OK" },
    { 201, "Created" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 301, "Redirect" },
    { 302, "Redirect" },
    { 304, "Not Modified" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Access Denied" },
    { 406, "Not Acceptable" },
    { 408, "Request Timeout" },
    { 409, "Conflict" },
    { 413, "Request too large" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 503, "Service Unavailable" },
    { 505, "HTTP Version Not Supported" },
    { 0, NULL }
};

#if ME_GOAHEAD_ACCESS_LOG && !ME_ROM
static WebsLogFile accessFile = { NULL, -1, 0 };    /* Access log file */
#define ACCESS_LOG "access.log"                     /* Access log filename */
#endif

static WebsHash sessions = -1;
static int      sessionCount = 0;
static int64    sessionUse;
static int      pruneId = -1;                       /* Callback ID */
static bool     routesAudited;
#if ME_UNIX_LIKE && defined(SIGPIPE)
static bool pipeIgnored;                            /* websOpen ignored SIGPIPE, so websClose restores it */
#endif

/**************************** Forward Declarations ****************************/

static void     abandonAccept(Webs *wp);
#if ME_UNIX_LIKE && defined(SIGPIPE)
static void     ignorePipeSignal(void);
static void     restorePipeSignal(void);
#endif
static void     addResponseHeaders(cchar *list);
static void     adjustIpCount(cchar *ip, int delta);
static void     checkTimeout(void *arg, int id);
static bool     concludeRequest(Webs *wp);
static bool     filterChunkData(Webs *wp);
static int64    getTimeSinceMark(Webs *wp);
static char *formatHttpDate(WebsTime when);
static time_t   toTimeT(WebsTime when);
static char *getToken(Webs *wp, char *delim, int validation);
static bool     hasUnsafeEscape(cchar *path);
static bool     hasToken(cchar *list, cchar *token);
static char *headerVarName(cchar *name);
static void     stripHopHeaders(Webs *wp, cchar *list);
static void     parseFirstLine(Webs *wp);
static void     parseHeaders(Webs *wp);
#if ME_GOAHEAD_TRACING && ME_GOAHEAD_LOGGING
static char *traceTarget(cchar *url);
static cchar *traceValue(cchar *key, cchar *value);
#endif
static void     setForwardedScheme(Webs *wp);
static bool     processContent(Webs *wp);
static bool     putStaged(Webs *wp);
static bool     parseIncoming(Webs *wp);
static void     pruneExpired(void);
static void     pruneSessions(void *data, int id);
static void     freeSession(WebsSession *sp);
static void     freeSessions(void);
static void     releaseSession(Webs *wp);
static void     releaseSessionRef(WebsSession **ref);
static bool     retireSession(WebsSession *sp);
static int      sessionHolders(WebsSession *sp);
static void     readEvent(Webs *wp);
static void     resetRequestVars(Webs *wp);
static void     reuseConn(Webs *wp);
static void     setFileLimits(void);
static void     setLocalHost(void);
static void     socketEvent(int sid, int mask, void *data);
static void     writeEvent(Webs *wp);
static char *validateToken(char *token, char *endToken, int validation);

#if ME_GOAHEAD_ACCESS_LOG
static void     logRequest(Webs *wp, int code);
#endif

/*********************************** Code *************************************/

PUBLIC int websOpen(cchar *documents, cchar *routeFile)
{
    WebsMime     *mt;
    WebsFileInfo info;

    webs = NULL;
    websMax = 0;

    websOsOpen();
#if ME_UNIX_LIKE && defined(SIGPIPE)
    ignorePipeSignal();
#endif
    websRuntimeOpen();
    websTimeOpen();
    if (websTime() < 0) {
        error("The system clock is unavailable, so sessions, timeouts and Digest authentication cannot work");
        return -1;
    }
    if (sizeof(time_t) < sizeof(int64)) {
        error("Warning: this build has a %d bit time_t, so sessions and Digest authentication stop working in 2038. "
              "Build with a 64 bit time_t", (int) sizeof(time_t) * 8);
    }
    websFsOpen();
    /*
        Validate the document root before allocating server resources, after ROM filesystem setup.
     */
    if (!documents) {
        documents = ME_GOAHEAD_DOCUMENTS;
    }
    if (*documents == '\0' || websStatFile(documents, &info) < 0 || !info.isDir) {
        error("Documents directory \"%s\" is not a directory", documents);
        return -1;
    }
#if ME_GOAHEAD_LOGGING
    if (logOpen() < 0) {
        error("Cannot open log file %s", logGetPath());
        return -1;
    }
#endif
    setFileLimits();
    socketOpen();
    setLocalHost();
#if ME_COM_SSL
    if (sslOpen() < 0) {
        return -1;
    }
    if (ME_GOAHEAD_SSL_VERIFY_PEER && !ME_GOAHEAD_SSL_VERIFY_ISSUER &&
        (*ME_GOAHEAD_SSL_AUTHORITY || *ME_GOAHEAD_SSL_REVOKE)) {
        error("Warning: ME_GOAHEAD_SSL_VERIFY_ISSUER is off, so any client certificate is accepted and the "
              "authority and revocation list are not enforced");
    }
#endif
    if ((sessions = hashCreate(-1)) < 0) {
        return -1;
    }
    if ((connIps = hashCreate(WEBS_HASH_INIT)) < 0) {
        return -1;
    }
    if (trustedProxies == 0) {
        websSetTrustedProxies(ME_GOAHEAD_TRUSTED_PROXIES);
    }
    if (!websDebug) {
        pruneId = websStartEvent(WEBS_SESSION_PRUNE, pruneSessions, 0);
    }
    websSetDocuments(documents);
    if (websOpenRoute() < 0) {
        return -1;
    }
#if ME_GOAHEAD_CGI
    websCgiOpen();
#endif
    websOptionsOpen();
    websActionOpen();
    websFileOpen();
#if ME_GOAHEAD_UPLOAD
    websUploadOpen();
#endif
#if ME_GOAHEAD_JAVASCRIPT
    websJstOpen();
#endif
#if ME_GOAHEAD_AUTH
    if (websOpenAuth(0) < 0) {
        return -1;
    }
#endif
    if (routeFile && websLoad(routeFile) < 0) {
        return -1;
    }
    /*
        Create a mime type lookup table for quickly determining the content type
     */
    websMime = hashCreate(WEBS_HASH_INIT * 4);
    assert(websMime >= 0);
    for (mt = websMimeList; mt->type; mt++) {
        hashEnter(websMime, mt->ext, valueString(mt->type, 0), 0);
    }
    addResponseHeaders(ME_GOAHEAD_RESPONSE_HEADERS);

#if ME_GOAHEAD_ACCESS_LOG && !ME_ROM
    if (websStartLogFile(&accessFile, ACCESS_LOG) < 0) {
        error("Cannot open access log %s", ACCESS_LOG);
        return -1;
    }
#endif
    return 0;
}


PUBLIC void websClose(void)
{
    Webs *wp;
    int  i;

    websCloseRoute();
#if ME_GOAHEAD_AUTH
    websCloseAuth();
#endif
    if (pruneId >= 0) {
        websStopEvent(pruneId);
        pruneId = -1;
    }
    for (i = 0; i < listenMax; i++) {
        if (listens[i] >= 0) {
            socketCloseConnection(listens[i]);
            listens[i] = -1;
        }
    }
    listenMax = 0;
    routesAudited = 0;
    for (i = websMax - 1; webs && i >= 0; i--) {
        if ((wp = webs[i]) != NULL) {
            websFree(wp);
        }
    }
    /*
        Free the sessions after the open connections. Requests hold session references that they release
        in termWebs, so freeing the session store first would leave those releases touching freed memory.
     */
    if (sessions >= 0) {
        freeSessions();
    }
    wfree(websHostUrl);
    wfree(websIpAddrUrl);
    websIpAddrUrl = websHostUrl = NULL;
    wfreeset(trustedProxies);

#if ME_COM_SSL
    sslClose();
#endif
#if ME_GOAHEAD_ACCESS_LOG && !ME_ROM
    websCloseLogFile(&accessFile);
#endif
    websFsClose();
    hashFree(websMime);
    hashFree(connIps);
    connIps = -1;
    if (responseHeaders >= 0) {
        hashFree(responseHeaders);
        responseHeaders = -1;
    }
    socketClose();
    logClose();
    websTimeClose();
    websRuntimeClose();
#if ME_UNIX_LIKE && defined(SIGPIPE)
    restorePipeSignal();
#endif
    websOsClose();
}


#if ME_UNIX_LIKE && defined(SIGPIPE)
/* Ignore default SIGPIPE to cover TLS writes as well as socketWrite. Preserve application-installed handlers. */
static void ignorePipeSignal(void)
{
    struct sigaction act, old;

    if (sigaction(SIGPIPE, NULL, &old) == 0 && !(old.sa_flags & SA_SIGINFO) && old.sa_handler == SIG_DFL) {
        memset(&act, 0, sizeof(act));
        act.sa_handler = SIG_IGN;
        sigemptyset(&act.sa_mask);
        pipeIgnored = sigaction(SIGPIPE, &act, NULL) == 0;
    }
}


static void restorePipeSignal(void)
{
    if (pipeIgnored) {
        signal(SIGPIPE, SIG_DFL);
        pipeIgnored = 0;
    }
}
#endif


static void initWebs(Webs *wp, int flags, int reuse)
{
    WebsBuf  rxbuf;
    WebsTime timestamp;
    void     *ssl;
    char     ipaddr[ME_MAX_IP], ifaddr[ME_MAX_IP];
    int      wid, sid, timeout, listenSid;

    assert(wp);

    memset(&rxbuf, 0, sizeof(rxbuf));
    if (reuse) {
        rxbuf = wp->rxbuf;
        wid = wp->wid;
        sid = wp->sid;
        timeout = wp->timeout;
        ssl = wp->ssl;
        listenSid = wp->listenSid;
        scopy(ipaddr, sizeof(ipaddr), wp->ipaddr);
        scopy(ifaddr, sizeof(ifaddr), wp->ifaddr);
        timestamp = wp->timestamp;
    } else {
        wid = sid = -1;
        timeout = -1;
        ssl = 0;
        listenSid = -1;
        timestamp = 0;
    }
    memset(wp, 0, sizeof(Webs));
    wp->flags = flags;
    wp->state = WEBS_BEGIN;
    wp->wid = wid;
    wp->sid = sid;
    wp->timeout = timeout;
    wp->docfd = -1;
    wp->txLen = -1;
    wp->rxLen = -1;
    wp->responseCookies = hashCreate(7);
    wp->code = HTTP_CODE_OK;
    wp->ssl = ssl;
    wp->listenSid = listenSid;
    wp->timestamp = timestamp;
#if !ME_ROM
    wp->putfd = -1;
#endif
#if ME_GOAHEAD_CGI
    wp->cgifd = -1;
#endif
#if ME_GOAHEAD_UPLOAD
    wp->files = -1;
    wp->upfd = -1;
#endif
    if (reuse) {
        scopy(wp->ipaddr, sizeof(wp->ipaddr), ipaddr);
        scopy(wp->ifaddr, sizeof(wp->ifaddr), ifaddr);
    } else {
        wp->timeout = -1;
    }
    wp->vars = hashCreate(WEBS_HASH_INIT);
    /*
        Ring queues can never be totally full and are short one byte. Better to do even I/O and allocate
        a little more memory than required. The chunkbuf has extra room to fit chunk headers and trailers.
     */
    assert(ME_GOAHEAD_LIMIT_BUFFER >= 1024);
    bufCreate(&wp->output, ME_GOAHEAD_LIMIT_BUFFER + 1, ME_GOAHEAD_LIMIT_BUFFER + 1);
    bufCreate(&wp->chunkbuf, ME_GOAHEAD_LIMIT_BUFFER + 1, ME_GOAHEAD_LIMIT_BUFFER * 2);
    bufCreate(&wp->input, ME_GOAHEAD_LIMIT_BUFFER + 1, ME_GOAHEAD_LIMIT_PUT + 1);
    if (reuse) {
        wp->rxbuf = rxbuf;
    } else {
        bufCreate(&wp->rxbuf, ME_GOAHEAD_LIMIT_HEADERS, ME_GOAHEAD_LIMIT_HEADERS + ME_GOAHEAD_LIMIT_PUT);
    }
}


static void termWebs(Webs *wp, int reuse)
{
    assert(wp);

    /*
        Some of this is done elsewhere, but keep this here for when a shutdown is done and there are open connections.
     */
    bufFree(&wp->input);
    bufFree(&wp->output);
    bufFree(&wp->chunkbuf);
    if (!reuse) {
        bufFree(&wp->rxbuf);
        if (wp->sid >= 0) {
#if ME_COM_SSL
            sslFree(wp);
#endif
            socketDeleteHandler(wp->sid);
            //  Let the client read the final response before unread input resets the connection. TLS closes at once
            socketLinger(wp->sid, (wp->flags & WEBS_SECURE) ? 0 : ME_GOAHEAD_LIMIT_LINGER);
            wp->sid = -1;
        }
    }
#if !ME_ROM
    if (wp->putfd >= 0) {
        close(wp->putfd);
        wp->putfd = -1;
    }
    //  A staged PUT body the file handler did not commit
    if (wp->putname) {
        unlink(wp->putname);
    }
#endif
#if ME_GOAHEAD_CGI
    /*
        Unregister before releasing anything the CGI registry borrows from this request. This covers
        reuse too: a keep-alive reuse frees wp->cgiStdin, which the registry slot points at.
     */
    websCgiDetach(wp);
    if (wp->cgifd >= 0) {
        close(wp->cgifd);
        wp->cgifd = -1;
    }
    if (wp->cgiStdin) {
        unlink(wp->cgiStdin);
        wfreeset(wp->cgiStdin);
    }
#endif
#if ME_GOAHEAD_UPLOAD
    wfreeset(wp->clientFilename);
#endif
    websPageClose(wp);
    wp->route = 0;
    websReapRoutes();
    if (wp->timeout >= 0 && !reuse) {
        websCancelTimeout(wp);
    }
    wfreeset(wp->authDetails);
    wfreeset(wp->authResponse);
    wfreeset(wp->authType);
    wfreeset(wp->contentType);
    wfreeset(wp->cookie);
    wfreeset(wp->decodedQuery);
    wfreeset(wp->digest);
    wfreeset(wp->ext);
    wfreeset(wp->filename);
    wfreeset(wp->host);
    wfreeset(wp->ifNoneMatch);
    wfreeset(wp->method);
    wfreeset(wp->password);
    wfreeset(wp->path);
    wfreeset(wp->protoVersion);
    wfreeset(wp->putname);
    wfreeset(wp->query);
    wfreeset(wp->realm);
    wfreeset(wp->referrer);
    wfreeset(wp->url);
    wfreeset(wp->userAgent);
    wfreeset(wp->username);
#if ME_GOAHEAD_UPLOAD
    wfreeset(wp->boundary);
    wfreeset(wp->uploadTmp);
    wfreeset(wp->uploadVar);
#endif
#if ME_GOAHEAD_DIGEST
    wfreeset(wp->cnonce);
    wfreeset(wp->digestUri);
    wfreeset(wp->opaque);
    wfreeset(wp->nc);
    wfreeset(wp->nonce);
    wfreeset(wp->qop);
#endif
    hashFree(wp->vars);
    hashFree(wp->responseCookies);

    /*
        Release the session reference on both close and keep-alive reuse. initWebs() memsets the request
        on reuse, so a release deferred to websFree would drop the last reference to a retired session
        without freeing it.
     */
    releaseSession(wp);

#if ME_GOAHEAD_UPLOAD
    if (wp->files >= 0) {
        websFreeUpload(wp);
    }
#endif
}


PUBLIC int websAlloc(int sid)
{
    Webs *wp;
    int  wid;

    if ((wid = wallocObject(&webs, &websMax, sizeof(Webs))) < 0) {
        return -1;
    }
    /*
        Count only after the allocation that can fail, and decrement in websFree alone. One increment
        site and one decrement site is the whole of the correctness here: a count incremented before a
        failing allocation, or never rolled back, drifts until it reads as permanently full or
        permanently empty.
     */
    connCount++;
    wp = webs[wid];
    assert(wp);
    initWebs(wp, 0, 0);
    wp->wid = wid;
    wp->sid = sid;
    wp->timestamp = websTime();
    return wid;
}


static void reuseConn(Webs *wp)
{
    assert(wp);
    assert(websValid(wp));

    bufCompact(&wp->rxbuf);
    if (bufLen(&wp->rxbuf)) {
        socketReservice(wp->sid);
    }
    termWebs(wp, 1);
    /*
        Preserve only connection-level WEBS_SECURE; recompute HTTP version and persistence per request.
     */
    initWebs(wp, wp->flags & WEBS_SECURE, 1);
}


PUBLIC void websFree(Webs *wp)
{
    assert(wp);
    assert(websValid(wp));

    adjustIpCount(wp->ipaddr, -1);
    termWebs(wp, 0);
    websMax = wfreeHandle(&webs, wp->wid);
    wfree(wp);
    assert(websMax >= 0);
    if (connCount > 0) {
        connCount--;
    }
}


/*
    Adjust the live count for a peer address by delta. The entry is removed when it reaches zero: the
    table is keyed by attacker-chosen addresses, so leaving spent entries behind turns the control
    into its own unbounded allocation.
 */
static void adjustIpCount(cchar *ip, int delta)
{
    WebsKey *key;
    int     count;

    if (connIps < 0 || connLimitPerIp <= 0 || ip == 0 || *ip == '\0') {
        return;
    }
    count = ((key = hashLookup(connIps, ip)) != 0) ? (int) key->content.value.integer : 0;
    count += delta;
    if (count <= 0) {
        if (key) {
            hashDelete(connIps, ip);
        }
        return;
    }
    hashEnter(connIps, ip, valueInteger(count), 0);
}


PUBLIC void websSetConnectionLimitPerIp(int limit)
{
    connLimitPerIp = limit < 0 ? ME_GOAHEAD_LIMIT_CONNECTIONS_PER_IP : limit;
}


PUBLIC int websGetConnectionCountForIp(cchar *ip)
{
    WebsKey *key;

    if (connIps < 0 || ip == 0 || *ip == '\0') {
        return 0;
    }
    return ((key = hashLookup(connIps, ip)) != 0) ? (int) key->content.value.integer : 0;
}


PUBLIC void websSetConnectionLimit(int limit)
{
    connLimit = limit > 0 ? limit : ME_GOAHEAD_LIMIT_CONNECTIONS;
}


PUBLIC int websGetConnectionCount(void)
{
    return connCount;
}


PUBLIC int websGetSessionCount(void)
{
    return sessionCount;
}


/*
    JST callbacks can write through any normal response API. Commit default headers only when
    a body is written or an empty page completes, so an early error can still choose its status.
 */
static void writeJstHeaders(Webs *wp)
{
    if (wp->flags & WEBS_JST_HEADERS) {
        websWriteHeaders(wp, -1, 0);
        websWriteHeader(wp, "Pragma", "no-cache");
        websWriteEndHeaders(wp);
    }
}


/*
    Called when the request is complete. Note: it may not have fully drained from the tx buffer.
 */
PUBLIC void websDone(Webs *wp)
{
    WebsSocket *sp;

    assert(wp);
    assert(websValid(wp));

    if (wp->finalized) {
        return;
    }
    assert(WEBS_BEGIN <= wp->state && wp->state <= WEBS_COMPLETE);
    writeJstHeaders(wp);
    wp->flags |= WEBS_FINALIZED;
    wp->finalized = 1;

    /*
        Once running, it is the handlers responsibility to conclude the request.
     */
    if (wp->connError && wp->state < WEBS_RUNNING) {
        wp->state = WEBS_COMPLETE;
    }
    if (wp->state < WEBS_COMPLETE) {
        /*
            Initiate flush. If not all flushed, wait for output to drain via a socket event.
         */
        if (websFlush(wp, 0) == 0 && (sp = socketPtr(wp->sid)) != NULL) {
            socketCreateHandler(wp->sid, sp->handlerMask | SOCKET_WRITABLE, socketEvent, wp);
        }
    }
#if ME_GOAHEAD_ACCESS_LOG
    logRequest(wp, wp->code);
#endif
    if (!(wp->flags & WEBS_RESPONSE_TRACED)) {
        trace(3, "Request complete: code %d", wp->code);
    }
}


static int complete(Webs *wp, int reuse)
{
    assert(wp);
    assert(websValid(wp));
    assert(wp->state == WEBS_BEGIN || wp->state == WEBS_COMPLETE);

    /*
        Use eof for body completion; rxRemaining reaches zero between chunks too.
     */
    if (reuse && wp->flags & WEBS_KEEP_ALIVE && wp->eof) {
        reuseConn(wp);
        socketCreateHandler(wp->sid, SOCKET_READABLE, socketEvent, wp);
        trace(5, "Keep connection alive");
        return 1;
    }
    trace(5, "Close connection");
    wp->state = WEBS_BEGIN;
    wp->flags |= WEBS_CLOSED;
    return 0;
}


/*
    Audit routes at first listen, after all configuration files are loaded.
 */
static int auditRoutes(void)
{
    logmsg(2, "Strict mode %s", websGetStrict() ? "on: an unsafe route table stops startup" : "off");
    if (websCheckRoutes() > 0 && websGetStrict()) {
        error("Strict mode: refusing to start with an unsafe route configuration");
        return -1;
    }
    /* Fail startup if request staging is inside a served directory. */
    if (websCheckTempDirs() > 0) {
        return -1;
    }
    return 0;
}


PUBLIC int websListen(cchar *endpoint)
{
    WebsSocket *sp;
    char       *ip;

#if ME_GOAHEAD_LOGGING
    char *ipaddr;
#endif
    int port, secure, sid;

    assert(endpoint && *endpoint);

    if (!routesAudited) {
        if (auditRoutes() < 0) {
            return -1;
        }
        routesAudited = 1;
    }
    if (listenMax >= WEBS_MAX_LISTEN) {
        error("Too many listen endpoints");
        return -1;
    }
    socketParseAddress(endpoint, &ip, &port, &secure, 80);
    if (secure) {
#if ME_COM_SSL
        if (!sslHasCertificate()) {
            error("Cannot listen on %s: no certificate and key are configured (use --cert and --key)", endpoint);
            wfree(ip);
            return -1;
        }
#else
        error("Cannot listen on %s: SSL is not enabled in this build", endpoint);
        wfree(ip);
        return -1;
#endif
    }
    if ((sid = socketListen(ip, port, websAccept, 0)) < 0) {
        error("Unable to open socket on port %d.", port);
        wfree(ip);
        return -1;
    }
    if ((sp = socketPtr(sid)) == NULL) {
        wfree(ip);
        return -1;
    }
    sp->secure = secure;
    if (sp->secure) {
        if (!defaultSslPort) {
            defaultSslPort = port;
        }
    } else if (!defaultHttpPort) {
        defaultHttpPort = port;
    }
    listens[listenMax++] = sid;
#if ME_GOAHEAD_LOGGING
    if (ip) {
        ipaddr = smatch(ip, "::") ? "[::]" : ip;
    } else {
        ipaddr = "*";
    }
    logmsg(2, "Started %s://%s:%d", secure ? "https" : "http", ipaddr, port);
#endif

    if (!websIpAddrUrl) {
        if (port == 80) {
            websIpAddrUrl = sclone(websIpAddr);
        } else {
            websIpAddrUrl = sfmt("%s:%d", websIpAddr, port);
        }
    }
    wfree(ip);
    return sid;
}


/*
    Accept a new connection from ipaddr:port
 */
PUBLIC int websAccept(int sid, cchar *ipaddr, int port, int listenSid)
{
    Webs                    *wp;
    WebsSocket              *lp, *sp;
    struct sockaddr_storage ifAddr;
    int                     wid, len;

    assert(sid >= 0);
    assert(ipaddr && *ipaddr);
    assert(listenSid >= 0);
    assert(port >= 0);

    /*
        Refuse before allocating anything. Returning -1 makes socketAccept call socketFree, which
        shuts down and closes the descriptor, so the peer is dropped without reading a request from it.
     */
    if (connCount >= connLimit) {
        trace(2, "Refused connection from %s: at connection limit %d", ipaddr, connLimit);
        return -1;
    }
    /*
        The total cap above bounds resource use but refuses everyone equally once reached, so a flood
        denies service to the clients it is meant to protect. The per-peer cap is what preserves
        capacity for other addresses.
     */
    if (connLimitPerIp > 0 && !websIsTrustedProxy(ipaddr) && websGetConnectionCountForIp(ipaddr) >= connLimitPerIp) {
        trace(2, "Refused connection from %s: at per-address limit %d", ipaddr, connLimitPerIp);
        return -1;
    }
    /*
        Allocate a new handle for this accepted connection. This will allocate a Webs structure in the webs[] list
     */
    if ((wid = websAlloc(sid)) < 0) {
        return -1;
    }
    wp = webs[wid];
    assert(wp);
    wp->listenSid = listenSid;
    strncpy(wp->ipaddr, ipaddr, sizeof(wp->ipaddr) - 1);
    wp->ipaddr[sizeof(wp->ipaddr) - 1] = '\0';
    /*
        Count against the peer only once the address is recorded on the connection, so the matching
        decrement in websFree can find the same key. Every failure path below releases through
        websFree, so the pair holds without an explicit rollback here.
     */
    adjustIpCount(wp->ipaddr, 1);

    /*
        Get the ip address of the interface that accept the connection.
     */
    memset(&ifAddr, 0, sizeof(ifAddr));
    len = sizeof(ifAddr);
    if ((sp = socketPtr(sid)) == NULL || getsockname(sp->sock, (struct sockaddr*) &ifAddr, (Socklen*) &len) < 0) {
        error("Cannot get sockname");
        abandonAccept(wp);
        return -1;
    }
    socketAddress((struct sockaddr*) &ifAddr, (int) len, wp->ifaddr, sizeof(wp->ifaddr), NULL);

#if ME_GOAHEAD_LEGACY
    /*
        Check if this is a request from a browser on this system. This is useful to know for permitting administrative
        operations only for local access
     */
    if (strcmp(wp->ipaddr, "127.0.0.1") == 0 || strcmp(wp->ipaddr, websIpAddr) == 0 ||
        strcmp(wp->ipaddr, websHost) == 0) {
        wp->flags |= WEBS_LOCAL;
    }
#endif

    /*
        Arrange for socketEvent to be called when read data is available
     */
    if ((lp = socketPtr(listenSid)) == NULL) {
        abandonAccept(wp);
        return -1;
    }
    trace(4, "New connection from %s:%d to %s:%d", ipaddr, port, wp->ifaddr, lp->port);

#if ME_COM_SSL
    if (lp->secure) {
        wp->flags |= WEBS_SECURE;
        trace(4, "Upgrade connection to TLS");
        if (sslUpgrade(wp) < 0) {
            error("Cannot upgrade to TLS");
            abandonAccept(wp);
            return -1;
        }
    }
#endif
    assert(wp->timeout == -1);
    wp->timeout = websStartEvent(PARSE_TIMEOUT, checkTimeout, (void*) wp);
    socketEvent(sid, SOCKET_READABLE, wp);
    return 0;
}


/* On accept failure, free the request only; socketAccept owns the socket cleanup. */
static void abandonAccept(Webs *wp)
{
#if ME_COM_SSL
    sslFree(wp);
#endif
    wp->sid = -1;
    websFree(wp);
}


/*
    The webs socket handler. Called in response to I/O. We just pass control to the relevant read or write handler. A
    pointer to the webs structure is passed as a (void*) in wptr.
 */
static void socketEvent(int sid, int mask, void *wptr)
{
    Webs *wp;

    wp = (Webs*) wptr;
    assert(wp);

    assert(websValid(wp));
    if (!websValid(wp)) {
        return;
    }
    if (mask & SOCKET_READABLE) {
        readEvent(wp);
    }
    if (mask & SOCKET_WRITABLE) {
        writeEvent(wp);
    }
    if (wp->flags & WEBS_CLOSED) {
        websFree(wp);
        /* WARNING: wp not valid here */
    }
}


/*
    Read from a connection. Return the number of bytes read if successful. This may be less than the requested "len" and
    may be zero. Return -1 for errors or EOF. Distinguish between error and EOF via socketEof().
 */
static ssize websRead(Webs *wp, char *buf, ssize len)
{
    assert(wp);
    assert(buf);
    assert(len > 0);
#if ME_COM_SSL
    if (wp->flags & WEBS_SECURE) {
        return sslRead(wp, buf, len);
    }
#endif
    return socketRead(wp->sid, buf, len);
}


/*
    The webs read handler. This is the primary read event loop. It uses a state machine to track progress while parsing
    the HTTP request.  Note: we never block as the socket is always in non-blocking mode.
 */
static void readEvent(Webs *wp)
{
    WebsBuf    *rxbuf;
    WebsSocket *sp;
    ssize      nbytes;

    assert(wp);
    assert(websValid(wp));

    if (!websValid(wp)) {
        return;
    }
    rxbuf = &wp->rxbuf;

    if (bufRoom(rxbuf) < (ME_GOAHEAD_LIMIT_BUFFER + 1)) {
        if (!bufGrow(rxbuf, ME_GOAHEAD_LIMIT_BUFFER + 1)) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot grow rxbuf");
            websPump(wp);
            return;
        }
    }
    if ((nbytes = websRead(wp, (char*) rxbuf->endp, ME_GOAHEAD_LIMIT_BUFFER)) > 0) {
        /*
            Start the parse deadline on the first request byte. Only received request bytes renew idle time; TLS
            events without data must not extend it.
         */
        if (wp->started == 0) {
            wp->started = websTime();
        }
        websNoteRequestActivity(wp);
        wp->lastRead = nbytes;
        bufAdjustEnd(rxbuf, nbytes);
    }
    bufAddNull(rxbuf);
    if (nbytes > 0 || wp->state > WEBS_BEGIN) {
        websPump(wp);
    }
    if (wp->flags & WEBS_CLOSED) {
        return;
    } else if (nbytes < 0 && socketEof(wp->sid)) {
        /* EOF or error. Allow running requests to continue. */
        if (wp->state < WEBS_READY) {
            if (wp->state > WEBS_BEGIN) {
                websError(wp, HTTP_CODE_COMMS_ERROR | WEBS_CLOSE, "Read error: connection lost");
                websPump(wp);
            } else {
                complete(wp, 0);
            }
        } else {
            socketDeleteHandler(wp->sid);
        }
    } else if (wp->state < WEBS_READY) {
        if ((sp = socketPtr(wp->sid)) != NULL) {
            socketCreateHandler(wp->sid, sp->handlerMask | SOCKET_READABLE, socketEvent, wp);
        }
    }
}


PUBLIC void websPump(Webs *wp)
{
    bool canProceed;

    for (canProceed = 1; canProceed; ) {
        switch (wp->state) {
        case WEBS_BEGIN:
            canProceed = parseIncoming(wp);
            break;
        case WEBS_CONTENT:
            canProceed = processContent(wp);
            break;
        case WEBS_READY:
            /*
                A finalized request is terminal. websDone sets wp->finalized but leaves wp->state
                alone, so without this test a request whose response has already been written is
                dispatched to its handler regardless.
             */
            if (wp->finalized) {
                canProceed = concludeRequest(wp);
                break;
            }
            if (!websRunRequest(wp)) {
                /* Reroute if the handler re-wrote the request */
                websRouteRequest(wp);
                /*
                    The reroute may have selected a route whose authentication or ability check
                    denied the request and wrote its response. Forcing WEBS_READY here would run
                    that route's handler anyway, serving the body the gate just refused.
                 */
                if (wp->finalized) {
                    canProceed = concludeRequest(wp);
                    break;
                }
                wp->state = WEBS_READY;
                canProceed = 1;
                continue;
            }
            canProceed = (wp->state != WEBS_RUNNING);
            break;
        case WEBS_RUNNING:
            /* Nothing to do until websDone is called */
            return;
        case WEBS_COMPLETE:
            canProceed = complete(wp, 1);
            break;
        }
    }
}


/*
    Complete finalized requests only after output drains; writeEvent handles pending output. Return true when
    complete.
 */
static bool concludeRequest(Webs *wp)
{
    if (bufLen(&wp->output) > 0 || (wp->flags & WEBS_CHUNKING)) {
        wp->state = WEBS_RUNNING;
        return 0;
    }
    wp->state = WEBS_COMPLETE;
    return 1;
}


static bool parseIncoming(Webs *wp)
{
    WebsBuf *rxbuf;

#if ME_GOAHEAD_CGI || ME_GOAHEAD_UPLOAD
    bool cgi;
#endif

    rxbuf = &wp->rxbuf;
    /*
        Ignore blank lines before the request line, as clients may emit a trailing CRLF after a body.
        These bytes are consumed here, so they never reach the LIMIT_HEADER test below and cannot be
        measured by bufLen. An unbounded strip is therefore a free and unloggable way to hold a
        connection open forever. Charge them against a separate budget.

        The budget must live on the connection. parseIncoming is re-entered on every read event, so a
        counter local to this function would restart at zero for each instalment and a client feeding
        two bytes at a time would never reach the cap. initWebs memsets the request state, so the
        count resets for each request on a keep-alive connection without an explicit reset here.
     */
    while (*rxbuf->servp == '\r' || *rxbuf->servp == '\n') {
        if (bufGetc(rxbuf) < 0) {
            break;
        }
        /*
            Charge only bytes actually discarded. Testing the budget before the read would also charge
            a pass that consumed nothing, and parseIncoming is re-entered whenever the connection is
            serviced, so an idle connection would accumulate its way to a spurious rejection.
         */
        if (++wp->rxPrologue > MAX_PROLOGUE) {
            websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Too many leading blank lines");
            return 1;
        }
    }
    if (strstr(bufStart(&wp->rxbuf), "\r\n\r\n") == 0) {
        if (bufLen(&wp->rxbuf) >= ME_GOAHEAD_LIMIT_HEADER) {
            websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Header too large");
            return 1;
        }
        return 0;
    }
    trace(3, "<<< Request");

    /*
        Parse the first line of the Http header
     */
    parseFirstLine(wp);
    if (wp->state == WEBS_COMPLETE) {
        return 1;
    }
    parseHeaders(wp);
    if (wp->state == WEBS_COMPLETE) {
        return 1;
    }
    wp->state = (wp->rxChunkState || wp->rxLen > 0) ? WEBS_CONTENT : WEBS_READY;

    websRouteRequest(wp);

    /*
        Do not stage a body after routing has refused the request.
     */
    if (wp->state == WEBS_COMPLETE || wp->finalized || !wp->route) {
        return 1;
    }
#if ME_GOAHEAD_CGI
    cgi = wp->route && wp->route->handler && wp->route->handler->service == cgiHandler;
#elif ME_GOAHEAD_UPLOAD
    cgi = 0;
#endif
#if ME_GOAHEAD_UPLOAD
    /*
        Parse uploads only after route authentication and opt-in; CGI may parse its own multipart body.
     */
    if ((wp->flags & WEBS_UPLOAD) && !(wp->route && (wp->route->flags & WEBS_ROUTE_UPLOAD))) {
        wp->flags &= ~WEBS_UPLOAD;
        if (!cgi) {
            websError(wp, HTTP_CODE_UNSUPPORTED_MEDIA_TYPE | WEBS_CLOSE, "Uploads are not accepted for this URI");
            return 1;
        }
    }
#endif
#if ME_GOAHEAD_CGI
    //  A CGI program reads the request body on stdin, whatever the method
    if (cgi) {
        if (smatch(wp->method, "POST") || wp->state == WEBS_CONTENT) {
            if ((wp->cgiStdin = websGetCgiCommName()) == 0) {
                websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Cannot create CGI file");
                return 1;
            }
            /*
                websGetCgiCommName created the file, so this opens an existing private file rather
                than creating one. O_NOFOLLOW keeps that true if the path is replaced in between.
             */
            if ((wp->cgifd = open(wp->cgiStdin, O_WRONLY | O_BINARY | O_TRUNC | O_NOFOLLOW, 0600)) < 0) {
                websError(wp, HTTP_CODE_NOT_FOUND | WEBS_CLOSE, "Cannot open CGI file");
                return 1;
            }
        }
    }
#endif
#if !ME_ROM
    /*
        Only the file handler stages PUT documents. Other handlers receive the body normally.
     */
    if (smatch(wp->method, "PUT") && wp->route->handler && smatch(wp->route->handler->name, "file")) {
        if (websPageIsDirectory(wp)) {
            websError(wp, HTTP_CODE_CONFLICT | WEBS_CLOSE, "Cannot put a directory");
            return 1;
        }
        wfreeset(wp->putname);
        if ((wp->putname = websTempFile(ME_GOAHEAD_PUT_DIR, "put")) == 0) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create the put URI");
            return 1;
        }
        /*
            The staging file exists and is private. Open it rather than create it, so a path swapped
            underneath us fails here instead of writing the request body somewhere else.
         */
        if ((wp->putfd = open(wp->putname, O_BINARY | O_WRONLY | O_NOFOLLOW, 0600)) < 0) {
            error("Cannot create PUT filename %s", wp->putname);
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create the put URI");
            unlink(wp->putname);
            wfreeset(wp->putname);
            return 1;
        }
    }
#endif
    /*
        Apply the POST memory limit to bodies not staged as files, regardless of method.
     */
    if (!putStaged(wp) && wp->rxLen > postLimit) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Too big");
        return 1;
    }
    return 1;
}


/*
    Only a nonempty suffix on the final path component is an extension.
 */
static char *getPathExtension(char *path)
{
    char *dot, *slash;

    dot = strrchr(path, '.');
    slash = strrchr(path, '/');
    return dot && dot[1] && (!slash || dot > slash) ? dot : NULL;
}


/*
    Match comma-separated tokens case-insensitively, trimming optional whitespace.
 */
/*
    True when the request body is being staged to a PUT file rather than held in memory. ROM builds have no PUT.
 */
static bool putStaged(Webs *wp)
{
#if !ME_ROM
    return wp->putfd >= 0;
#else
    return 0;
#endif
}


static bool hasToken(cchar *list, cchar *token)
{
    cchar *cp, *start, *end;
    ssize len;

    if (list == NULL || token == NULL) {
        return 0;
    }
    len = slen(token);
    for (cp = list; *cp; ) {
        while (*cp == ',' || *cp == ' ' || *cp == '\t') {
            cp++;
        }
        start = cp;
        while (*cp && *cp != ',') {
            cp++;
        }
        for (end = cp; end > start && (end[-1] == ' ' || end[-1] == '\t'); end--) { }
        if ((end - start) == len && sncaselesscmp(start, token, len) == 0) {
            return 1;
        }
        if (*cp == ',') {
            cp++;
        }
    }
    return 0;
}


/*
    Remove named hop-by-hop fields, but retain framing, routing, authentication and origin fields. Compare variable
    names so that every spelling of a protected field is retained.
 */
static void stripHopHeaders(Webs *wp, cchar *list)
{
    cchar *protected[] = { "HTTP_CONNECTION", "HTTP_HOST", "HTTP_CONTENT_LENGTH", "HTTP_TRANSFER_ENCODING",
                           "HTTP_AUTHORIZATION", "HTTP_COOKIE", "HTTP_ORIGIN", "HTTP_SEC_FETCH_SITE",
                           "HTTP_CLOSE", "HTTP_KEEP_ALIVE", NULL };
    char  *copy, *option, *key, *tok;
    int   i;

    copy = sclone(list);
    for (option = stok(copy, ",", &tok); option; option = stok(NULL, ",", &tok)) {
        option = strim(option, " \t", WEBS_TRIM_BOTH);
        if (*option == '\0' || (key = headerVarName(option)) == 0) {
            continue;
        }
        for (i = 0; protected[i]; i++) {
            if (smatch(key, protected[i])) {
                break;
            }
        }
        if (!protected[i]) {
            hashDelete(wp->vars, key);
        }
        wfree(key);
    }
    wfree(copy);
}


/* Map only letter, digit and hyphen header names to HTTP_ variables, preserving a one-to-one mapping. */
static char *headerVarName(cchar *name)
{
    cchar *np;
    char  *var, *cp;

    for (np = name; *np; np++) {
        if (!isalnum((uchar) *np) && *np != '-') {
            return 0;
        }
    }
    var = sfmt("HTTP_%s", name);
    for (cp = var; *cp; cp++) {
        if (*cp == '-') {
            *cp = '_';
        }
    }
    return supper(var);
}


static bool isTchar(uchar c)
{
    return isalnum(c) || (c && strchr("!#$%&'*+-.^_`|~", c) != NULL);
}


static bool validMethod(cchar *method)
{
    cchar *cp;

    if (method == NULL || *method == '\0' || slen(method) > MAX_METHOD) {
        return 0;
    }
    for (cp = method; *cp; cp++) {
        if (!isTchar((uchar) *cp)) {
            return 0;
        }
    }
    return 1;
}


/*
    Validate RFC 9112 7.1.1 chunk extensions in [cp, end); reject whitespace consistently with chunk sizes.
 */
static bool validChunkExt(cchar *cp, cchar *end)
{
    uchar c;

    while (cp < end) {
        if (*cp++ != ';' || cp >= end || !isTchar((uchar) *cp)) {
            return 0;
        }
        while (cp < end && isTchar((uchar) *cp)) {
            cp++;
        }
        if (cp < end && *cp == '=') {
            if (++cp >= end) {
                return 0;
            }
            if (*cp == '"') {
                for (cp++; cp < end && *cp != '"'; cp++) {
                    c = (uchar) *cp;
                    if (c == '\\') {
                        if (++cp >= end) {
                            return 0;
                        }
                        c = (uchar) *cp;
                        if (c != '\t' && (c < 0x20 || c == 0x7F)) {
                            return 0;
                        }
                    } else if (c != '\t' && (c < 0x20 || c == 0x7F)) {
                        return 0;
                    }
                }
                if (cp++ >= end) {
                    return 0;
                }
            } else {
                if (!isTchar((uchar) *cp)) {
                    return 0;
                }
                while (cp < end && isTchar((uchar) *cp)) {
                    cp++;
                }
            }
        }
    }
    return 1;
}


static bool validVersion(cchar *version)
{
    return sncmp(version, "HTTP/", 5) == 0 && isdigit((uchar) version[5]) && version[6] == '.' &&
           isdigit((uchar) version[7]) && version[8] == '\0';
}


static ssize schemeLength(cchar *url)
{
    cchar *cp;

    if (url == 0 || !isalpha((uchar) *url)) {
        return 0;
    }
    for (cp = url + 1; isalnum((uchar) *cp) || *cp == '+' || *cp == '-' || *cp == '.'; cp++) { }
    return sncmp(cp, "://", 3) == 0 ? (cp - url) + 3 : 0;
}


static bool validHost(cchar *host)
{
    return host && *host && strspn(host, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-.[]:") ==
           strlen(host);
}


/*
    Check Origin and Sec-Fetch-Site against the client scheme and configured or request Host. Trust header variables
    only. Non-browser requests without either header pass.
 */
PUBLIC bool websSameOrigin(Webs *wp)
{
    WebsKey *sp;
    cchar   *authority, *port, *site;
    char    *expected;
    ssize   len, plen;
    bool    secure, same;

    if ((sp = hashLookup(wp->vars, "HTTP_ORIGIN")) == 0) {
        if ((sp = hashLookup(wp->vars, "HTTP_SEC_FETCH_SITE")) == 0) {
            return 1;
        }
        site = sp->content.value.string;
        return sp->arg == 0 && (smatch(site, "same-origin") || smatch(site, "none"));
    }
    authority = websHostUrl ? websHostUrl : wp->host;
    if (sp->arg != 0 || authority == 0) {
        return 0;
    }
    //  Browsers omit the default port from Origin, while a Host header may carry it
    secure = (wp->flags & WEBS_HTTPS) != 0;
    port = secure ? ":443" : ":80";
    len = slen(authority);
    plen = slen(port);
    if (len > plen && smatch(&authority[len - plen], port)) {
        len -= plen;
    }
    expected = sfmt("%s://%.*s", secure ? "https" : "http", (int) len, authority);
    same = scaselessmatch(sp->content.value.string, expected);
    wfree(expected);
    return same;
}


PUBLIC void websSetTrustedProxies(cchar *addresses)
{
    WebsBuf buf;
    char    *list, *address, *tok, *end;

    wfreeset(trustedProxies);
    if (addresses == 0 || *addresses == '\0') {
        return;
    }
    bufCreate(&buf, -1, -1);
    bufPutc(&buf, ',');
    list = sclone(addresses);
    for (address = stok(list, ", \t", &tok); address; address = stok(NULL, ", \t", &tok)) {
        if (*address == '[' && (end = strchr(address, ']')) != 0) {
            *end = '\0';
            address++;
        }
        if (*address) {
            bufPutStr(&buf, address);
            bufPutc(&buf, ',');
        }
    }
    wfree(list);
    bufAddNull(&buf);
    if (bufLen(&buf) > 1) {
        trustedProxies = sclone(bufStart(&buf));
    }
    bufFree(&buf);
}


PUBLIC bool websIsTrustedProxy(cchar *ipaddr)
{
    char key[ME_MAX_IP + 3];

    if (trustedProxies == 0 || ipaddr == 0 || *ipaddr == '\0') {
        return 0;
    }
    //  An IPv4 peer on a dual-stack listener is reported as an IPv4-mapped IPv6 address
    if (sstarts(ipaddr, "::ffff:") && strchr(&ipaddr[7], '.')) {
        ipaddr += 7;
    }
    fmt(key, sizeof(key), ",%s,", ipaddr);
    return strstr(trustedProxies, key) != 0;
}


/* Trust only the nearest proxy's right-most X-Forwarded-Proto value. Ignore it for other peers. */
static void setForwardedScheme(Webs *wp)
{
    cchar *proto, *cp;

    if (!websIsTrustedProxy(wp->ipaddr) || (proto = websGetVar(wp, "HTTP_X_FORWARDED_PROTO", 0)) == 0) {
        return;
    }
    if ((cp = strrchr(proto, ',')) != 0) {
        proto = cp + 1;
    }
    while (*proto == ' ' || *proto == '\t') {
        proto++;
    }
    if (scaselessmatch(proto, "https")) {
        wp->flags |= WEBS_HTTPS | WEBS_PROXIED;
    } else if (scaselessmatch(proto, "http")) {
        wp->flags = (wp->flags & ~WEBS_HTTPS) | WEBS_PROXIED;
    }
    wp->protocol = wp->flags & WEBS_HTTPS ? "https" : "http";
}


/*
    Parse the first line of a HTTP request
 */
static void parseFirstLine(Webs *wp)
{
    char  *op, *protoVer, *url, *host, *query, *path, *port, *ext, *buf, *authority;
    ssize schemeLen;
    int   listenPort;

#if ME_GOAHEAD_TRACING && ME_GOAHEAD_LOGGING
    char *target;
#endif

    assert(wp);
    assert(websValid(wp));

    /*
        Require method SP target SP version with exactly one space and no tabs.
     */
    op = getToken(wp, " ", TOKEN_WORD | TOKEN_STRICT);
    if (!validMethod(op)) {
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad HTTP request");
        return;
    }
    wp->method = sclone(op);

    url = getToken(wp, " ", TOKEN_URI_VALUE | TOKEN_STRICT);
    if (url == NULL || *url == '\0') {
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad HTTP request");
        return;
    }
    /* Origin-form targets do not supply a host. Absolute-form requires http/https and a valid authority. */
    authority = 0;
    if (*url != '/') {
        schemeLen = schemeLength(url);
        if (!(schemeLen == 7 && sncaselesscmp(url, "http://", 7) == 0) &&
            !(schemeLen == 8 && sncaselesscmp(url, "https://", 8) == 0)) {
            websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad HTTP request target");
            return;
        }
        authority = snclone(&url[schemeLen], strcspn(&url[schemeLen], "/?#"));
        if (!validHost(authority)) {
            websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad HTTP request target");
            wfree(authority);
            return;
        }
    }
    if (strlen(url) > ME_GOAHEAD_LIMIT_URI) {
        websError(wp, HTTP_CODE_REQUEST_URL_TOO_LARGE | WEBS_CLOSE, "URI too big");
        wfree(authority);
        return;
    }
    protoVer = getToken(wp, "\r\n", TOKEN_WORD | TOKEN_STRICT);
    if (protoVer == NULL || *protoVer == '\0') {
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad HTTP request");
        wfree(authority);
        return;
    }
#if ME_GOAHEAD_TRACING && ME_GOAHEAD_LOGGING
    if (websGetLogLevel() >= 2) {
        target = traceTarget(url);
        trace(2, "%s %s %s", wp->method, target, protoVer);
        wfree(target);
    }
#endif

    /*
        Parse the URL and store all the various URL components. websUrlParse returns an allocated buffer in buf which we
        must free. We support both proxied and non-proxied requests. Proxied requests will have http://host/ at the
        start of the URL. Non-proxied will just be local path names.
     */
    host = path = port = query = ext = NULL;
    if (websUrlParse(url, &buf, NULL, &host, &port, &path, NULL, NULL, &query) < 0) {
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Cannot parse the request URL");
        wfree(authority);
        return;
    }

    //  Decode the path and save. This includes the extension.
    if ((wp->path = websValidateUriPath(path)) == 0) {
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE | WEBS_NOLOG, "Bad URL");
        wfree(buf);
        wfree(authority);
        return;
    }
    wp->url = sclone(url);
    if ((ext = getPathExtension(wp->path)) != NULL) {
        wp->ext = sclone(slower(ext));
    }
    wp->filename = sfmt("%s%s", websGetDocuments(), wp->path);
    wp->query = sclone(query);
    if (authority) {
        /*
            The absolute-form authority overrides Host (RFC 9112 3.2.2). Export it as HTTP_HOST so a CGI program sees
            the host the server used.
         */
        wp->host = authority;
        wp->flags |= WEBS_ABSOLUTE_TARGET;
        websSetVar(wp, "HTTP_HOST", authority);
    } else {
        wp->host = sclone(host);
    }
    //  The connection's scheme, which a trusted proxy may override in parseHeaders
    if (wp->flags & WEBS_SECURE) {
        wp->flags |= WEBS_HTTPS;
    }
    wp->protocol = wp->flags & WEBS_HTTPS ? "https" : "http";
    if (smatch(protoVer, "HTTP/1.1")) {
        wp->flags |= WEBS_KEEP_ALIVE | WEBS_HTTP11;
    } else if (smatch(protoVer, "HTTP/1.0")) {
        wp->flags &= ~(WEBS_HTTP11);
    } else {
        //  A well-formed HTTP-version (RFC 9112 2.3) we do not speak is 505, anything else is malformed
        if (validVersion(protoVer)) {
            websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_VERSION, "Unsupported HTTP version");
        } else {
            websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad HTTP version");
        }
        wfree(buf);
        return;
    }
    wp->protoVersion = sclone(protoVer);
    if ((listenPort = socketGetPort(wp->listenSid)) >= 0) {
        wp->port = listenPort;
    } else {
        wp->port = atoi(port);
    }
    if (!websKnownMethod(wp->method)) {
        websError(wp, HTTP_CODE_NOT_IMPLEMENTED | WEBS_CLOSE, "Unsupported method");
    }
    wfree(buf);
}


/*
    Parse a full request
 */
/*
    Return a tracking bit for singleton headers, or zero for repeatable fields.
 */
static int singleValuedHeader(cchar *key)
{
    if (smatch(key, "content-length")) {
        return SEEN_CONTENT_LENGTH;
    }
    if (smatch(key, "transfer-encoding")) {
        return SEEN_TRANSFER_ENCODING;
    }
    if (smatch(key, "host")) {
        return SEEN_HOST;
    }
    if (smatch(key, "authorization")) {
        return SEEN_AUTHORIZATION;
    }
    if (smatch(key, "content-type")) {
        return SEEN_CONTENT_TYPE;
    }
    return 0;
}


/*
    Parse decimal or hexadecimal digits up to end, checking limit before multiplication.
    Return -1 on overflow and set next to the first unconsumed character.
 */
static ssize parseLimitedNumber(cchar *digits, cchar *end, int base, ssize limit, cchar **next)
{
    cchar *cp;
    ssize value;
    int   digit;

    value = 0;
    for (cp = digits; cp < end; cp++) {
        if (isdigit((uchar) *cp)) {
            digit = *cp - '0';
        } else if (base == 16 && isxdigit((uchar) *cp)) {
            digit = (tolower((uchar) *cp) - 'a') + 10;
        } else {
            break;
        }
        if (value >= 0) {
            value = (digit > limit || value > (limit - digit) / base) ? -1 : (value * base) + digit;
        }
    }
    *next = cp;
    return value;
}


#if ME_GOAHEAD_TRACING && ME_GOAHEAD_LOGGING
/*
    Release traces redact credentials, session IDs, and queries; debug traces retain full values.
 */
static cchar *traceValue(cchar *key, cchar *value)
{
    if (!ME_DEBUG && key &&
        (scaselessmatch(key, "authorization") || scaselessmatch(key, "proxy-authorization") ||
         scaselessmatch(key, "cookie") || scaselessmatch(key, "set-cookie"))) {
        return "<redacted>";
    }
    return value;
}



static char *traceTarget(cchar *url)
{
    cchar *query;

    if (!ME_DEBUG && (query = strchr(url, '?')) != 0) {
        return sfmt("%.*s?...", (int) (query - url), url);
    }
    return sclone(url);
}
#endif


static void parseHeaders(Webs *wp)
{
    cchar *prior, *connection, *next;
    char  *combined, *upperKey, *cp, *key, *value, *tok, *mediaType;
    ssize length, rxLimit;
    int   count, field, seen;

    assert(websValid(wp));

    seen = 0;

    /*
        Parse the header and create the Http header keyword variables
        We rewrite the header as we go for non-local requests.  NOTE: this
        modifies the header string directly and tokenizes each line with '\0'.
     */
    for (count = 0; wp->rxbuf.servp[0] != '\r'; count++) {
        if (count >= ME_GOAHEAD_LIMIT_NUM_HEADERS) {
            websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Too many headers");
            return;
        }
        key = getToken(wp, ":", TOKEN_HEADER_KEY);
        if (key == NULL || *key == '\0' || bufLen(&wp->rxbuf) == 0) {
            websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad header format");
            return;
        }
        value = getToken(wp, "\r\n", TOKEN_HEADER_VALUE);
        if (value == NULL || bufLen(&wp->rxbuf) == 0 || wp->rxbuf.servp[0] == '\0') {
            websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad header format");
            return;
        }
        slower(key);
        trace(3, "%s: %s", key, traceValue(key, value));
        if ((field = singleValuedHeader(key)) != 0) {
            if (seen & field) {
                websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Duplicate header");
                return;
            }
            seen |= field;
        }

        /*
            Create a header variable for each line in the header
         */
        if ((upperKey = headerVarName(key)) == 0) {
            trace(5, "No header variable for \"%s\"", key);
        } else if ((prior = websGetVar(wp, upperKey, 0)) != 0) {
            /*
                Join Cookie fields with semicolons, not commas.
             */
            combined = sfmt("%s%s%s", prior, smatch(key, "cookie") ? "; " : ", ", value);
            websSetVar(wp, upperKey, combined);
            wfree(combined);
        } else {
            websSetVar(wp, upperKey, value);
        }
        wfree(upperKey);

        /*
            Track the requesting agent (browser) type
         */
        if (strcmp(key, "user-agent") == 0) {
            wfreeset(wp->userAgent);
            wp->userAgent = sclone(value);

        } else if (scaselesscmp(key, "authorization") == 0) {
            wfreeset(wp->authType);
            wp->authType = sclone(value);
            ssplit(wp->authType, " \t", &tok);
            wfreeset(wp->authDetails);
            wp->authDetails = sclone(tok);
            slower(wp->authType);

        } else if (strcmp(key, "content-length") == 0) {
            /*
                Strict per RFC 9112 6.3.5: the field value must be a single run of digits. atoi() would accept a
                leading sign/space and a non-digit tail, which a strict downstream hop may frame differently.
             */
            if (*value == '\0' || (int) strspn(value, "0123456789") != (int) slen(value)) {
                websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Invalid content length");
                return;
            }
            /*
                Check before multiplying to prevent length truncation and request-framing ambiguity.
             */
            rxLimit = smatch(wp->method, "PUT") ? ME_GOAHEAD_LIMIT_PUT : postLimit;
            if ((length = parseLimitedNumber(value, value + slen(value), 10, rxLimit, &next)) < 0) {
                websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Too big");
                return;
            }
            wp->rxLen = length;
            /*
                Account for the declared request body for every method, including HEAD, so the body is
                drained before the keep-alive connection is reused. An undrained body is mis-parsed as
                the next request (CWE-444). The HEAD response is unaffected: websWriteHeaders() still
                emits headers only.
             */
            wp->rxRemaining = wp->rxLen;

        } else if (strcmp(key, "content-type") == 0) {
            wfreeset(wp->contentType);
            wp->contentType = sclone(value);
            mediaType = websParseContentType(value, NULL);
            if (smatch(mediaType, "application/x-www-form-urlencoded")) {
                wp->flags |= WEBS_FORM;
            } else if (smatch(mediaType, "application/json")) {
                wp->flags |= WEBS_JSON;
            } else if (smatch(mediaType, "multipart/form-data")) {
                wp->flags |= WEBS_UPLOAD;
            }
            wfree(mediaType);

        } else if (strcmp(key, "cookie") == 0) {
            /* Should be only one cookie header really with semicolon delimmited key/value pairs */
            wp->flags |= WEBS_COOKIE;
            if (wp->cookie) {
                char *old = wp->cookie;
                wp->cookie = sfmt("%s; %s", old, value);
                wfree(old);
            } else {
                wp->cookie = sclone(value);
            }

        } else if (strcmp(key, "host") == 0) {
            if (!validHost(value)) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad host header");
                return;
            }
            /*
                Validate Host but do not let it replace an absolute-target authority, in wp->host or in HTTP_HOST.
             */
            if (wp->flags & WEBS_ABSOLUTE_TARGET) {
                websSetVar(wp, "HTTP_HOST", wp->host);
            } else {
                wfreeset(wp->host);
                wp->host = sclone(value);
            }

        } else if (strcmp(key, "if-none-match") == 0) {
            //  A list-valued field: repeated fields are one list
            if (wp->ifNoneMatch) {
                char *old = wp->ifNoneMatch;
                wp->ifNoneMatch = sfmt("%s, %s", old, value);
                wfree(old);
            } else {
                wp->ifNoneMatch = sclone(value);
            }

        } else if (strcmp(key, "if-modified-since") == 0) {
            if ((cp = strchr(value, ';')) != NULL) {
                *cp = '\0';
            }
            /*
                Use strict date parsing so invalid If-Modified-Since values are ignored.
             */
            if (websParseHttpDate(&wp->since, value) < 0) {
                trace(5, "Ignoring invalid If-Modified-Since \"%s\"", value);
                wp->since = 0;
            }

            /*
                Yes Veronica, the HTTP spec does misspell Referrer
             */
        } else if (strcmp(key, "referer") == 0) {
            wfreeset(wp->referrer);
            wp->referrer = sclone(value);

        } else if (strcmp(key, "transfer-encoding") == 0) {
            if (!(wp->flags & WEBS_HTTP11)) {
                websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Transfer-Encoding requires HTTP/1.1");
                return;
            }
            /*
                GoAhead implements only the "chunked" transfer coding. Per RFC 9112 6.1, chunked must be the final
                coding; any other coding (or a list such as "gzip, chunked") is one we cannot decode -> 501. Never
                fall back to Content-Length framing on an unrecognised value: that is the TE.CL smuggling vector.
             */
            if (scaselessmatch(strim(value, " \t", WEBS_TRIM_BOTH), "chunked")) {
                wp->rxChunkState = WEBS_CHUNK_START;
                wp->rxRemaining = MAXINT;
            } else {
                websError(wp, HTTP_CODE_NOT_IMPLEMENTED | WEBS_CLOSE, "Unsupported transfer coding");
                return;
            }
        }
    }
    /*
        RFC 9112 6.1: a message with both Content-Length and Transfer-Encoding is invalid and must be rejected, as the
        two framings are how an attacker desyncs GoAhead from a front-end. Order-independent: both flags are set by
        the time the header loop ends regardless of which header came first.
     */
    if ((seen & SEEN_CONTENT_LENGTH) && (seen & SEEN_TRANSFER_ENCODING)) {
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Conflicting Content-Length and Transfer-Encoding");
        return;
    }
    /*
        RFC 9112 3.2: an HTTP/1.1 request must carry exactly one Host, even with an absolute-form target.
     */
    if ((wp->flags & WEBS_HTTP11) && !(seen & SEEN_HOST)) {
        websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Missing host header");
        return;
    }
    /*
        Combine Connection fields so any close token wins; remove hop-by-hop fields after all headers are read.
     */
    if ((connection = websGetVar(wp, "HTTP_CONNECTION", NULL)) != NULL) {
        if (hasToken(connection, "close")) {
            wp->flags &= ~WEBS_KEEP_ALIVE;
        } else if (hasToken(connection, "keep-alive")) {
            wp->flags |= WEBS_KEEP_ALIVE;
        }
        stripHopHeaders(wp, connection);
    }
    setForwardedScheme(wp);
    if (!wp->rxChunkState) {
        /*
            Step over "\r\n" after headers.
            Don't do this if chunked so that chunking can parse a single chunk delimiter of "\r\nSIZE ...\r\n"
         */
        if (bufLen(&wp->rxbuf) < 2 || wp->rxbuf.servp[0] != '\r' || wp->rxbuf.servp[1] != '\n') {
            websError(wp, HTTP_CODE_BAD_REQUEST | WEBS_CLOSE, "Bad header termination");
            return;
        }
        bufAdjustStart(&wp->rxbuf, 2);
    }
    wp->eof = (wp->rxRemaining == 0);
}


static bool processContent(Webs *wp)
{
    bool canProceed;

    canProceed = 1;
    if (!wp->eof) {
        canProceed = filterChunkData(wp);
        if (!canProceed || wp->finalized) {
            return canProceed;
        }
#if ME_GOAHEAD_UPLOAD
        if (wp->flags & WEBS_UPLOAD) {
            canProceed = websProcessUploadData(wp);
            if (!canProceed || wp->finalized) {
                return canProceed;
            }
        }
#endif
#if !ME_ROM
        if (wp->putfd >= 0) {
            canProceed = websProcessPutData(wp);
            if (!canProceed || wp->finalized) {
                return canProceed;
            }
        }
#endif
#if ME_GOAHEAD_CGI
        if (wp->cgifd >= 0) {
            canProceed = websProcessCgiData(wp);
            if (!canProceed || wp->finalized) {
                return canProceed;
            }
        }
#endif
    }
    if (wp->eof) {
        wp->state = WEBS_READY;
        /*
            Prevent reading content from the next request
            The handler may not have been created if all the content was read in the initial read. No matter.
         */
        socketDeleteHandler(wp->sid);
        canProceed = 1;
    }
    return canProceed;
}


/*
    Always called after data is consumed from the input buffer
 */
PUBLIC void websConsumeInput(Webs *wp, ssize nbytes)
{
    assert(wp);
    assert(nbytes >= 0);

    assert(bufLen(&wp->input) >= nbytes);
    if (nbytes <= 0) {
        return;
    }
    bufAdjustStart(&wp->input, nbytes);
    if (bufLen(&wp->input) == 0) {
        bufReset(&wp->input);
    }
}


static bool filterChunkData(Webs *wp)
{
    WebsBuf *rxbuf;
    ssize   chunkSize;
    cchar   *sp;
    char    *start, *cp;
    ssize   len, nbytes, limit, decoded;
    int     bad, digits;

    assert(wp);
    assert(wp->rxbuf.buf);
    rxbuf = &wp->rxbuf;
    /*
        Return progress after decoding available chunks so body consumers can drain without waiting for completion
        or another read event.
     */
    decoded = 0;

    while (bufLen(rxbuf) > 0) {
        //  Each state reads the received data as one flat block
        bufStart(rxbuf);
        switch (wp->rxChunkState) {
        case WEBS_CHUNK_UNCHUNKED:
            len = min(wp->rxRemaining, bufLen(rxbuf));
            //  A short put means the input buffer is at its maximum: refuse rather than truncate the body
            if (bufPutBlk(&wp->input, rxbuf->servp, len) != len) {
                websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Too big");
                return 1;
            }
            bufAddNull(&wp->input);
            bufAdjustStart(rxbuf, len);
            bufCompact(rxbuf);
            wp->rxRemaining -= len;
            if (wp->rxRemaining <= 0) {
                wp->eof = 1;
            }
            assert(wp->rxRemaining >= 0);
            return 1;

        case WEBS_CHUNK_START:
            /*
                Expect: "\r\nSIZE.*\r\n"
             */
            if (bufLen(rxbuf) < 5) {
                return decoded > 0;
            }
            start = rxbuf->servp;
            bad = (start[0] != '\r' || start[1] != '\n');
            /*
                Distinguish incomplete chunk headers from oversized ones without reading past buffered data.
             */
            for (cp = &start[2]; cp < rxbuf->endp && *cp != '\n'; cp++) { }
            if (cp >= rxbuf->endp) {
                if ((cp - start) < MAX_CHUNK_HEADER) {
                    /* Insufficient data */
                    return decoded > 0;
                }
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Chunk header too long");
                return 1;
            }
            if ((cp - start) >= MAX_CHUNK_HEADER) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Chunk header too long");
                return 1;
            }
            bad += (cp[-1] != '\r');
            if (bad) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 1;
            }
            /*
                Require hexadecimal chunk sizes followed by an optional semicolon extension (RFC 9112 7.1).
             */
            limit = putStaged(wp) ? ME_GOAHEAD_LIMIT_PUT : postLimit;
            chunkSize = parseLimitedNumber(&start[2], cp, 16, limit, &sp);
            digits = (int) (sp - &start[2]);
            /* Reject bare CR outside the grammatical chunk-size line ending. */
            if (digits == 0 || (sp != cp - 1 && (*sp != ';' || !validChunkExt(sp, cp - 1))) ||
                chunkSize < 0 || chunkSize > limit) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 1;
            }
            /*
                Enforce cumulative body limits before reading each chunk, using remaining capacity to avoid overflow.
             */
            if (chunkSize > limit - wp->rxBytes) {
                websError(wp, WEBS_CLOSE | HTTP_CODE_REQUEST_TOO_LARGE, "Request too big");
                return 1;
            }
            wp->rxBytes += chunkSize;
            if (chunkSize == 0) {
                /* On the last chunk, consume the final "\r\n" */
                if ((cp + 2) >= rxbuf->endp) {
                    /* Insufficient data */
                    return decoded > 0;
                }
                cp += 2;
                bad += (cp[-1] != '\r' || cp[0] != '\n');
                if (bad) {
                    websError(wp, WEBS_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad final chunk specification");
                    return 1;
                }
            }
            bufAdjustStart(rxbuf, cp - start + 1);
            wp->rxChunkSize = chunkSize;
            wp->rxRemaining = chunkSize;
            if (chunkSize == 0) {
                /*
                    The decoded body is complete, so publish the authoritative total. Chunked framing has no
                    Content-Length header, so rxLen would otherwise keep its -1 placeholder and be published to
                    the CGI gateway as CONTENT_LENGTH=-1 after the body had already been written to its stdin.
                 */
                wp->rxLen = wp->rxBytes;
                wp->eof = 1;
                return 1;
            }
            trace(7, "chunkFilter: start incoming chunk of %zd bytes", chunkSize);
            wp->rxChunkState = WEBS_CHUNK_DATA;
            break;

        case WEBS_CHUNK_DATA:
            len = min(bufLen(rxbuf), wp->rxRemaining);
            nbytes = min(bufRoom(&wp->input), len);
            if (len > 0 && (nbytes = bufPutBlk(&wp->input, rxbuf->servp, nbytes)) == 0) {
                websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Too big");
                return 1;
            }
            bufAddNull(&wp->input);
            bufAdjustStart(rxbuf, nbytes);
            wp->rxRemaining -= nbytes;
            decoded += nbytes;
            if (wp->rxRemaining <= 0) {
                wp->rxChunkState = WEBS_CHUNK_START;
                bufCompact(rxbuf);
            }
            break;
        }
    }
    return decoded > 0;
}


/*
    Basic event loop. SocketReady returns true when a socket is ready for service. SocketSelect will block until an
    event occurs. SocketProcess will actually do the servicing.
 */
PUBLIC void websServiceEvents(volatile int *finished)
{
    int delay, nextEvent;

    delay = 0;
    while (!finished || !*finished) {
        if (socketSelect(-1, delay)) {
            socketProcess();
        }
#if ME_GOAHEAD_CGI
        delay = websCgiPoll();
#else
        delay = MAXINT;
#endif
        nextEvent = websRunEvents();
        delay = min(delay, nextEvent);
    }
}


static bool reservedRequestVar(cchar *name)
{
    if (sncmp(name, "HTTP_", 5) == 0) {
        return 1;
    }
#if ME_GOAHEAD_UPLOAD
    if (websIsUploadVar(name)) {
        return 1;
    }
#endif
    return 0;
}


PUBLIC WebsKey *websSetRequestVar(Webs *wp, cchar *name, cchar *value, int origin)
{
    WebsKey *sp, *prior;
    int     mark;

    assert(wp);
    assert(name && *name);

    if (reservedRequestVar(name)) {
        trace(5, "Ignoring request parameter with a reserved name");
        return 0;
    }
    if ((prior = hashLookup(wp->vars, name)) != 0) {
        if (prior->arg == 0) {
            trace(5, "Ignoring request parameter named like a server variable");
            return 0;
        }
        mark = prior->arg == origin ? origin : WEBS_VAR_QUERY;
        sp = websSetVarFmt(wp, name, "%s %s", websGetVar(wp, name, ""), value);
    } else {
        mark = origin;
        sp = websSetVar(wp, name, value);
    }
    //  A non-zero origin also marks the variable as untrusted, which CGI prefixes
    sp->arg = mark;
    return sp;
}


/*
    Parse in place. Size repeated-name values first, then join with spaces in linear time.
 */
static int addFormVars(Webs *wp, char *vars, int origin)
{
    struct {
        char *name, *value;
        char *joined, *end;
        ssize len;
        int first;
    } *pairs, *pp, *fp;
    WebsKey  *sp;
    WebsHash names;
    char     *cp, *keyword, *value, *tok;
    ssize    len;
    int      count, n, i;

    assert(wp);
    assert(vars);

    /*
        Bound parameter count before insertion to limit quadratic hash-table work.
     */
    for (count = 1, cp = vars; *cp; cp++) {
        if (*cp == '&' && ++count > ME_GOAHEAD_LIMIT_PARAMS) {
            return -1;
        }
    }
    pairs = walloc(count * sizeof(*pairs));
    names = hashCreate(max(count, WEBS_SMALL_HASH));
    n = 0;
    keyword = stok(vars, "&", &tok);
    while (keyword != NULL && n < count) {
        if ((value = strchr(keyword, '=')) != NULL) {
            *value++ = '\0';
            websDecodeUrl(value, value, -1);
        } else {
            value = "";
        }
        //  A key is decoded whether or not it has a value, so "?caf%C3%A9" and "?caf%C3%A9=" name one variable
        websDecodeUrl(keyword, keyword, -1);
        if (*keyword) {
            pp = &pairs[n];
            pp->name = keyword;
            pp->value = value;
            if ((sp = hashLookup(names, keyword)) != 0) {
                pp->first = (int) sp->content.value.integer;
                pairs[pp->first].len += slen(value) + 1;
            } else {
                pp->first = n;
                pp->len = slen(value);
                hashEnter(names, keyword, valueInteger(n), 0);
            }
            n++;
        }
        keyword = stok(NULL, "&", &tok);
    }
    for (i = 0; i < n; i++) {
        pp = &pairs[i];
        fp = &pairs[pp->first];
        if (fp == pp) {
            fp->joined = fp->end = walloc(fp->len + 1);
        } else {
            *fp->end++ = ' ';
        }
        len = slen(pp->value);
        memcpy(fp->end, pp->value, len);
        fp->end += len;
    }
    for (i = 0; i < n; i++) {
        pp = &pairs[i];
        if (pp->first == i) {
            *pp->end = '\0';
            websSetRequestVar(wp, pp->name, pp->joined, origin);
            wfreeset(pp->joined);
        }
    }
    hashFree(names);
    wfree(pairs);
    return 0;
}


/*
    Set the variable (CGI) environment for this request. Create variables for all standard CGI variables. Also decode
    the query string and create a variable for each name=value pair.
 */
PUBLIC void websSetEnv(Webs *wp)
{
    assert(wp);
    assert(websValid(wp));

    //  Only a request authenticated on this route has an AUTH_TYPE; wp->username is set only by authentication
    websSetVar(wp, "AUTH_TYPE", wp->username ? wp->authType : 0);
    /*
        Never publish the -1 placeholder. A request with no body carries no Content-Length, and the canonical
        CGI idioms malloc(atoi(CONTENT_LENGTH) + 1) and read(0, buf, len) turn a negative length into a zero
        sized allocation and a read of SIZE_MAX bytes. Zero is the correct CGI/1.1 value for an absent body.
     */
    websSetVarFmt(wp, "CONTENT_LENGTH", "%zd", wp->rxLen > 0 ? wp->rxLen : 0);
    websSetVar(wp, "CONTENT_TYPE", wp->contentType);
    if (wp->route && wp->route->dir) {
        websSetVar(wp, "DOCUMENT_ROOT", wp->route->dir);
    }
    websSetVar(wp, "GATEWAY_INTERFACE", "CGI/1.1");
    websSetVar(wp, "PATH_INFO", wp->path);
    websSetVar(wp, "PATH_TRANSLATED", wp->filename);
    websSetVar(wp, "QUERY_STRING", wp->query);
    websSetVar(wp, "REMOTE_ADDR", wp->ipaddr);
    websSetVar(wp, "REMOTE_USER", wp->username);
    websSetVar(wp, "REMOTE_HOST", wp->ipaddr);
    websSetVar(wp, "REQUEST_METHOD", wp->method);
    websSetVar(wp, "REQUEST_TRANSPORT", wp->protocol);
    /*
        Keep REQUEST_URI encoded to prevent double decoding by CGI programs.
     */
    websSetVar(wp, "REQUEST_URI", websOriginForm(wp->url));
    websSetVar(wp, "SERVER_ADDR", wp->ifaddr);
    websSetVar(wp, "SERVER_HOST", websHost);
    websSetVar(wp, "SERVER_NAME", websHost);
    websSetVarFmt(wp, "SERVER_PORT", "%d", wp->port);
    websSetVar(wp, "SERVER_PROTOCOL", wp->protoVersion);
    websSetVar(wp, "SERVER_URL", websHostUrl);
    websSetVarFmt(wp, "SERVER_SOFTWARE", "GoAhead/%s", ME_VERSION);
}


PUBLIC int websSetFormVars(Webs *wp)
{
    char *data;
    int  rc;

    /*
        Test the decoded buffer rather than the declared length. A chunked form post has no
        Content-Length, so gating on rxLen skips form variable parsing for the whole request.
     */
    rc = 0;
    if (bufLen(&wp->input) > 0) {
        if (wp->flags & WEBS_FORM) {
            data = sclone(bufStart(&wp->input));
            rc = addFormVars(wp, data, WEBS_VAR_BODY);
            wfree(data);
        }
    }
    return rc;
}


PUBLIC int websSetQueryVars(Webs *wp)
{
    /*
        Decode and create an environment query variable for each query keyword. We split into pairs at each '&', then
        split pairs at the '='.  Note: we rely on wp->decodedQuery preserving the decoded values in the symbol table.
     */
    if (wp->query && *wp->query) {
        wfreeset(wp->decodedQuery);
        wp->decodedQuery = sclone(wp->query);
        return addFormVars(wp, wp->decodedQuery, WEBS_VAR_QUERY);
    }
    return 0;
}


/*
    Define a webs (CGI) variable for this connection. Also create in relevant scripting engines. Note: the incoming
    value may be volatile.
 */
PUBLIC WebsKey *websSetVarFmt(Webs *wp, cchar *var, cchar *fmt, ...)
{
    WebsValue v;
    va_list   args;

    assert(websValid(wp));
    assert(var && *var);

    if (fmt) {
        va_start(args, fmt);
        v = valueString(sfmtv(fmt, args), 0);
        v.allocated = 1;
        va_end(args);
    } else {
        v = valueString("", 0);
    }
    return hashEnter(wp->vars, var, v, 0);
}


PUBLIC WebsKey *websSetVar(Webs *wp, cchar *var, cchar *value)
{
    WebsValue v;

    assert(websValid(wp));
    assert(var && *var);

    if (value) {
        v = valueString(value, VALUE_ALLOCATE);
    } else {
        v = valueString("", 0);
    }
    return hashEnter(wp->vars, var, v, 0);
}


/*
    Return TRUE if a webs variable exists for this connection.
 */
PUBLIC bool websTestVar(Webs *wp, cchar *var)
{
    WebsKey *sp;

    assert(websValid(wp));
    assert(var && *var);

    if (var == NULL || *var == '\0') {
        return 0;
    }
    if ((sp = hashLookup(wp->vars, var)) == NULL) {
        return 0;
    }
    return 1;
}


/*
    Get a webs variable but return a default value if string not found.  Note, defaultGetValue can be NULL to permit
    testing existence.
 */
PUBLIC cchar *websGetVar(Webs *wp, cchar *var, cchar *defaultGetValue)
{
    WebsKey *sp;

    assert(websValid(wp));
    assert(var && *var);

    if ((sp = hashLookup(wp->vars, var)) != NULL) {
        assert(sp->content.type == string);
        if (sp->content.value.string) {
            return sp->content.value.string;
        } else {
            return "";
        }
    }
    return defaultGetValue;
}


/*
    Return TRUE if a webs variable is set to a given value
 */
PUBLIC int websCompareVar(Webs *wp, cchar *var, cchar *value)
{
    assert(websValid(wp));
    assert(var && *var);

    if (strcmp(value, websGetVar(wp, var, " __UNDEF__ ")) == 0) {
        return 1;
    }
    return 0;
}


/*
    Cancel the request timeout. Note may be called multiple times.
 */
PUBLIC void websCancelTimeout(Webs *wp)
{
    assert(websValid(wp));

    if (wp->timeout >= 0) {
        websStopEvent(wp->timeout);
        wp->timeout = -1;
    }
}


/*
    Output a HTTP response back to the browser. If redirect is set to a URL, the browser will be sent to this location.
 */
PUBLIC void websResponse(Webs *wp, int code, cchar *message)
{
    ssize len;

    assert(websValid(wp));
    /*
        Write at most one response per request.
     */
    if (wp->finalized) {
        trace(4, "Ignoring response %d for a request that is already complete", code & WEBS_CODE_MASK);
        return;
    }
    websSetStatus(wp, code);

    /*
        A HEAD response declares the length the GET would send. Its body is discarded by websWriteBlock.
     */
    if (message && *message) {
        len = slen(message);
        websWriteHeaders(wp, len + 2, 0);
        websWriteEndHeaders(wp);
        websWriteBlock(wp, message, len);
        websWriteBlock(wp, "\r\n", 2);
        /*
            After dispatch, drain output through writable events. Before dispatch or during close, write the
            response before ending the request.
         */
        websFlush(wp, wp->state < WEBS_READY || wp->connError);
    } else {
        websWriteHeaders(wp, 0, 0);
        websWriteEndHeaders(wp);
    }
    websDone(wp);
}


static char *makeUri(cchar *scheme, cchar *host, int port, cchar *path)
{
    bool secure;

    secure = smatch(scheme, "https");
    if (port <= 0) {
        port = secure ? defaultSslPort : defaultHttpPort;
    }
    /*
        With no listener, imply only the requested scheme's default port.
     */
    if (port <= 0 || port == (secure ? 443 : 80)) {
        return sfmt("%s://%s%s", scheme, host, path);
    }
    return sfmt("%s://%s:%d%s", scheme, host, port, path);
}


/*
    Redirect the user to another webs page
 */
PUBLIC void websRedirect(Webs *wp, cchar *uri)
{
    char  *encoded, *message, *location, *scheme, *host, *pstr;
    bool  secure, fullyQualified;
    ssize len;
    int   originalPort, port;

    assert(websValid(wp));
    assert(uri);
    if (wp->finalized) {
        trace(4, "Ignoring redirect to %s for a request that is already complete", uri);
        return;
    }
    message = location = NULL;
    originalPort = port = 0;

    if (websHostUrl) {
        host = sclone(websHostUrl);
    } else if (strchr(wp->ifaddr, ':')) {
        host = sfmt("[%s]", wp->ifaddr);
    } else {
        host = sclone(wp->ifaddr);
    }
    pstr = strchr(host, ']');
    pstr = pstr ? pstr : host;
    if ((pstr = strchr(pstr, ':')) != 0 && pstr[1] != ':') {
        *pstr++ = '\0';
        originalPort = atoi(pstr);
    }
    if (smatch(uri, "http://") || smatch(uri, "https://")) {
        /* Protocol switch with existing Uri */
        scheme = sncmp(uri, "https", 5) == 0 ? "https" : "http";
        uri = location = makeUri(scheme, host, 0, websOriginForm(wp->url));
    }
    /*
        Recognize schemes only at the start of the target.
     */
    secure = sstarts(uri, "https://");
    fullyQualified = secure || sstarts(uri, "http://");
    if (!fullyQualified) {
        port = originalPort;
        if (wp->flags & WEBS_HTTPS) {
            secure = 1;
        }
    }
    scheme = secure ? "https" : "http";
    if (port <= 0) {
        //  Behind a proxy the server's own listen ports are not the client's, so imply the scheme's default
        port = (wp->flags & WEBS_PROXIED) ? 0 : secure ? defaultSslPort : defaultHttpPort;
    }
    if (sstarts(uri, "https:///")) {
        /* Short-hand for redirect to https */
        uri = location = makeUri(scheme, host, port, &uri[8]);

    } else if (sstarts(uri, "http:///")) {
        uri = location = makeUri(scheme, host, port, &uri[7]);

    } else if (!fullyQualified) {
        uri = location = makeUri(scheme, host, port, uri);
    }
    /*
        Keep Location URI-encoded; HTML-escape it for the response body's link.
     */
    encoded = websEscapeHtml(uri);
    message =
        sfmt(
            "<html><head></head><body>\r\n\
        This document has moved to a new <a href=\"%s\">location</a>.\r\n\
        Please update your documents to reflect the new location.\r\n\
        </body></html>\r\n",
            encoded);
    len = slen(message);
    websSetStatus(wp, HTTP_CODE_MOVED_TEMPORARILY);
    websWriteHeaders(wp, len + 2, uri);
    websWriteHeader(wp, "Content-Type", "text/html");
    websWriteEndHeaders(wp);
    websWriteBlock(wp, message, len);
    websWriteBlock(wp, "\r\n", 2);
    websDone(wp);
    wfree(message);
    wfree(location);
    wfree(encoded);
    wfree(host);
}


PUBLIC int websRedirectByStatus(Webs *wp, int status)
{
    WebsKey *key;
    char    code[16], *uri;

    assert(wp);
    assert(status >= 0);

    /*
        Use the redirect for the status, else the wildcard (redirect=*@uri), which is stored as status 0. A status
        with neither is answered as it would be on a route without redirects, so the request always gets a
        response. Status 0 itself is the redirect handler's lookup, which reports no match to its caller.
     */
    if (wp->route && wp->route->redirects >= 0) {
        itosbuf(code, sizeof(code), status, 10);
        if ((key = hashLookup(wp->route->redirects, code)) == 0) {
            key = hashLookup(wp->route->redirects, "0");
        }
        if (key) {
            uri = key->content.value.string;
            websRedirect(wp, uri);
            return 0;
        }
        if (status == 0) {
            return -1;
        }
    }
    if (status == HTTP_CODE_UNAUTHORIZED) {
        websError(wp, status, "Access Denied. User not logged in.");
    } else {
        websError(wp, status, 0);
    }
    return 0;
}


/*
    Share entity definitions between output sizing and writing.
 */
static cchar *htmlEntity(uchar c)
{
    if (!(charMatch[c] & WEBS_ENCODE_HTML)) {
        return 0;
    }
    switch (c) {
    case '&':
        return "&amp;";
    case '<':
        return "&lt;";
    case '>':
        return "&gt;";
    case '"':
        return "&quot;";
    case '\'':
        return "&#39;";
    }
    return 0;
}


/*
    Escape HTML to escape defined characters (prevent cross-site scripting)
    Returns an allocated string.
 */
PUBLIC char *websEscapeHtml(cchar *html)
{
    cchar *ip, *entity;
    char  *result, *op;
    ssize len, elen;

    if (!html) {
        return sclone("");
    }
    for (len = 1, ip = html; *ip; ip++) {
        entity = htmlEntity((uchar) *ip);
        len += entity ? slen(entity) : 1;
    }
    if ((result = walloc(len)) == 0) {
        return 0;
    }
    for (op = result, ip = html; *ip; ip++) {
        if ((entity = htmlEntity((uchar) *ip)) != 0) {
            elen = slen(entity);
            memcpy(op, entity, (size_t) elen);
            op += elen;
        } else {
            *op++ = *ip;
        }
    }
    *op = '\0';
    return result;
}

/*
    URI-encode with the same predicate for sizing and writing.
 */
PUBLIC char *websEscapeUri(cchar *uri)
{
    static cchar hexTable[] = "0123456789ABCDEF";
    uchar        c;
    cchar        *ip;
    char         *result, *op;
    ssize        len;

    if (!uri) {
        return sclone("");
    }
    for (len = 1, ip = uri; *ip; ip++) {
        len += (charMatch[(uchar) *ip] & WEBS_ENCODE_URI) ? 3 : 1;
    }
    if ((result = walloc(len)) == 0) {
        return 0;
    }
    op = result;
    while ((c = (uchar) (*uri++)) != 0) {
        if (charMatch[c] & WEBS_ENCODE_URI) {
            *op++ = '%';
            *op++ = hexTable[c >> 4];
            *op++ = hexTable[c & 0xf];
        } else {
            *op++ = (char) c;
        }
    }
    *op = '\0';
    return result;
}


/*
    Escape one byte or a U+2028/U+2029 sequence. Return output length and set used to input bytes consumed.
    Write only when op is non-NULL.
 */
static ssize escapeJsUnit(cchar *ip, char *op, ssize *used)
{
    static cchar hexTable[] = "0123456789ABCDEF";
    uchar        c;

    c = (uchar) *ip;
    *used = 1;
    if (c == 0xE2 && (uchar) ip[1] == 0x80 && ((uchar) ip[2] == 0xA8 || (uchar) ip[2] == 0xA9)) {
        *used = 3;
        if (op) {
            memcpy(op, (uchar) ip[2] == 0xA8 ? "\\u2028" : "\\u2029", 6);
        }
        return 6;
    }
    if (c >= 0x80 || isalnum(c) || c == ' ' || c == ',' || c == '.' || c == '_' || c == '-') {
        if (op) {
            *op = (char) c;
        }
        return 1;
    }
    if (op) {
        op[0] = '\\';
        op[1] = 'u';
        op[2] = '0';
        op[3] = '0';
        op[4] = hexTable[c >> 4];
        op[5] = hexTable[c & 0xf];
    }
    return 6;
}


/*
    Escape for a JavaScript string literal with the same predicate for sizing and writing.
 */
PUBLIC char *websEscapeJs(cchar *str)
{
    cchar *ip;
    char  *result, *op;
    ssize len, used;

    if (!str) {
        return sclone("");
    }
    for (len = 1, ip = str; *ip; ip += used) {
        len += escapeJsUnit(ip, NULL, &used);
    }
    if ((result = walloc(len)) == 0) {
        return 0;
    }
    for (op = result, ip = str; *ip; ip += used) {
        op += escapeJsUnit(ip, op, &used);
    }
    *op = '\0';
    return result;
}


PUBLIC char *websEscapeControl(cchar *str, cchar *keep, cchar *escape)
{
    static cchar hexTable[] = "0123456789abcdef";
    cuchar       *ip;
    char         *result, *op;

    if (!str) {
        return sclone("");
    }
    result = walloc(slen(str) * 4 + 1);
    for (op = result, ip = (cuchar*) str; *ip; ip++) {
        if (((*ip < 0x20 || *ip == 0x7f) && !(keep && strchr(keep, *ip))) || (escape && strchr(escape, *ip))) {
            *op++ = '\\';
            *op++ = 'x';
            *op++ = hexTable[*ip >> 4];
            *op++ = hexTable[*ip & 0xf];
        } else {
            *op++ = (char) *ip;
        }
    }
    *op = '\0';
    return result;
}


/*
    A header name must be an RFC 9110 token, less '%', which in a header name is only an encoding artifact.
 */
PUBLIC bool websValidHeaderName(cchar *key)
{
    cchar *cp;

    if (key == 0 || *key == '\0') {
        return 0;
    }
    for (cp = key; *cp; cp++) {
        if (!isTchar((uchar) *cp) || *cp == '%') {
            return 0;
        }
    }
    return 1;
}


/*
    Reject header controls except tab to prevent response splitting.
 */
PUBLIC bool websValidHeaderValue(cchar *value)
{
    cchar *cp;

    if (value == 0) {
        return 0;
    }
    for (cp = value; *cp; cp++) {
        if (((uchar) *cp < 0x20 && *cp != '\t') || (uchar) *cp == 0x7F) {
            return 0;
        }
    }
    return 1;
}


/* Validate and write each header atomically; key NULL selects the status line. */
PUBLIC int websWriteHeader(Webs *wp, cchar *key, cchar *fmt, ...)
{
    va_list vargs;
    char    *line, *value;
    ssize   len;
    int     rc;

    assert(websValid(wp));

    line = 0;
    value = 0;
    rc = -1;

    if (!(wp->flags & WEBS_RESPONSE_TRACED)) {
        wp->flags |= WEBS_RESPONSE_TRACED;
        trace(3, ">>> Response");
    }
    if (fmt) {
        va_start(vargs, fmt);
        value = sfmtv(fmt, vargs);
        va_end(vargs);
        if (value == 0) {
            error("websWrite lost data, buffer overflow");
            goto done;
        }
    }
    if (key && !websValidHeaderName(key)) {
        error("Discarding a header with an invalid name");
        goto done;
    }
    if (value && !websValidHeaderValue(value)) {
        error("Discarding header %s with a control character in its value", key ? key : "");
        goto done;
    }
    if (key) {
        line = sfmt("%s: %s\r\n", key, value ? value : "");
    } else if (value) {
        line = sfmt("%s\r\n", value);
    } else {
        rc = 0;
        goto done;
    }
    len = slen(line);
    trace(3, "%s%s%s", key ? key : "", key ? ": " : "", traceValue(key, value ? value : ""));
    if (websWriteBlock(wp, line, len) != len) {
        goto done;
    }
    if (key && scaselessmatch(key, "Content-Type")) {
        /* Preserve the handler's Content-Type. */
        wp->flags |= WEBS_TYPE_WRITTEN;
    }
    rc = 0;

done:
    wfree(line);
    wfree(value);
    return rc;
}


PUBLIC void websSetStatus(Webs *wp, int code)
{
    wp->code = (code & WEBS_CODE_MASK);
    if (code & WEBS_CLOSE) {
        wp->flags &= ~WEBS_KEEP_ALIVE;
    }
}


/*
    A 1XX, 204 or 304 response carries no body. Shared by the header writer below and by handlers
    forwarding a body they did not generate, so the two cannot drift: a site emitting no
    Content-Length while another writes the body produces an empty-framed response followed by
    unframed bytes.
 */
PUBLIC bool websStatusHasNoBody(int code)
{
    return (100 <= code && code <= 199) || code == HTTP_CODE_NO_CONTENT || code == HTTP_CODE_NOT_MODIFIED;
}


/*
    HEAD, 1xx, 204, and 304 responses carry no body, regardless of handler.
 */
static bool responseHasBody(Webs *wp)
{
    return !smatch(wp->method, "HEAD") && !websStatusHasNoBody(wp->code);
}


/*
    Write a set of headers. Does not write the trailing blank line so callers can add more headers.
    Set length to -1 if unknown and transfer-chunk-encoding will be employed.
 */
PUBLIC void websWriteHeaders(Webs *wp, Offset length, cchar *location)
{
    WebsKey *cookie, *key, *next;
    char    *date, *protoVersion;
    char    lenbuf[32];
    int     cacheable;

    assert(websValid(wp));

    wp->flags &= ~WEBS_JST_HEADERS;

    if (!(wp->flags & WEBS_HEADERS_CREATED)) {
        protoVersion = wp->protoVersion;
        if (!protoVersion) {
            protoVersion = "HTTP/1.0";
            wp->flags &= ~WEBS_KEEP_ALIVE;
        }
        /*
            A connection whose request body is not fully read is not reused (see complete), so do not promise it.
         */
        if (!wp->eof) {
            wp->flags &= ~WEBS_KEEP_ALIVE;
        }
        websWriteHeader(wp, NULL, "%s %d %s", protoVersion, wp->code, websErrorMsg(wp->code));
#if !ME_GOAHEAD_STEALTH
        websWriteHeader(wp, "Server", "GoAhead-http");
#endif
        if ((date = websGetDateString(NULL)) != NULL) {
            websWriteHeader(wp, "Date", "%s", date);
            wfree(date);
        }
        if (wp->authResponse) {
            websWriteHeader(wp, "WWW-Authenticate", "%s", wp->authResponse);
        }
        if (length >= 0) {
            /*
                Emit the full width. wp->txLen below keeps the untruncated value and governs the body, so
                truncating here makes the header disagree with the bytes actually written and splits the
                response on a keep-alive connection. Convert via itosbuf rather than a format specifier:
                fmt() has no %z, and its %Ld extension conflicts with the printf attribute on
                websWriteHeader, under which L means long double.
             */
            itosbuf(lenbuf, sizeof(lenbuf), (int64) length, 10);
            /*
                Ordinary HEAD responses declare the equivalent GET length. The status-based
                suppression for 1XX, 204 and 304 applies to every method.
             */
            if (!websStatusHasNoBody(wp->code)) {
                websWriteHeader(wp, "Content-Length", "%s", lenbuf);
            }
        }
        /*
            Use close-delimited bodies for unknown-length HTTP/1.0 responses; chunking requires HTTP/1.1.
         */
        wp->txLen = length;
        if (wp->txLen < 0 && !(wp->flags & WEBS_HTTP11) && responseHasBody(wp)) {
            wp->flags &= ~WEBS_KEEP_ALIVE;
        }
        /*
            A HEAD response keeps the Transfer-Encoding the equivalent GET would send. A 1xx, 204 or 304 has no body
            to frame.
         */
        if (wp->txLen < 0 && (wp->flags & WEBS_HTTP11) && !websStatusHasNoBody(wp->code)) {
            websWriteHeader(wp, "Transfer-Encoding", "chunked");
        }
        if (wp->flags & WEBS_KEEP_ALIVE) {
            websWriteHeader(wp, "Connection", "keep-alive");
        } else {
            websWriteHeader(wp, "Connection", "close");
        }
        if (location) {
            websWriteHeader(wp, "Location", "%s", location);
        }
        /*
            Prevent browsers from interpreting content as another type.
         */
        websWriteHeader(wp, "X-Content-Type-Options", "nosniff");
        /*
            Public caching requires WEBS_CACHEABLE, 200/304, GET/HEAD, and no credentials, session, or cookies.
            Use hashFirst: responseCookies index zero is valid.
         */
        cacheable = (wp->flags & WEBS_CACHEABLE) && !(wp->flags & WEBS_NO_CACHE) &&
                    (smatch(wp->method, "GET") || smatch(wp->method, "HEAD")) &&
                    (wp->code == HTTP_CODE_OK || wp->code == HTTP_CODE_NOT_MODIFIED) &&
                    !wp->authType && !wp->username && !wp->session && hashFirst(wp->responseCookies) == 0;

        for (cookie = hashFirst(wp->responseCookies); cookie; cookie = next) {
            websWriteHeader(wp, "Set-Cookie", "%s", cookie->content.value.string);
            next = hashNext(wp->responseCookies, cookie);
        }
#if defined(ME_GOAHEAD_CLIENT_CACHE)
        if (cacheable && wp->ext) {
            /*
                Comma-delimit both the list and the extension so only whole entries match. A bare "ext," matched
                against the raw list publicly caches any extension that is a tail of a listed one.
             */
            char *etok = sfmt(",%s,", &wp->ext[1]);
            if (strstr("," ME_GOAHEAD_CLIENT_CACHE ",", etok)) {
                websWriteHeader(wp, "Cache-Control", "public, max-age=%d", ME_GOAHEAD_CLIENT_CACHE_LIFESPAN);
            }
            wfree(etok);
        }
#endif
        if (!cacheable) {
            /*
                Not inside the CLIENT_CACHE guard: a build without client caching still needs no-store on a 401,
                because the poisoning cache is the intermediary's, not ours.
             */
            websWriteHeader(wp, "Cache-Control", "no-store, private");
            websWriteHeader(wp, "Vary", "Authorization, Cookie");
        }
#ifdef ME_GOAHEAD_XFRAME_HEADER
        if (*ME_GOAHEAD_XFRAME_HEADER) {
            websWriteHeader(wp, "X-Frame-Options", "%s", ME_GOAHEAD_XFRAME_HEADER);
        }
#endif
        if (*ME_GOAHEAD_HSTS && (wp->flags & WEBS_HTTPS)) {
            websWriteHeader(wp, "Strict-Transport-Security", "%s", ME_GOAHEAD_HSTS);
        }
        if (responseHeaders >= 0) {
            for (key = hashFirst(responseHeaders); key; key = hashNext(responseHeaders, key)) {
                websWriteHeader(wp, key->name.value.string, "%s", key->content.value.string);
            }
        }
    }
}


PUBLIC void websAddResponseHeader(cchar *key, cchar *value)
{
    if (key == 0 || *key == '\0') {
        return;
    }
    if (value == 0) {
        if (responseHeaders >= 0) {
            hashDelete(responseHeaders, key);
        }
        return;
    }
    if (responseHeaders < 0) {
        responseHeaders = hashCreate(-1);
    }
    hashEnter(responseHeaders, key, valueString(value, VALUE_ALLOCATE), 0);
}


static void addResponseHeaders(cchar *list)
{
    char *buf, *line, *value, *tok;

    buf = sclone(list);
    for (line = stok(buf, "\n", &tok); line; line = stok(NULL, "\n", &tok)) {
        if ((value = strchr(line, ':')) == 0) {
            error("Ignoring response header \"%s\" without a value", line);
            continue;
        }
        *value++ = '\0';
        websAddResponseHeader(strim(line, " \t\r", WEBS_TRIM_BOTH), strim(value, " \t\r", WEBS_TRIM_BOTH));
    }
    wfree(buf);
}


PUBLIC void websWriteEndHeaders(Webs *wp)
{
    WebsKey *key;

    assert(wp);
    /*
        Supply Content-Type only if the handler has not. Default unknown types to application/octet-stream to
        prevent HTML sniffing.
     */
    if (!(wp->flags & WEBS_TYPE_WRITTEN)) {
        if (wp->error) {
            websWriteHeader(wp, "Content-Type", "text/html");
        } else if ((key = hashLookup(websMime, wp->ext)) != 0) {
            websWriteHeader(wp, "Content-Type", "%s", key->content.value.string);
        } else if (wp->txLen != 0 && !websStatusHasNoBody(wp->code)) {
            websWriteHeader(wp, "Content-Type", "application/octet-stream");
        }
    }
    /*
        By omitting the "\r\n" delimiter after the headers, chunks can emit "\r\nSize\r\n" as a single chunk delimiter
     */
    /*
        Keep body framing consistent with the HTTP version and Transfer-Encoding header.
     */
    if (wp->txLen >= 0 || !(wp->flags & WEBS_HTTP11) || !responseHasBody(wp)) {
        websWriteBlock(wp, "\r\n", 2);
    }
    wp->flags |= WEBS_HEADERS_CREATED;
    if (wp->txLen < 0 && (wp->flags & WEBS_HTTP11) && responseHasBody(wp)) {
        wp->flags |= WEBS_CHUNKING;
    }
}


PUBLIC void websSetTxLength(Webs *wp, Offset length)
{
    assert(wp);
    wp->txLen = length;
}


/*
    Do formatted output to the browser. This is the public Javascript and form write procedure.
 */
PUBLIC ssize websWrite(Webs *wp, cchar *fmt, ...)
{
    va_list vargs;
    char    *buf;
    ssize   rc;

    assert(websValid(wp));
    assert(fmt && *fmt);

    va_start(vargs, fmt);

    buf = NULL;
    rc = 0;
    if ((buf = sfmtv(fmt, vargs)) == 0) {
        error("websWrite lost data, buffer overflow");
    }
    va_end(vargs);
    assert(buf);
    if (buf) {
        rc = websWriteBlock(wp, buf, strlen(buf));
        wfree(buf);
    }
    return rc;
}


/*
    Non-blocking write to socket.
    Returns number of bytes written. Returns -1 on errors. May return short.
 */
PUBLIC ssize websWriteSocket(Webs *wp, cchar *buf, ssize size)
{
    ssize written;

    assert(wp);
    assert(buf);
    assert(size >= 0);

    if (wp->flags & WEBS_CLOSED) {
        return -1;
    }
#if ME_COM_SSL
    if (wp->flags & WEBS_SECURE) {
        if ((written = sslWrite(wp, (void*) buf, size)) < 0) {
            return written;
        }
    } else
#endif
    if ((written = socketWrite(wp->sid, (void*) buf, size)) < 0) {
        return written;
    }
    wp->written += written;
    websNoteRequestActivity(wp);
    return written;
}


/*
    Write some output using transfer chunk encoding if required.
    Returns true if all the data was written. Otherwise return zero.
 */
static bool flushChunkData(Webs *wp)
{
    ssize len, written, room;

    assert(wp);

    while (bufLen(&wp->chunkbuf) > 0) {
        /*
            Stop if there is not room for a reasonable size chunk.
            Subtract 16 to allow for the final trailer.
         */
        if ((room = bufRoom(&wp->output) - 16) <= CHUNK_LOW) {
            bufGrow(&wp->output, CHUNK_LOW - room + 1);
            if ((room = bufRoom(&wp->output) - 16) <= CHUNK_LOW) {
                return 0;
            }
        }
        switch (wp->txChunkState) {
        default:
        case WEBS_CHUNK_START:
            /* Select the chunk size so that both the prefix and data will fit */
            wp->txChunkLen = min(bufLen(&wp->chunkbuf), room - 16);
            fmt(wp->txChunkPrefix, sizeof(wp->txChunkPrefix), "\r\n%zx\r\n", wp->txChunkLen);
            wp->txChunkPrefixLen = slen(wp->txChunkPrefix);
            wp->txChunkPrefixNext = wp->txChunkPrefix;
            wp->txChunkState = WEBS_CHUNK_HEADER;
            break;

        case WEBS_CHUNK_HEADER:
            if ((written = bufPutBlk(&wp->output, wp->txChunkPrefixNext, wp->txChunkPrefixLen)) < 0) {
                return 0;
            } else {
                wp->txChunkPrefixNext += written;
                wp->txChunkPrefixLen -= written;
                if (wp->txChunkPrefixLen <= 0) {
                    wp->txChunkState = WEBS_CHUNK_DATA;
                } else {
                    return 0;
                }
            }
            break;

        case WEBS_CHUNK_DATA:
            if (wp->txChunkLen > 0) {
                //  A contiguous block at a time: the ring's data can wrap past the end of its allocation
                len = min(min(room, wp->txChunkLen), bufGetBlkMax(&wp->chunkbuf));
                if ((written = bufPutBlk(&wp->output, wp->chunkbuf.servp, len)) != len) {
                    assert(0);
                    return -1;
                }
                bufAdjustStart(&wp->chunkbuf, written);
                wp->txChunkLen -= written;
                if (wp->txChunkLen <= 0) {
                    wp->txChunkState = WEBS_CHUNK_START;
                    bufCompact(&wp->chunkbuf);
                }
                bufAddNull(&wp->output);
            }
        }
    }
    return bufLen(&wp->chunkbuf) == 0;
}


/*
    Initiate flushing output buffer. Returns true if all data is written to the socket and the buffer is empty.
    Returns <  0 for errors
            == 0 if there is output remaining to be flushed
            == 1 if the output was fully written to the socket
 */
PUBLIC int websFlush(Webs *wp, bool block)
{
    WebsBuf  *op;
    WebsTime deadline, now;
    ssize    nbytes, written;
    int      errCode, wasBlocking, bounded;

    wasBlocking = 0;
    bounded = 0;
    deadline = 0;
    if (block) {
        wasBlocking = socketSetBlock(wp->sid, 1);
        /*
            Bound all blocking flushes of one response by a shared deadline.
            Use short socket timeouts so slow readers cannot extend it indefinitely.
         */
        bounded = socketSetSendTimeout(wp->sid, FLUSH_SLICE) == 0;
        if (wp->flushDeadline == 0 && (now = websTime()) >= 0) {
            wp->flushDeadline = now + ME_GOAHEAD_LIMIT_TIMEOUT;
        }
        deadline = wp->flushDeadline;
    }
    op = &wp->output;
    if (wp->flags & WEBS_CHUNKING) {
        trace(6, "websFlush chunking finalized %d", wp->finalized);
        if (flushChunkData(wp) && wp->finalized) {
            trace(6, "websFlush: write chunk trailer");
            bufPutStr(op, "\r\n0\r\n\r\n");
            bufAddNull(op);
            wp->flags &= ~WEBS_CHUNKING;
        }
    }
    trace(6, "websFlush: buflen %zd", bufLen(op));
    written = 0;
    //  Written a contiguous block at a time: the ring's data can wrap past the end of its allocation
    while ((nbytes = bufGetBlkMax(op)) > 0) {
        /*
            Test the deadline on every pass, not just after a send timeout. A peer that accepts a
            few bytes per slice makes progress without ever timing out a send and would otherwise
            hold the event loop for as long as it cared to trickle.
         */
        if (bounded && ((now = websTime()) < 0 || now >= deadline)) {
            trace(2, "websFlush: peer not reading, close connection");
            written = -1;
        } else if ((written = websWriteSocket(wp, op->servp, nbytes)) < 0) {
            errCode = socketGetError(wp->sid);
            if (errCode == EWOULDBLOCK || errCode == EAGAIN || errCode == ETIMEDOUT) {
                if (!bounded) {
                    /* Not an error. Without a send timeout the wait cannot be bounded here */
                    written = 0;
                    break;
                }
                /* The send slice expired but the flush deadline has not. Retry */
                continue;
            }
        }
        if (written < 0) {
            /*
                Connection Error
             */
            wp->flags &= ~WEBS_KEEP_ALIVE;
            bufFlush(op);
            wp->state = WEBS_COMPLETE;
            break;
        } else if (written == 0) {
            break;
        }
        trace(6, "websFlush: wrote %zd to socket", written);
        bufAdjustStart(op, written);
        bufCompact(op);
    }
    assert(websValid(wp));

    if (bufLen(op) == 0 && wp->finalized) {
        wp->state = WEBS_COMPLETE;
    }
    if (block) {
        if (bounded) {
            socketSetSendTimeout(wp->sid, 0);
        }
        socketSetBlock(wp->sid, wasBlocking);
    }
    if (written < 0) {
        /* I/O Error */
        return -1;
    }
    return bufLen(op) == 0;
}


/*
    Respond to a writable event. First write any tx buffer by calling websFlush.
    Then write body data if writeProc is defined. If all written, ensure transition to complete state.
    Calls websPump() to advance state.
 */
static void writeEvent(Webs *wp)
{
    WebsBuf *op;

    op = &wp->output;
    if (bufLen(op) > 0) {
        websFlush(wp, 0);
    }
    if (bufLen(op) == 0 && wp->writeData) {
        (wp->writeData)(wp);
    }
    if (wp->state != WEBS_RUNNING) {
        websPump(wp);
    }
}


PUBLIC void websSetBackgroundWriter(Webs *wp, WebsWriteProc proc)
{
    WebsSocket *sp;
    WebsBuf    *op;

    assert(proc);

    wp->writeData = proc;
    op = &wp->output;

    if (bufLen(op) > 0) {
        websFlush(wp, 0);
    }
    if (bufLen(op) == 0) {
        (wp->writeData)(wp);
    }
    if (wp->sid >= 0 && wp->state < WEBS_COMPLETE) {
        if ((sp = socketPtr(wp->sid)) != NULL) {
            socketCreateHandler(wp->sid, sp->handlerMask | SOCKET_WRITABLE, socketEvent, wp);
        }
    }
}


/*
    Write a block of data of length to the user's browser. Output is buffered and flushed via websFlush.
    This routine will never return "short". i.e. it will return the requested size to write or -1.
    Buffer data. Will flush as required. May return -1 on write errors.
 */
PUBLIC ssize websWriteBlock(Webs *wp, cchar *buf, ssize size)
{
    WebsBuf *op;
    ssize   written, thisWrite, len, room;

    assert(wp);
    assert(websValid(wp));
    assert(buf);
    assert(size >= 0);

    if (wp->state >= WEBS_COMPLETE) {
        return -1;
    }
    if (size > 0) {
        writeJstHeaders(wp);
    }
    if ((wp->flags & WEBS_HEADERS_CREATED) && !responseHasBody(wp)) {
        //  Discard body bytes the response may not carry. The caller sees them written.
        return size;
    }
    op = (wp->flags & WEBS_CHUNKING) ? &wp->chunkbuf : &wp->output;
    written = len = 0;

    while (size > 0 && wp->state < WEBS_COMPLETE) {
        if (bufRoom(op) < size) {
            /*
                This will do a blocking I/O write. Will only ever fail for I/O errors.
             */
            if (websFlush(wp, 1) < 0) {
                return -1;
            }
        }
        if ((room = bufRoom(op)) == 0) {
            break;
        }
        thisWrite = min(room, size);
        bufPutBlk(op, buf, thisWrite);
        size -= thisWrite;
        buf += thisWrite;
        written += thisWrite;
    }
    bufAddNull(op);
    if (wp->state >= WEBS_COMPLETE && written == 0) {
        return -1;
    }
    return written;
}


/*
    Decode in place if needed. Treat + as space only for form/query grammar.
 */
static void decodeUrl(char *decoded, char *input, ssize len, bool plusIsSpace)
{
    char *ip,  *op;
    int  num, i, c;

    assert(decoded);
    assert(input);

    if (!decoded) {
        return;
    }
    if (!input) {
        *decoded = '\0';
        return;
    }
    /*
        len is the size of the decoded buffer, terminator included. -1 decodes in place, where the
        buffer is the input.
     */
    if (len < 0) {
        len = slen(input) + 1;
    }
    if (len == 0) {
        return;
    }
    op = decoded;
    for (ip = input; *ip && len > 1; ip++, op++) {
        if (*ip == '+' && plusIsSpace) {
            *op = ' ';
        } else if (*ip == '%' && isxdigit((uchar) ip[1]) && isxdigit((uchar) ip[2]) &&
                   !(ip[1] == '0' && ip[2] == '0')) {
            /*
                Convert %nn to a single character
             */
            ip++;
            for (i = 0, num = 0; i < 2; i++, ip++) {
                c = tolower((uchar) *ip);
                if (c >= 'a' && c <= 'f') {
                    num = (num * 16) + 10 + c - 'a';
                } else {
                    num = (num * 16) + c - '0';
                }
            }
            *op = (char) num;
            ip--;

        } else {
            *op = *ip;
        }
        len--;
    }
    *op = '\0';
}


PUBLIC void websDecodeUrl(char *decoded, char *input, ssize len)
{
    decodeUrl(decoded, input, len, 1);
}


#if ME_GOAHEAD_ACCESS_LOG && !ME_ROM
/*
    Escape log controls, quotes, and backslashes as hex to prevent forged entries. Caller frees.
 */
static char *logField(cchar *field)
{
    if (field == 0 || *field == '\0') {
        return sclone("-");
    }
    return websEscapeControl(field, NULL, "\"\\");
}


/*
    Output a log message in Common Log Format: See http://httpd.apache.org/docs/1.3/logs.html#common
 */
static void logRequest(Webs *wp, int code)
{
    char      *buf, *method, *path, *protocol, *username, timeStr[28], zoneStr[6], dataStr[16];
    ssize     len;
    time_t    timer;
    struct tm localt;

#if WINDOWS
    DWORD                 dwRet;
    TIME_ZONE_INFORMATION tzi;
#endif

    assert(wp);
    timer = toTimeT(websTime());
#if WINDOWS
    localtime_s(&localt, &timer);
#else
    localtime_r(&timer, &localt);
#endif
    strftime(timeStr, sizeof(timeStr), "%d/%b/%Y:%H:%M:%S", &localt);
    timeStr[sizeof(timeStr) - 1] = '\0';
#if WINDOWS
    dwRet = GetTimeZoneInformation(&tzi);
    fmt(zoneStr, sizeof(zoneStr), "%+03d00", -(int) (tzi.Bias / 60));
#elif !VXWORKS
    fmt(zoneStr, sizeof(zoneStr), "%+03d00", (int) (localt.tm_gmtoff / 3600));
#else
    zoneStr[0] = '\0';
#endif
    zoneStr[sizeof(zoneStr) - 1] = '\0';
    if (wp->written != 0) {
        fmt(dataStr, sizeof(dataStr), "%ld", (long) wp->written);
        dataStr[sizeof(dataStr) - 1] = '\0';
    } else {
        dataStr[0] = '-'; dataStr[1] = '\0';
    }
    username = logField(wp->username);
    method = logField(wp->method);
    path = logField(wp->path);
    protocol = logField(wp->protoVersion);
    buf = sfmt("%s - %s [%s %s] \"%s %s %s\" %d %s\n",
               wp->ipaddr, username, timeStr, zoneStr, method, path, protocol, code, dataStr);
    len = slen(buf);
    websWriteLogFile(&accessFile, buf, len);
    wfree(buf);
    wfree(username);
    wfree(method);
    wfree(path);
    wfree(protocol);
}
#endif


/*
    Request and connection timeout. The timeout triggers if we have not read any data from the
    users browser in the last WEBS_TIMEOUT period. If we have heard from the browser, simply
    re-issue the timeout.
 */
static void checkTimeout(void *arg, int id)
{
    Webs     *wp;
    int64    elapsed, sinceStart, delay;
    WebsTime now;
    bool     parsing, receiving, parseExpired, requestExpired, clockFailed;

    wp = (Webs*) arg;
    assert(websValid(wp));

    /*
        Reset timeout marks after a backward wall-clock step. A failed clock cannot time a request, so close it.
     */
    now = websTime();
    clockFailed = now < 0;
    if (!clockFailed) {
        if (wp->timestamp > now) {
            wp->timestamp = now;
        }
        if (wp->started > now) {
            wp->started = now;
        }
    }
    elapsed = getTimeSinceMark(wp) * 1000;
    if (websDebug) {
        websRestartEvent(id, (int) WEBS_TIMEOUT);
        return;
    }
    /*
        Enforce header and whole-request deadlines independently of the renewable idle timeout.
     */
    sinceStart = (wp->started && !clockFailed) ? (int64) (now - wp->started) * 1000 : 0;
    parsing = wp->started != 0 && wp->state == WEBS_BEGIN;
    receiving = wp->started != 0 && wp->state < WEBS_READY && REQUEST_TIMEOUT > 0;
    parseExpired = parsing && sinceStart >= PARSE_TIMEOUT;
    requestExpired = receiving && sinceStart >= REQUEST_TIMEOUT;

    if (elapsed >= WEBS_TIMEOUT || parseExpired || requestExpired || clockFailed) {
        if (!(wp->flags & WEBS_HEADERS_CREATED)) {
            if (parseExpired && elapsed < WEBS_TIMEOUT) {
                websError(wp, HTTP_CODE_REQUEST_TIMEOUT, "Request header timeout");
            } else if (requestExpired && elapsed < WEBS_TIMEOUT) {
                websError(wp, HTTP_CODE_REQUEST_TIMEOUT, "Request body timeout");
            } else if (wp->state > WEBS_BEGIN || wp->started || bufLen(&wp->rxbuf) > 0) {
                websError(wp, HTTP_CODE_REQUEST_TIMEOUT, "Request exceeded timeout");
            } else {
                /* No request has begun, so a response could be read as the answer to the next one */
                trace(5, "Idle connection closed");
            }
        }
        wp->state = WEBS_COMPLETE;
        complete(wp, 0);
        websFree(wp);
        /* WARNING: wp not valid here */
        return;
    }
    /*
        Schedule the nearest deadline to avoid late timeout detection.
     */
    delay = WEBS_TIMEOUT - elapsed;
    if (parsing && (PARSE_TIMEOUT - sinceStart) < delay) {
        delay = PARSE_TIMEOUT - sinceStart;
    }
    if (receiving && (REQUEST_TIMEOUT - sinceStart) < delay) {
        delay = REQUEST_TIMEOUT - sinceStart;
    }
    if (delay <= 0) {
        delay = 1;
    }
    websRestartEvent(id, (int) delay);
}


/*
    Use loopback for redirects and SERVER_NAME when hostname resolution fails.
 */
static void setLocalHost(void)
{
    struct in_addr intaddr;
    char           host[128], *ipaddr;

    websSetIpAddr("127.0.0.1");
    websSetHost("127.0.0.1");
    if (gethostname(host, sizeof(host)) < 0) {
        error("Cannot get hostname: errno %d - using 127.0.0.1", errno);
        return;
    }
#if VXWORKS
    {
        int hostAddr;

        if ((hostAddr = hostGetByName(host)) == ERROR) {
            error("Cannot get host address for host %s - using 127.0.0.1", host);
            return;
        }
        intaddr.s_addr = (ulong) hostAddr;
    }
    ipaddr = inet_ntoa(intaddr);
    websSetIpAddr(ipaddr);
    websSetHost(ipaddr);
    #if _WRS_VXWORKS_MAJOR < 6
    free(ipaddr);
    #endif
#elif ECOS
    ipaddr = inet_ntoa(eth0_bootp_data.bp_yiaddr);
    websSetIpAddr(ipaddr);
    websSetHost(ipaddr);
#elif TIDSP
    {
        struct hostent *hp;
        if ((hp = gethostbyname(host)) == NULL) {
            error("Cannot get host address for host %s: errno %d - using 127.0.0.1", host, errno);
            return;
        }
        /*
            Copy only IPv4 bytes into intaddr.
         */
        if (hp->h_addrtype != AF_INET || hp->h_length != (int) sizeof(intaddr)) {
            error("Host %s does not have an IPv4 address - using 127.0.0.1", host);
            return;
        }
        memcpy((char*) &intaddr, (char*) hp->h_addr_list[0], sizeof(intaddr));
        ipaddr = inet_ntoa(intaddr);
        websSetIpAddr(ipaddr);
        websSetHost(ipaddr);
    }
#else
    /*
        Use getaddrinfo for thread-safe address resolution.
     */
    {
        struct addrinfo    hints, *res;
        struct sockaddr_in *sa;
        int                rc;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        res = 0;
        rc = getaddrinfo(host, NULL, &hints, &res);
#if MACOSX
        if (rc != 0) {
            /*
                Bare hostnames are commonly absent from DNS on macOS while the mDNS ".local" form
                resolves. sfmt allocates, so keep the name to free rather than passing it inline.
             */
            char *mdnsName = sfmt("%s.local", host);
            rc = getaddrinfo(mdnsName, NULL, &hints, &res);
            wfree(mdnsName);
        }
#endif
        if (rc != 0 || res == NULL) {
            error("Cannot get host address for host %s: %s - using 127.0.0.1", host, gai_strerror(rc));
            if (res) {
                freeaddrinfo(res);
            }
            return;
        }
        sa = (struct sockaddr_in*) (void*) res->ai_addr;
        intaddr = sa->sin_addr;
        ipaddr = inet_ntoa(intaddr);
        websSetIpAddr(ipaddr);
        websSetHost(ipaddr);
        freeaddrinfo(res);
    }
#endif
}


PUBLIC void websSetHost(cchar *host)
{
    scopy(websHost, sizeof(websHost), host);
}


PUBLIC void websSetHostUrl(cchar *url)
{
    assert(url && *url);

    wfree(websHostUrl);
    websHostUrl = sclone(url);
}


PUBLIC void websSetIpAddr(cchar *ipaddr)
{
    assert(ipaddr && *ipaddr);
    scopy(websIpAddr, sizeof(websIpAddr), ipaddr);
}


#if ME_GOAHEAD_LEGACY
PUBLIC void websSetRequestFilename(Webs *wp, cchar *filename)
{
    assert(websValid(wp));
    assert(filename && *filename);

    wfreeset(wp->filename);
    wp->filename = sclone(filename);
    websSetVar(wp, "PATH_TRANSLATED", wp->filename);
}
#endif


PUBLIC int websRewriteRequest(Webs *wp, cchar *url)
{
    char *buf, *path, *query, *validated, *ext, *newUrl;

    if (websUrlParse(url, &buf, NULL, NULL, NULL, &path, NULL, NULL, &query) < 0) {
        return -1;
    }
    /*
        Validate and normalize the rewrite before changing request state; recompute its extension.
     */
    if ((validated = websValidateUriPath(path)) == 0) {
        wfree(buf);
        return -1;
    }
    //  The url may point into the request query, so copy it before the query changes
    newUrl = (query == 0 && wp->query) ? sfmt("%s?%s", url, wp->query) : sclone(url);
    if (query && !smatch(query, wp->query)) {
        //  The target's query replaces the request query and the variables parsed from it
        wfreeset(wp->query);
        wp->query = sclone(query);
        resetRequestVars(wp);
    }
    wfree(buf);
    wfreeset(wp->url);
    wp->url = newUrl;
    wfreeset(wp->path);
    wp->path = validated;
    wfreeset(wp->ext);
    if ((ext = getPathExtension(wp->path)) != NULL) {
        wp->ext = sclone(slower(ext));
    }
    wfreeset(wp->filename);
    wp->flags |= WEBS_REROUTE;
    return 0;
}


/* Reparse query and form variables on reroute; retain handler variables and upload fields. */
static void resetRequestVars(Webs *wp)
{
    WebsKey *sp;
    char    **names;
    int     count, i;

    if (!(wp->flags & WEBS_VARS_ADDED)) {
        return;
    }
    count = 0;
    for (sp = hashFirst(wp->vars); sp; sp = hashNext(wp->vars, sp)) {
        count++;
    }
    names = walloc((count + 1) * sizeof(char*));
    count = 0;
    for (sp = hashFirst(wp->vars); sp; sp = hashNext(wp->vars, sp)) {
        if (sp->arg == WEBS_VAR_QUERY || (sp->arg == WEBS_VAR_BODY && (wp->flags & WEBS_FORM))) {
            names[count++] = sclone(sp->name.value.string);
        }
    }
    for (i = 0; i < count; i++) {
        hashDelete(wp->vars, names[i]);
        wfree(names[i]);
    }
    wfree(names);
    wfreeset(wp->decodedQuery);
    wp->flags &= ~WEBS_VARS_ADDED;
}


PUBLIC bool websValid(Webs *wp)
{
    int wid;

    for (wid = 0; wid < websMax; wid++) {
        if (wp == webs[wid]) {
            return 1;
        }
    }
    return 0;
}


/*
    Build an ASCII time string.  If sbuf is NULL we use the current time, else we use the last modified time of sbuf;
 */
PUBLIC char *websGetDateString(WebsFileInfo *sbuf)
{
    return formatHttpDate(sbuf ? sbuf->mtime : websTime());
}


/*
    Format a time as an RFC 9110 IMF-fixdate, the form HTTP and cookie dates take. Caller must free.
 */
static char *formatHttpDate(WebsTime when)
{
    static cchar *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static cchar *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    struct tm    tm;
    time_t       timer;

    timer = toTimeT(when);
#if ME_UNIX_LIKE
    if (gmtime_r(&timer, &tm) == NULL) {
        return NULL;
    }
#else
    {
        struct tm *tp;
        if ((tp = gmtime(&timer)) == NULL) {
            return NULL;
        }
        tm = *tp;
    }
#endif
    return sfmt("%s, %02d %s %04d %02d:%02d:%02d GMT", days[tm.tm_wday], tm.tm_mday,
                months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
}


/*
    Clamp a WebsTime to the range of the platform time_t for the C library time functions.
 */
static time_t toTimeT(WebsTime when)
{
    WebsTime min, max;

    if (sizeof(time_t) < sizeof(WebsTime)) {
        /* A narrow time_t is 32 bits, signed or unsigned */
        if ((time_t) -1 > 0) {
            min = 0;
            max = (WebsTime) UINT_MAX;
        } else {
            min = (WebsTime) INT_MIN;
            max = (WebsTime) INT_MAX;
        }
        when = when < min ? min : (when > max ? max : when);
    }
    return (time_t) when;
}


/*
    Take not of the request activity and mark the time. Set a timestamp so that, later, we can return the number of
       seconds
    since we made the mark.
 */
PUBLIC void websNoteRequestActivity(Webs *wp)
{
    wp->timestamp = websTime();
}


/*
    Get the number of seconds since the last mark.
 */
static int64 getTimeSinceMark(Webs *wp)
{
    WebsTime now;

    if ((now = websTime()) < 0) {
        return 0;
    }
    return (int64) (now - wp->timestamp);
}


PUBLIC bool websValidUriChars(cchar *uri)
{
    ssize pos;

    if (uri == 0 || *uri == 0) {
        return 1;
    }
    pos = strspn(uri, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%");
    if (pos < slen(uri)) {
        trace(2, "Bad character 0x%02x in URI at offset %d", (uchar) uri[pos], (int) pos);
        return 0;
    }
    return 1;
}


/*
    Parse the URL. A single buffer is allocated to store the parsed URL in *pbuf. This must be freed by the caller.
 */
PUBLIC int websUrlParse(cchar *url, char **pbuf, char **pscheme, char **phost, char **pport, char **ppath, char **pext,
                        char **preference, char **pquery)
{
    char  *tok, *delim, *host, *path, *port, *scheme, *reference, *query, *ext, *buf, *buf2;
    ssize buflen, ulen, len, schemeLen;
    char  sep;

    assert(pbuf);
    if (url == 0) {
        url = "";
    }
    /*
        Allocate twice. Need to null terminate the host so have to copy the path.
     */
    ulen = strlen(url);
    len = max(ulen + 1, 2);
    buflen = len * 2;
    if ((buf = walloc(buflen)) == NULL) {
        return -1;
    }
    buf2 = &buf[len];
    sncopy(buf, len, url, ulen);
    sncopy(buf2, len, url, ulen);
    url = buf;

    scheme = 0;
    host = 0;
    port = 0;
    path = 0;
    ext = 0;
    query = 0;
    reference = 0;
    tok = buf;
    sep = '/';

    /*
        [scheme://][hostname[:port]][/path[.ext]][?query][#ref]
        First trim the reference and then the query (RFC 3986 3.4-3.5)
     */
    if ((reference = strchr(tok, '#')) != NULL) {
        *reference++ = '\0';
    }
    if ((query = strchr(tok, '?')) != NULL) {
        *query++ = '\0';
    }

    /*
        [scheme://][hostname[:port]][/path]
     */
    if ((schemeLen = schemeLength(tok)) > 0) {
        scheme = tok;
        tok[schemeLen - 3] = '\0';
        tok = &tok[schemeLen];
    }

    /*
        [hostname[:port]][/path]
     */
    if (*tok == '[' && ((delim = strchr(tok, ']')) != 0)) {
        /* IPv6 [::] */
        host = &tok[1];
        *delim++ = '\0';
        tok = delim;

    } else if (*tok && *tok != '/' && *tok != ':' && (scheme || strchr(tok, ':'))) {
        /*
           Supported forms:
               scheme://hostname
               hostname[:port][/path]
         */
        host = tok;
        if ((tok = strpbrk(tok, ":/")) == 0) {
            tok = "";
        }
        /* Don't terminate the hostname yet, need to see if tok is a ':' for a port. */
        assert(tok);
    }

    /* [:port][/path] */
    if (*tok == ':') {
        /* Terminate hostname */
        *tok++ = '\0';
        port = tok;
        if ((tok = strchr(tok, '/')) == 0) {
            tok = "";
        }
    }

    /* [/path] */
    if (*tok) {
        /*
           Terminate hostname. This zeros the leading path slash.
           This will be repaired before returning if ppath is set
         */
        sep = *tok;
        *tok++ = '\0';
        path = tok;
        /* path[.ext[/extra]] */
        ext = getPathExtension(path);
    }
    /*
        Pass back the requested fields
     */
    *pbuf = buf;
    if (pscheme) {
        if (scheme == 0) {
            scheme = "http";
        }
        *pscheme = scheme;
    }
    if (phost) {
        if (host == 0) {
            host = "localhost";
        }
        *phost = host;
    }
    if (pport) {
        *pport = port;
    }
    if (ppath) {
        if (path == 0) {
            scopy(buf2, 2, "/");
            path = buf2;
        } else {
            /* Copy path to reinsert leading slash */
            scopy(&buf2[1], len - 1, path);
            path = buf2;
            *path = sep;
        }
        *ppath = path;
    }
    if (pquery) {
        *pquery = query;
    }
    if (preference) {
        *preference = reference;
    }
    if (pext) {
        if (ext) {
            /* The extension comes off the path, so it follows the path grammar */
            decodeUrl(ext, ext, -1, 0);
#if ME_WIN_LIKE || MACOSX
            slower(ext);
#endif
        }
        *pext = ext;
    }
    return 0;
}


/*
    Normalize a URI path to remove "./",  "../" and redundant separators.
    Note: this does not make an abs path and does not map separators nor change case.
    This validates the URI and expects it to begin with "/".
    Returns an allocated path, caller must free.
 */
PUBLIC char *websNormalizeUriPath(cchar *pathArg)
{
    char *dupPath, *path, *sp, *dp, *mark, **segments;
    int  firstc, j, i, nseg, len;

    if (pathArg == 0 || *pathArg == '\0') {
        return sclone("");
    }
    len = (int) slen(pathArg);
    if ((dupPath = walloc(len + 2)) == 0) {
        return NULL;
    }
    strncpy(dupPath, pathArg, len + 1);
    dupPath[len + 1] = '\0';

    if ((segments = walloc(sizeof(char*) * (len + 1))) == 0) {
        wfree(dupPath);
        return NULL;
    }
    nseg = len = 0;
    firstc = *dupPath;
    for (mark = sp = dupPath; *sp; sp++) {
        if (*sp == '/') {
            *sp = '\0';
            while (sp[1] == '/') {
                sp++;
            }
            segments[nseg++] = mark;
            len += (int) (sp - mark);
            mark = sp + 1;
        }
    }
    segments[nseg++] = mark;
    len += (int) (sp - mark);
    for (j = i = 0; i < nseg; i++, j++) {
        sp = segments[i];
        if (sp[0] == '.') {
            if (sp[1] == '\0') {
                if ((i + 1) == nseg) {
                    /* Trim trailing "." */
                    segments[j] = "";
                } else {
                    j--;
                }
            } else if (sp[1] == '.' && sp[2] == '\0') {
                j = max(j - 2, -1);
                if ((i + 1) == nseg) {
                    nseg--;
                }
            } else {
                /* .more-chars */
                segments[j] = segments[i];
            }
        } else {
            segments[j] = segments[i];
        }
    }
    nseg = j;
    assert(nseg >= 0);
    if ((path = walloc(len + nseg + 1)) != 0) {
        for (i = 0, dp = path; i < nseg; ) {
            strcpy(dp, segments[i]);
            len = (int) slen(segments[i]);
            dp += len;
            if (++i < nseg || (nseg == 1 && *segments[0] == '\0' && firstc == '/')) {
                *dp++ = '/';
            }
        }
        *dp = '\0';
    }
    wfree(dupPath);
    wfree(segments);
    return path;
}


/*
    Unicode ranges that NFC composition removes: combining marks and the Hangul conjoining jamo.
    A path containing any of these is in decomposed (NFD) form. Normalization-insensitive file
    systems (APFS, HFS+) resolve NFD and the precomposed (NFC) spelling of a name to the same file,
    so an NFD request would be authorized against one string and opened as another. Composing NFD
    to NFC needs a large table; rejecting costs nothing and the NFC spelling always remains
    reachable. Not exhaustive - it covers the scripts that compose: Latin, Greek, Cyrillic, Hebrew,
    Arabic, Syriac, Hangul and Japanese kana.
 */
static const struct {
    uint low;
    uint high;
} combiningRanges[] = {
    { 0x0300, 0x036F },                 /* Combining diacritical marks (Latin, Greek, Cyrillic) */
    { 0x0483, 0x0489 },                 /* Cyrillic combining marks */
    { 0x0591, 0x05C7 },                 /* Hebrew points and accents */
    { 0x0610, 0x061A },                 /* Arabic marks */
    { 0x064B, 0x0670 },                 /* Arabic marks and superscript alef */
    { 0x06D6, 0x06ED },                 /* Arabic marks */
    { 0x0711, 0x0711 },                 /* Syriac letter superscript alaph */
    { 0x0730, 0x074A },                 /* Syriac points */
    { 0x1100, 0x11FF },                 /* Hangul conjoining jamo */
    { 0x1AB0, 0x1AFF },                 /* Combining diacritical marks extended */
    { 0x1DC0, 0x1DFF },                 /* Combining diacritical marks supplement */
    { 0x20D0, 0x20F0 },                 /* Combining marks for symbols */
    { 0x3099, 0x309A },                 /* Kana voiced/semi-voiced sound marks */
    { 0xFE20, 0xFE2F },                 /* Combining half marks */
};


#if ME_GOAHEAD_CASE_INSENSITIVE_FS
/*
    Non-ASCII code points whose full case folding, simple uppercase or simple lowercase contains an ASCII character.
    A case-insensitive file system can open one of these as an ASCII name: APFS applies full case folding (U+017F to
    "s", U+FB06 to "st") and NTFS its upcase table (U+0131 to "I"). Routes and the hidden-file policy compare ASCII
    case only, so such a spelling would be authorized against one name and opened as another.

    Derived from Unicode 16.0 CaseFolding.txt and UnicodeData.txt. The Unicode case folding stability policy keeps
    this set closed for assigned characters. To regenerate:
        python3 -c "print([hex(c) for c in range(0x80, 0x110000) if not 0xD800 <= c <= 0xDFFF and
            any(ord(x) < 0x80 for x in chr(c).casefold() + chr(c).upper() + chr(c).lower())])"
 */
static const uint caseFoldingCodePoints[] = {
    0x00DF,                             /* Sharp s, folds to "ss" */
    0x0130, 0x0131,                     /* Dotted capital I, dotless i */
    0x0149,                             /* n preceded by apostrophe */
    0x017F,                             /* Long s */
    0x01F0,                             /* j with caron */
    0x1E96, 0x1E97, 0x1E98, 0x1E99, 0x1E9A,
    0x1E9E,                             /* Capital sharp s */
    0x212A,                             /* Kelvin sign */
    0xFB00, 0xFB01, 0xFB02, 0xFB03, 0xFB04, 0xFB05, 0xFB06, /* Latin ligatures */
};
#endif


/*
    Decode one shortest-form UTF-8 scalar and advance sp; return zero for invalid input.
 */
static uint decodeUtf8(cchar **sp)
{
    static uint minimum[] = { 0, 0x80, 0x800, 0x10000 };
    cchar       *cp;
    uint        c, cp0;
    int         i, need;

    cp = *sp;
    cp0 = (uchar) *cp++;
    if (cp0 < 0x80) {
        *sp = cp;
        return cp0;
    } else if ((cp0 & 0xE0) == 0xC0) {
        need = 1;
        c = cp0 & 0x1F;
    } else if ((cp0 & 0xF0) == 0xE0) {
        need = 2;
        c = cp0 & 0x0F;
    } else if ((cp0 & 0xF8) == 0xF0) {
        need = 3;
        c = cp0 & 0x07;
    } else {
        return 0;
    }
    for (i = 0; i < need; i++) {
        if (((uchar) *cp & 0xC0) != 0x80) {
            return 0;
        }
        c = (c << 6) | ((uchar) *cp++ & 0x3F);
    }
    if (c < minimum[need] || (c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF) {
        return 0;
    }
    *sp = cp;
    return c;
}


/*
    Reject filesystem aliases before routing: trailing dot/space, ADS, 8.3 names, decomposed Unicode and malformed
    UTF-8. On case-insensitive filesystems also reject Unicode mappings to ASCII. Route matching handles ASCII case.
 */
static bool validCanonicalPath(cchar *path)
{
    cchar *sp, *segment;
    uint  c;
    int   i;

    segment = path;
    for (sp = path; *sp; ) {
        /*
            Reject ASCII control characters, DEL included. websValidUriChars runs on the raw URI and
            must permit '%', so percent-decoding delivers a real control byte here - a request for
            "/%0A" arrives as a path holding a newline. The accepted path becomes wp->path, which the
            access log writes as one line of a record and websRedirect passes unescaped into a
            Location header value. Both treat it as trusted single-line text, and this is the one
            choke point they share.
         */
        if ((uchar) *sp < 0x20 || (uchar) *sp == 0x7F) {
            return 0;
        }
        if (*sp == '/') {
            if (sp > segment && (sp[-1] == '.' || sp[-1] == ' ')) {
                return 0;
            }
            segment = ++sp;
            continue;
        }
        if (*sp == ':') {
            return 0;
        }
        if (*sp == '~') {
            for (c = 0, sp++; isdigit((uchar) *sp); sp++) {
                c++;
            }
            if (c > 0 && (*sp == '\0' || *sp == '/' || *sp == '.')) {
                return 0;
            }
            continue;
        }
        if ((uchar) *sp < 0x80) {
            sp++;
            continue;
        }
        if ((c = decodeUtf8(&sp)) == 0) {
            return 0;
        }
        for (i = 0; i < (int) (sizeof(combiningRanges) / sizeof(combiningRanges[0])); i++) {
            if (combiningRanges[i].low <= c && c <= combiningRanges[i].high) {
                return 0;
            }
        }
#if ME_GOAHEAD_CASE_INSENSITIVE_FS
        for (i = 0; i < (int) (sizeof(caseFoldingCodePoints) / sizeof(caseFoldingCodePoints[0])); i++) {
            if (caseFoldingCodePoints[i] == c) {
                return 0;
            }
        }
#endif
    }
    if (sp > segment && (sp[-1] == '.' || sp[-1] == ' ')) {
        return 0;
    }
    return 1;
}


/*
    Validate a URI path for use in a HTTP request line
    The URI must contain only valid characters and must being with "/" both before and after decoding.
    A decoded, normalized URI path is returned.
    The uri is modified. Returns an allocated path. Caller must free.
 */
PUBLIC char *websValidateUriPath(cchar *uri)
{
    char *decoded, *normalized;

    if (uri == 0 || *uri != '/') {
        return 0;
    }
    if (!websValidUriChars(uri)) {
        return 0;
    }
    decoded = walloc(slen(uri) + 1);
    decodeUrl(decoded, (char*) uri, -1, 0);
    normalized = websNormalizeUriPath(decoded);
    wfree(decoded);

    if (normalized == 0) {
        return 0;
    }
    if (*normalized != '/' || strchr(normalized, '\\') || !validCanonicalPath(normalized) ||
        hasUnsafeEscape(normalized)) {
        wfree(normalized);
        return 0;
    }
    return normalized;
}


/*
    Reject remaining escapes for NUL, dot, and separators to prevent double-decoded traversal.
 */
static bool hasUnsafeEscape(cchar *path)
{
    cchar *cp;

    for (cp = path; (cp = strchr(cp, '%')) != 0; cp++) {
        if (sncmp(cp, "%00", 3) == 0 || sncaselesscmp(cp, "%2e", 3) == 0 || sncaselesscmp(cp, "%2f", 3) == 0 ||
            sncaselesscmp(cp, "%5c", 3) == 0) {
            return 1;
        }
    }
    return 0;
}


PUBLIC cchar *websOriginForm(cchar *url)
{
    cchar *cp;
    ssize schemeLen;

    if ((schemeLen = schemeLength(url)) > 0) {
        return (cp = strchr(&url[schemeLen], '/')) != 0 ? cp : "/";
    }
    return url;
}


PUBLIC char *websParseContentType(cchar *contentType, cchar *param)
{
    cchar *cp, *start;
    char  *value, *vp;
    ssize nameLen;
    bool  found;

    if (contentType == 0) {
        return 0;
    }
    start = contentType + strspn(contentType, " \t");
    for (cp = start; *cp && *cp != ';'; cp++) { }
    if (param == 0) {
        value = snclone(start, cp - start);
        strim(value, " \t", WEBS_TRIM_END);
        return slower(value);
    }
    while (*cp == ';') {
        cp++;
        cp += strspn(cp, " \t");
        for (start = cp; isTchar((uchar) *cp); cp++) { }
        nameLen = cp - start;
        if (nameLen == 0 || *cp != '=') {
            return 0;
        }
        found = (nameLen == slen(param) && sncaselesscmp(start, param, nameLen) == 0);
        cp++;
        if (*cp == '"') {
            //  Quoted-string: copy the content, removing the backslash of each quoted-pair
            value = vp = walloc(slen(cp));
            for (cp++; *cp && *cp != '"'; cp++) {
                if (*cp == '\\' && cp[1]) {
                    cp++;
                }
                *vp++ = *cp;
            }
            *vp = '\0';
            if (*cp++ != '"') {
                wfree(value);
                return 0;
            }
        } else {
            for (start = cp; isTchar((uchar) *cp); cp++) { }
            value = snclone(start, cp - start);
        }
        cp += strspn(cp, " \t");
        if (*cp && *cp != ';') {
            wfree(value);
            return 0;
        }
        if (found) {
            return value;
        }
        wfree(value);
    }
    return 0;
}


#if ME_GOAHEAD_DENY_HIDDEN
/*
    Reject hidden segments and backup suffixes, except a leading /.well-known/.
 */
static bool isHiddenPath(cchar *path)
{
    cchar *segment, *end, *ext;
    ssize len;
    int   i;
    cchar *backups[] = { ".bak", ".orig", ".swp", ".tmp" };

    for (segment = path; segment && *segment; segment = end) {
        while (*segment == '/') {
            segment++;
        }
        if ((end = strchr(segment, '/')) == 0) {
            end = segment + slen(segment);
        }
        len = end - segment;
        if (len > 0 && *segment == '.' &&
            !(segment == &path[1] && len == 11 && sncmp(segment, ".well-known", 11) == 0)) {
            return 1;
        }
        if (len > 0) {
            if (segment[len - 1] == '~') {
                return 1;
            }
            for (i = 0; i < (int) (sizeof(backups) / sizeof(backups[0])); i++) {
                ext = backups[i];
                if (len > slen(ext) && sncaselesscmp(end - slen(ext), ext, slen(ext)) == 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}
#endif


PUBLIC bool websPageAllowed(Webs *wp)
{
    if (wp->filename && websIsReservedDevice(wp->filename)) {
        trace(2, "Refusing %s: reserved device name", wp->filename);
        return 0;
    }
#if ME_GOAHEAD_DENY_HIDDEN
    if (wp->path && isHiddenPath(wp->path)) {
        trace(2, "Refusing %s: hidden or backup file", wp->path);
        return 0;
    }
#endif
    return websPageContained(wp);
}


PUBLIC bool websPageContained(Webs *wp)
{
#if ME_GOAHEAD_CONTAIN_DOCUMENTS && ME_UNIX_LIKE
    cchar *root;
    char  *path, *real, *realRoot, *sep;
    ssize len;
    bool  contained;
    int   err;

    if (!wp->filename) {
        return 1;
    }
    /*
        realpath allocates with the C library, so release with free() not wfree()
     */
    contained = 0;
    real = 0;
    root = (wp->route && wp->route->dir) ? wp->route->dir : websGetDocuments();
    if (!root || (realRoot = realpath(root, NULL)) == 0) {
        trace(2, "Refusing %s: cannot resolve the document root %s, errno %d", wp->filename, root, errno);
        return 0;
    }
    /* Use the parent only for missing files. Refuse other resolution errors. */
    if ((real = realpath(wp->filename, NULL)) == 0) {
        if (errno != ENOENT && errno != ENOTDIR) {
            trace(2, "Refusing %s: cannot resolve it, errno %d", wp->filename, errno);
            goto done;
        }
        err = ENOENT;
        path = sclone(wp->filename);
        if ((sep = strrchr(path, '/')) != 0) {
            *sep = '\0';
            if ((real = realpath(*path ? path : "/", NULL)) == 0) {
                err = errno;
            }
        }
        wfree(path);
        if (!real) {

            if (err == ENOENT || err == ENOTDIR) {
                contained = 1;
            } else {
                trace(2, "Refusing %s: cannot resolve its directory, errno %d", wp->filename, err);
            }
            goto done;
        }
    }
    len = slen(realRoot);
    if (len == 1 && realRoot[0] == '/') {
        contained = 1;
        goto done;
    }
    if (strncmp(real, realRoot, len) != 0 || (real[len] != '/' && real[len] != '\0')) {
        trace(2, "Refusing %s: it resolves to %s, outside %s", wp->filename, real, realRoot);
        goto done;
    }
#if ME_GOAHEAD_DENY_HIDDEN
    /*
        Check stored names too; links and filesystem case rules may resolve another spelling.
     */
    if (isHiddenPath(&real[len])) {
        trace(2, "Refusing %s: it resolves to hidden or backup file %s", wp->filename, real);
        goto done;
    }
#endif
    contained = 1;

done:
    free(real);
    free(realRoot);
    return contained;
#else
    return 1;
#endif
}


PUBLIC int websPageOpen(Webs *wp, int mode, int perm)
{
    assert(websValid(wp));
    if (!websPageAllowed(wp)) {
        errno = ENOENT;
        return wp->docfd = -1;
    }
    return wp->docfd = websOpenFile(wp->filename, mode, perm);
}


PUBLIC void websPageClose(Webs *wp)
{
    assert(websValid(wp));

    if (wp->docfd >= 0) {
        websCloseFile(wp->docfd);
        wp->docfd = -1;
    }
}


PUBLIC int websPageStat(Webs *wp, WebsFileInfo *sbuf)
{
    /*
        Use the open file's metadata so it matches the response bytes.
     */
    if (wp->docfd >= 0) {
        return websStatOpenFile(wp->docfd, sbuf);
    }
    if (!websPageAllowed(wp)) {
        return -1;
    }
    return websStatFile(wp->filename, sbuf);
}


PUBLIC int websPageIsDirectory(Webs *wp)
{
    WebsFileInfo sbuf;

    if (websPageAllowed(wp) && websStatFile(wp->filename, &sbuf) >= 0) {
        return sbuf.isDir;
    }
    return 0;
}


/*
    Read a web page. Returns the number of _bytes_ read. len is the size of buf, in bytes.
 */
PUBLIC ssize websPageReadData(Webs *wp, char *buf, ssize nBytes)
{

    assert(websValid(wp));
    return websReadFile(wp->docfd, buf, nBytes);
}


/*
    Move file pointer offset bytes.
 */
PUBLIC void websPageSeek(Webs *wp, Offset offset, int origin)
{
    assert(websValid(wp));

    websSeekFile(wp->docfd, offset, origin);
}


/*
    Reject cookie characters that could end a component or inject attributes.
 */
static bool validCookiePart(cchar *part, cchar *reject)
{
    cuchar *cp;

    for (cp = (cuchar*) part; *cp; cp++) {
        if (*cp <= ' ' || *cp >= 0x7f || strchr(reject, *cp)) {
            return 0;
        }
    }
    return 1;
}


PUBLIC void websSetCookie(Webs *wp, cchar *name, cchar *value, cchar *path, cchar *domain, int lifespan, int flags)
{
    WebsTime now;
    char     *expiresAtt, *expires, *domainAtt, *secure, *httponly, *cookie, *sameSite;

    assert(wp);
    assert(name && *name);

    if (value == 0) {
        value = "";
    }
    if (path == 0) {
        path = "/";
    }
    if (domain == 0) {
        domain = "";
    }
    if (!validCookiePart(name, "()<>@,;:\\\"/[]?={}") || !validCookiePart(value, "\",;\\") ||
        !validCookiePart(path, ";") || !validCookiePart(domain, "\",;\\")) {
        error("Refusing to set cookie \"%s\" with invalid characters", name);
        return;
    }
    /*
        Omit Domain by default to keep cookies host-only.
     */
    domainAtt = *domain ? "; domain=" : "";
    //  Clients read a cookie date as GMT
    if (lifespan > 0 && (now = websTime()) >= 0 && (expires = formatHttpDate(now + lifespan)) != 0) {
        expiresAtt = "; expires=";
    } else {
        expiresAtt = "";
        expires = 0;
    }
    /*
       Allow multiple cookie headers. Even if the same name. Later definitions take precedence
     */
    secure = (flags & WEBS_COOKIE_SECURE) ? "; secure" : "";
    httponly = (flags & WEBS_COOKIE_HTTP) ?  "; httponly" : "";
    sameSite = "";
    if (flags & WEBS_COOKIE_SAME_LAX) {
        sameSite = "; SameSite=Lax";
    } else if (flags & WEBS_COOKIE_SAME_STRICT) {
        sameSite = "; SameSite=Strict";
    }
    cookie = sfmt("%s=%s; path=%s%s%s%s%s%s%s%s", name, value, path, domainAtt, domain, expiresAtt,
                  expires ? expires : "", secure, httponly, sameSite);
    wfree(expires);
    /*
        The hash must own the cookie string. With VALUE_ALLOCATE, valueFree() releases it both when
        hashEnter replaces a same-named cookie and when termWebs calls hashFree. Without it, neither
        path frees and every Set-Cookie orphans its allocation.
     */
    hashEnter(wp->responseCookies, name, valueString(cookie, VALUE_ALLOCATE), 0);
    wfree(cookie);
}


/*
    Return the next token in the input stream. Does not allocate.
    The content buffer is advanced to the next token.
    The delimiter is a string to match and not a set of characters.
    If the delimeter null, it means use white space (space or tab) as a delimiter.
 */
static char *getToken(Webs *wp, char *delim, int validation)
{
    WebsBuf *buf;
    char    *token, *endToken;
    int     strict;

    assert(wp);

    strict = validation & TOKEN_STRICT;
    validation &= ~TOKEN_STRICT;
    buf = &wp->rxbuf;
    token = bufStart(buf);
    endToken = (char*) buf->endp;

    /*
        Keep whitespace on strict tokens so request-line validation can reject it.
     */
    if (!strict && validation != TOKEN_HEADER_VALUE && validation != TOKEN_HEADER_KEY) {
        for (; token < (char*) buf->endp && (*token == ' ' || *token == '\t'); token++) { }
    }

    if (delim) {
        if ((endToken = strstr(token, delim)) == NULL) {
            return NULL;
        }
        /* Only eat one occurrence of the delimiter */
        buf->servp = endToken + strlen(delim);
        *endToken = '\0';

    } else {
        delim = " \t";
        if ((endToken = strpbrk(token, delim)) == NULL) {
            return NULL;
        }
        buf->servp = endToken + strspn(endToken, delim);
        *endToken = '\0';
    }
    token = validateToken(token, endToken, validation);
    return token;
}


static char *validateToken(char *token, char *endToken, int validation)
{
    char *t;

    if (validation == TOKEN_HEADER_KEY) {
        if (token == NULL || *token == '\0') {
            return NULL;
        }
        if (strpbrk(token, "\"\\/ \t\r\n(),:;<=>?@[]{}")) {
            return NULL;
        }
        for (t = token; *t; t++) {
            if (!isprint((uchar) *t)) {
                return NULL;
            }
        }
    } else if (validation == TOKEN_HEADER_VALUE) {
        /*
            Trim only SP and HTAB; retain bare CR/LF for rejection.
         */
        if (token < endToken) {
            for (t = &token[slen(token) - 1]; t >= token; t--) {
                if (*t == ' ' || *t == '\t') {
                    *t = '\0';
                } else {
                    break;
                }
            }
        }
        while (*token == ' ' || *token == '\t') {
            token++;
        }
        for (t = token; *t; t++) {
            if (!isprint((uchar) *t)) {
                return NULL;
            }
        }
    } else if (validation == TOKEN_URI_VALUE) {
        //  A request target never carries a fragment (RFC 9112 3.2)
        if (!websValidUriChars(token) || strchr(token, '#') != NULL) {
            return NULL;
        }
    } else if (validation == TOKEN_NUMBER) {
        if (!snumber(token)) {
            return NULL;
        }
    } else if (validation == TOKEN_WORD) {
        if (strpbrk(token, " \t\r\n") != NULL) {
            return NULL;
        }
    } else {
        if (strpbrk(token, "\r\n") != NULL) {
            return NULL;
        }
    }
    return token;
}


PUBLIC int websGetBackground(void)
{
    return websBackground;
}


PUBLIC void websSetBackground(int on)
{
    websBackground = on;
}


PUBLIC int websGetDebug(void)
{
    return websDebug;
}


PUBLIC void websSetDebug(int on)
{
    websDebug = on;
}


#if ME_UNIX_LIKE
PUBLIC int websDropPrivileges(cchar *user, cchar *group)
{
    struct passwd *pw;
    struct group  *gr;
    uid_t         uid;
    gid_t         gid;

    if ((user == 0 || *user == '\0') && (group == 0 || *group == '\0')) {
        return 0;
    }
    pw = 0;
    uid = geteuid();
    gid = getegid();
    if (user && *user) {
        if ((pw = getpwnam(user)) == 0) {
            error("Cannot find user \"%s\"", user);
            return -1;
        }
        uid = pw->pw_uid;
        gid = pw->pw_gid;
    }
    if (group && *group) {
        if ((gr = getgrnam(group)) == 0) {
            error("Cannot find group \"%s\"", group);
            return -1;
        }
        gid = gr->gr_gid;
    }
    if (geteuid() != 0) {
        if (uid != geteuid() || gid != getegid()) {
            error("Cannot change to user %d group %d: not running as root", (int) uid, (int) gid);
            return -1;
        }
        return 0;
    }
    /*
        Change groups before dropping root privileges.
     */
    if (pw ? initgroups(pw->pw_name, gid) < 0 : setgroups(1, &gid) < 0) {
        error("Cannot set supplementary groups, errno %d", errno);
        return -1;
    }
    if (setgid(gid) < 0) {
        error("Cannot change group to %d, errno %d", (int) gid, errno);
        return -1;
    }
    if (setuid(uid) < 0) {
        error("Cannot change user to %d, errno %d", (int) uid, errno);
        return -1;
    }
    if (getuid() != uid || geteuid() != uid || getgid() != gid || getegid() != gid) {
        error("Privilege change incomplete");
        return -1;
    }
    if ((uid != 0 && setuid(0) == 0) || (gid != 0 && setgid(0) == 0)) {
        error("Privilege change is reversible");
        return -1;
    }
    logmsg(2, "Running as user %d group %d", (int) uid, (int) gid);
    return 0;
}
#else

PUBLIC int websDropPrivileges(cchar *user, cchar *group)
{
    if ((user && *user) || (group && *group)) {
        error("Changing user and group is not supported on this platform");
        return -1;
    }
    return 0;
}
#endif


/*
    Override the runtime POST-body size cap. Defaults to ME_GOAHEAD_LIMIT_POST.
    Used by the test harness to relax the production-tight default for upload stress tests.
 */
PUBLIC void websSetPostLimit(ssize limit)
{
    postLimit = limit > 0 ? limit : ME_GOAHEAD_LIMIT_POST;
}


static char *makeSessionID(Webs *wp)
{
    char randomData[16];
    char *b64id, *sessionId;

    assert(wp);
    if (websGetRandomBytes(randomData, sizeof(randomData), 0) < 0) {
        return NULL;
    }
    b64id = websEncode64Block(randomData, sizeof(randomData));
    if (!b64id) {
        return NULL;
    }
    sessionId = sfmt("::webs.session::%s", b64id);
    wfree(b64id);
    return sessionId;
}


/*
    Clear references before freeing the user; later checks resolve the username again.
 */
#if ME_GOAHEAD_AUTH
PUBLIC void websReleaseUser(struct WebsUser *user)
{
    int wid;

    for (wid = 0; webs && wid < websMax; wid++) {
        if (webs[wid] && webs[wid]->user == user) {
            webs[wid]->user = 0;
        }
    }
}
#endif


PUBLIC int websRouteHolders(WebsRoute *route)
{
    int count, wid;

    count = 0;
    for (wid = 0; webs && wid < websMax; wid++) {
        if (webs[wid] && webs[wid]->route == route) {
            count++;
        }
    }
    return count;
}


/* Count active and retired session references across open requests; initWebs clears both on allocation and reuse. */
static int sessionHolders(WebsSession *sp)
{
    int count, wid;

    count = 0;
    for (wid = 0; webs && wid < websMax; wid++) {
        if (webs[wid] && (webs[wid]->session == sp || webs[wid]->retiredSession == sp)) {
            count++;
        }
    }
    return count;
}


/*
    Remove a retired session from lookup immediately, but free it only after its last request releases it. Return
    true if freed.
 */
static bool retireSession(WebsSession *sp)
{
    assert(sp);

    if (!sp->retired) {
        hashDelete(sessions, sp->id);
        sessionCount--;
        sp->retired = 1;
    }
    if (sessionHolders(sp) == 0) {
        freeSession(sp);
        return 1;
    }
    return 0;
}


/* Release *ref and free a retired session when its last holder leaves. */
static void releaseSessionRef(WebsSession **ref)
{
    WebsSession *sp;

    if ((sp = *ref) != 0) {
        *ref = 0;
        if (sp->retired) {
            retireSession(sp);
        }
    }
}


/*
    Release this request's session references. Called from termWebs for both connection close and
    keep-alive reuse.
 */
static void releaseSession(Webs *wp)
{
    assert(wp);

    releaseSessionRef(&wp->session);
    releaseSessionRef(&wp->retiredSession);
}


PUBLIC void websDestroySession(Webs *wp)
{
    WebsSession *sp;

    websGetSession(wp, 0);
    if ((sp = wp->session) != 0) {
        /* Keep the retired session until this request ends so values returned by websGetSessionVar remain valid. */
        wp->session = 0;
        releaseSessionRef(&wp->retiredSession);
        wp->retiredSession = sp;
        if (retireSession(sp)) {
            //  A request outside the connection list is not counted as a holder, so the session is already freed
            wp->retiredSession = 0;
        }
    }
}


/*
    SECURITY: Rotate the session id at a privilege transition. Destroys the session bound to this
    request server-side and issues a new session with a freshly minted, server-generated id and a
    Set-Cookie. Session data is deliberately not carried across: everything held in the old session
    was written while the request was unauthenticated, possibly by whoever planted the id. Callers
    must use this, not websGetSession(wp, 1), when authenticating a user: websGetSession adopts a
    live client-presented id, which writes the authenticated identity into a session of the client's
    choosing.
 */
PUBLIC WebsSession *websRotateSession(Webs *wp)
{
    websDestroySession(wp);
    return websGetSession(wp, 1);
}


PUBLIC WebsSession *websCreateSession(Webs *wp)
{
    return websRotateSession(wp);
}


PUBLIC WebsSession *websLoginSession(Webs *wp, cchar *authType)
{
#if ME_GOAHEAD_LIMIT_SESSION_USER > 0
    WebsSession *sp, *oldest;
    WebsKey     *sym, *owner;
    int         count;

    /*
        Evict least-recently-used sessions before adding one beyond the account limit.
     */
    websGetSession(wp, 0);
    do {
        count = 0;
        oldest = 0;
        for (sym = hashFirst(sessions); sym; sym = hashNext(sessions, sym)) {
            sp = (WebsSession*) sym->content.value.symbol;
            if (sp == wp->session || (owner = hashLookup(sp->cache, WEBS_SESSION_USERNAME)) == 0) {
                continue;
            }
            if (smatch(owner->content.value.string, wp->username)) {
                count++;
                if (oldest == 0 || sp->used < oldest->used) {
                    oldest = sp;
                }
            }
        }
        if (count >= ME_GOAHEAD_LIMIT_SESSION_USER) {
            retireSession(oldest);
        }
    } while (count > ME_GOAHEAD_LIMIT_SESSION_USER);
#endif
    if (websRotateSession(wp) == 0) {
        return 0;
    }
    if (websSetSessionVar(wp, WEBS_SESSION_USERNAME, wp->username) < 0 ||
        websSetSessionVar(wp, WEBS_SESSION_AUTHTYPE, authType) < 0) {
        return 0;
    }
    return wp->session;
}


WebsSession *websAllocSession(Webs *wp, cchar *id, int lifespan)
{
    WebsSession *sp;
    WebsTime    now;

    assert(wp);

    /*
        A session cannot be given an expiry without a clock, so create nothing.
     */
    if ((now = websTime()) < 0) {
        trace(2, "Cannot create session: system clock unavailable");
        return 0;
    }
    /*
        Count sessions only when they enter or leave the store.
     */
    if (sessionCount >= ME_GOAHEAD_LIMIT_SESSION_COUNT) {
        pruneExpired();
        if (sessionCount >= ME_GOAHEAD_LIMIT_SESSION_COUNT) {
            error("Too many sessions %d/%d", sessionCount, ME_GOAHEAD_LIMIT_SESSION_COUNT);
            return 0;
        }
    }
    if ((sp = walloc(sizeof(WebsSession))) == 0) {
        return 0;
    }
    sp->lifespan = lifespan;
    sp->retired = 0;
    sp->expires = now + lifespan;
    sp->used = ++sessionUse;
    if (id == 0) {
        sp->id = makeSessionID(wp);
    } else {
        sp->id = sclone(id);
    }
    /*
        Fail session creation if entropy is unavailable.
     */
    if (sp->id == 0) {
        wfree(sp);
        return 0;
    }
    if ((sp->cache = hashCreate(WEBS_SESSION_HASH)) < 0) {
        wfreeset(sp->id);
        wfree(sp);
        return 0;
    }
    if (hashEnter(sessions, sp->id, valueSymbol(sp), 0) == 0) {
        hashFree(sp->cache);
        wfreeset(sp->id);
        wfree(sp);
        return 0;
    }
    sessionCount++;
    return sp;
}


static void freeSession(WebsSession *sp)
{
    assert(sp);

    if (sp->cache >= 0) {
        hashFree(sp->cache);
        sp->cache = -1;
    }
    wfreeset(sp->id);
    wfree(sp);
}


WebsSession *websGetSession(Webs *wp, int create)
{
    WebsSession *sp;
    WebsTime    now;
    WebsKey     *sym;
    char        *id;
    int         flags;

    assert(wp);

    if (!wp->session) {
        id = websGetSessionID(wp);
        if ((sym = hashLookup(sessions, id)) != 0) {
            sp = (WebsSession*) sym->content.value.symbol;
            /*
                Reject expired sessions on lookup before refreshing them. A failed clock expires every session.
             */
            if ((now = websTime()) < 0 || sp->expires <= now) {
                retireSession(sp);
            } else {
                wp->session = sp;
            }
        }
        wfree(id);
        if (!wp->session) {
            if (!create) {
                return 0;
            }
            /*
                SECURITY: Always mint a fresh, server-generated session id when creating a session.
                The client-supplied cookie value is honored above only when it matches a live
                server-issued session (the hashLookup hit). Never adopt an unknown client value as a
                new session id: that enables session fixation, where an id planted in the victim's
                browser before login survives into the authenticated session. Passing NULL makes
                websAllocSession() generate a new random id via makeSessionID(); this also rotates the
                id on login, since websCreateSession() destroys the old session and re-enters here.
             */
            if ((wp->session = websAllocSession(wp, NULL, ME_GOAHEAD_LIMIT_SESSION_LIFE)) == 0) {
                return 0;
            }
            flags = WEBS_COOKIE_SAME_LAX | WEBS_COOKIE_HTTP;
            if (wp->flags & WEBS_HTTPS) {
                flags |= WEBS_COOKIE_SECURE;
            }
            websSetCookie(wp, WEBS_SESSION, wp->session->id, "/", NULL, 0, flags);
        }
    }
    /*
        Refresh the sliding inactivity timeout on access.
     */
    if (wp->session && !wp->session->retired) {
        if ((now = websTime()) >= 0) {
            wp->session->expires = now + wp->session->lifespan;
        }
        wp->session->used = ++sessionUse;
    }
    return wp->session;
}


PUBLIC char *websParseCookie(Webs *wp, char *name)
{
    char *buf, *cookie, *end, *key, *tok, *value, *vtok;

    assert(wp);

    if (wp->cookie == 0 || name == 0 || *name == '\0') {
        return 0;
    }
    buf = sclone(wp->cookie);
    end = &buf[slen(buf)];
    value = 0;

    for (tok = buf; tok && tok < end; ) {
        cookie = stok(tok, ";", &tok);
        cookie = strim(cookie, " ", WEBS_TRIM_START);
        if (cookie == 0 || *cookie == '\0') {
            continue;
        }
        key = stok(cookie, "=", &vtok);
        if (smatch(key, name)) {
            // Remove leading spaces first, then double quotes. Spaces inside double quotes preserved.
            value = sclone(strim(strim(vtok, " ", WEBS_TRIM_BOTH), "\"", WEBS_TRIM_BOTH));
            break;
        }
    }
    wfree(buf);
    return value;
}


PUBLIC char *websGetSessionID(Webs *wp)
{
    assert(wp);

    if (wp->session) {
        /*
            Return an owned copy as required by the API contract.
         */
        return sclone(wp->session->id);
    }
    return websParseCookie(wp, WEBS_SESSION);
}


PUBLIC cchar *websGetSessionVar(Webs *wp, cchar *key, cchar *defaultValue)
{
    WebsSession *sp;
    WebsKey     *sym;

    assert(wp);
    assert(key && *key);

    /*
        SECURITY: reading a session variable must not create a session. Creating one here would let any
        handler on an unauthenticated route mint server state (and a Set-Cookie) for an anonymous client,
        which is an unauthenticated path to exhausting ME_GOAHEAD_LIMIT_SESSION_COUNT.
     */
    if ((sp = websGetSession(wp, 0)) != 0) {
        if ((sym = hashLookup(sp->cache, key)) == 0) {
            return defaultValue;
        }
        return (char*) sym->content.value.symbol;
    }
    return defaultValue;
}


PUBLIC void websRemoveSessionVar(Webs *wp, cchar *key)
{
    WebsSession *sp;

    assert(wp);
    assert(key && *key);

    /*
        SECURITY: removing a session variable must not create a session. See websGetSessionVar.
     */
    if ((sp = websGetSession(wp, 0)) != 0) {
        hashDelete(sp->cache, key);
    }
}


PUBLIC int websSetSessionVar(Webs *wp, cchar *key, cchar *value)
{
    WebsSession *sp;

    assert(wp);
    assert(key && *key);
    assert(value);

    /*
        Writing a session variable is the one operation that legitimately establishes a session.
     */
    if ((sp = websGetSession(wp, 1)) == 0) {
        return -1;
    }
    if (hashEnter(sp->cache, key, valueString(value, VALUE_ALLOCATE), 0) == 0) {
        return -1;
    }
    return 0;
}



static void pruneExpired(void)
{
    WebsSession *sp;
    WebsTime    when;
    WebsKey     *sym, *next;

    if (sessions >= 0) {
        if ((when = websTime()) < 0) {
            return;
        }
        for (sym = hashFirst(sessions); sym; sym = next) {
            next = hashNext(sessions, sym);
            sp = (WebsSession*) sym->content.value.symbol;
            if (sp->expires <= when) {
                retireSession(sp);
            }
        }
    }
}


static void pruneSessions(void *data, int id)
{
    int oldCount;

    if (sessions >= 0) {
        oldCount = sessionCount;
        pruneExpired();
        if (oldCount != sessionCount || sessionCount) {
            trace(4, "Prune %d sessions. Remaining: %d", oldCount - sessionCount, sessionCount);
        }
    }
    websRestartEvent(pruneId, WEBS_SESSION_PRUNE);
}


static void freeSessions(void)
{
    WebsSession *sp;
    WebsKey     *sym, *next;

    if (sessions >= 0) {
        for (sym = hashFirst(sessions); sym; sym = next) {
            next = hashNext(sessions, sym);
            sp = (WebsSession*) sym->content.value.symbol;
            hashDelete(sessions, sp->id);
            freeSession(sp);
        }
        hashFree(sessions);
        sessions = -1;
        sessionCount = 0;
    }
}


/*
    One line embedding
 */
PUBLIC int websServer(cchar *endpoint, cchar *documents)
{
    int finished = 0;

    if (websOpen(documents, "route.txt") < 0) {
        error("Cannot initialize server. Exiting.");
        return -1;
    }
    if (websLoad("auth.txt") < 0) {
        error("Cannot load auth.txt");
        return -1;
    }
    if (websListen(endpoint) < 0) {
        return -1;
    }
    websServiceEvents(&finished);
    websClose();
    return 0;
}


static void setFileLimits(void)
{
#if ME_UNIX_LIKE
    struct rlimit r;
    int           limit, avail;

    limit = ME_GOAHEAD_LIMIT_FILES;
    if (limit) {
        r.rlim_cur = r.rlim_max = limit;
        if (setrlimit(RLIMIT_NOFILE, &r) < 0) {
            error("Cannot set file limit to %d", limit);
        }
    }
    getrlimit(RLIMIT_NOFILE, &r);
    trace(6, "Max files soft %ld, max %ld", (long) r.rlim_cur, (long) r.rlim_max);
    /* Reserve descriptors for listeners, logs, documents and staging before capping connections. */
    if (r.rlim_cur != RLIM_INFINITY && r.rlim_cur < (rlim_t) INT_MAX) {
        avail = (int) r.rlim_cur - FILE_RESERVE;
        if (avail < connLimit) {
            connLimit = max(avail, 1);
            trace(2, "Connection limit lowered to %d to fit the file limit %ld", connLimit, (long) r.rlim_cur);
        }
    }
#endif
}

/*
    Output an error message and cleanup
 */
PUBLIC void websError(Webs *wp, int code, cchar *fmt, ...)
{
    va_list args;
    char    *msg, *buf;
    char    *encoded;

    assert(wp);
    wp->error = 1;
    if (code & WEBS_CLOSE) {
        wp->flags &= ~WEBS_KEEP_ALIVE;
        wp->connError++;
    }
    /*
        Record the error without writing another response.
     */
    if (wp->finalized) {
        if (fmt && !(code & WEBS_NOLOG)) {
            va_start(args, fmt);
            msg = sfmtv(fmt, args);
            va_end(args);
            trace(2, "%s (request already complete)", msg);
            wfree(msg);
        }
        return;
    }
#if !ME_ROM
    if (wp->putfd >= 0) {
        close(wp->putfd);
        wp->putfd = -1;
    }
#endif
    encoded = websEscapeHtml(wp->url);
    wfreeset(wp->url);
    wp->url = encoded;
    if (fmt) {
        if (!(code & WEBS_NOLOG)) {
            va_start(args, fmt);
            msg = sfmtv(fmt, args);
            va_end(args);
            trace(2, "%s", msg);
            wfree(msg);
        }
        buf =
            sfmt(
                "\
<html>\r\n\
    <head><title>Document Error: %s</title></head>\r\n\
    <body>\r\n\
        <h2>Access Error: %s</h2>\r\n\
    </body>\r\n\
</html>\r\n",
                websErrorMsg(
                    code), websErrorMsg(code));
    } else {
        buf = 0;
    }
    websResponse(wp, code, buf);
    wfree(buf);
}


/*
    Return the error message for a given code
 */
PUBLIC cchar *websErrorMsg(int code)
{
    WebsError *ep;

    assert(code >= 0);
    code &= WEBS_CODE_MASK;
    for (ep = websErrors; ep->code; ep++) {
        if (code == ep->code) {
            return ep->msg;
        }
    }
    return websErrorMsg(HTTP_CODE_INTERNAL_SERVER_ERROR);
}


/*
    Accessors
 */
PUBLIC cchar *websGetCookie(Webs *wp)
{
    return wp->cookie;
}
PUBLIC cchar *websGetDir(Webs *wp)
{
    return wp->route && wp->route->dir ? wp->route->dir : websGetDocuments();
}
PUBLIC int  websGetEof(Webs *wp)
{
    return wp->eof;
}
PUBLIC cchar *websGetExt(Webs *wp)
{
    return wp->ext;
}
PUBLIC cchar *websGetFilename(Webs *wp)
{
    return wp->filename;
}
PUBLIC cchar *websGetHost(Webs *wp)
{
    return wp->host;
}
PUBLIC cchar *websGetIfaddr(Webs *wp)
{
    return wp->ifaddr;
}
PUBLIC cchar *websGetIpaddr(Webs *wp)
{
    return wp->ipaddr;
}
PUBLIC cchar *websGetMethod(Webs *wp)
{
    return wp->method;
}
PUBLIC cchar *websGetPassword(Webs *wp)
{
    return wp->password;
}
PUBLIC cchar *websGetPath(Webs *wp)
{
    return wp->path;
}
PUBLIC int   websGetPort(Webs *wp)
{
    return wp->port;
}
PUBLIC cchar *websGetProtocol(Webs *wp)
{
    return wp->protocol;
}
PUBLIC cchar *websGetQuery(Webs *wp)
{
    return wp->query;
}
PUBLIC cchar *websGetServer(void)
{
    return websHost;
}
PUBLIC cchar *websGetServerAddress(void)
{
    return websIpAddr;
}
PUBLIC cchar *websGetServerAddressUrl(void)
{
    return websIpAddrUrl;
}
PUBLIC cchar *websGetServerUrl(void)
{
    return websHostUrl;
}
PUBLIC cchar *websGetUrl(Webs *wp)
{
    return wp->url;
}
PUBLIC cchar *websGetUserAgent(Webs *wp)
{
    return wp->userAgent;
}
PUBLIC cchar *websGetUsername(Webs *wp)
{
    return wp->username;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/js.c ************/

/*
    js.c -- Mini JavaScript

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "js.h"

#if ME_GOAHEAD_JAVASCRIPT
/********************************** Defines ***********************************/

#define     OCTAL                       8
#define     HEX                         16

#if defined(__clang__) && defined(__has_attribute)
    #if __has_attribute(no_sanitize)
        #define JS_NO_SANITIZE_FUNCTION __attribute__((no_sanitize("function")))
    #endif
#endif
#ifndef JS_NO_SANITIZE_FUNCTION
    #define JS_NO_SANITIZE_FUNCTION
#endif

/*
    Allocate loop positions only while parsing a for statement to reduce recursive stack use.
 */
typedef struct JsLoop {
    JsInput cond;                               /* Before the condition */
    JsInput incr;                               /* Before the increment */
    JsInput body;                               /* Before the body */
    JsInput end;                                /* After the body */
} JsLoop;

static Js  **jsHandles;     /* List of js handles */
static int jsMax = -1;      /* Maximum size of  */

/****************************** Forward Declarations **************************/

static Js *jsPtr(int jid);
static void     clearString(char **ptr);
static void     setString(char **ptr, cchar *s);
static int      appendString(Js *ep, char **ptr, cchar *s);
static int      parse(Js *ep, int state, int flags);
static int      parseStmt(Js *ep, int state, int flags);
static int      parseDeclaration(Js *ep, int state, int flags);
static int      parseCond(Js *ep, int state, int flags);
static int      parseExpr(Js *ep, int state, int flags);
static int      parseFunctionArgs(Js *ep, int state, int flags);
static int      evalExpr(Js *ep, cchar *lhs, int rel, cchar *rhs);
static int      evalCond(Js *ep, cchar *lhs, int rel, cchar *rhs, int flags);
static int      isTrue(Js *ep, cchar *value, int flags, bool *truth);
static int      parseInteger(Js *ep, cchar *value, int *result);
static int      evalFunction(Js *ep);
static void     freeFunc(JsFun *func);
static void     jsRemoveNewlines(Js *ep, int state);

static int      getLexicalToken(Js *ep, int state);
static int      tokenAddChar(Js *ep, int c);
static int      inputGetc(Js *ep);
static void     inputPutback(Js *ep, int c);
static int      charConvert(Js *ep, int base, int maxDig);

/************************************* Code ***********************************/

/*
    Push a variable frame onto the frame stack and return its frame id. Frames are a simple stack: the frame id is
    the index and frames must be closed in reverse order of creation.
 */
static int pushFrame(Js *ep, WebsHash variables)
{
    assert(ep);

    ep->variables = wrealloc(ep->variables, sizeof(WebsHash) * (ep->variableMax + 1));
    ep->variables[ep->variableMax] = variables + JS_OFFSET;
    return ep->variableMax++;
}


PUBLIC int jsOpenEngine(WebsHash variables, WebsHash functions)
{
    Js  *ep;
    int jid;

    if ((jid = wallocObject(&jsHandles, &jsMax, sizeof(Js))) < 0) {
        return -1;
    }
    ep = jsHandles[jid];
    ep->jid = jid;

    /*
        Create a top level symbol table if one is not provided for variables and functions. Variables may create other
        symbol tables for block level declarations so the tables are kept in a stack of variable frames.
     */
    if (variables == -1) {
        pushFrame(ep, hashCreate(64));
        ep->flags |= FLAGS_VARIABLES;
    } else {
        pushFrame(ep, variables);
    }
    if (functions == -1) {
        ep->functions = hashCreate(64);
        ep->flags |= FLAGS_FUNCTIONS;
    } else {
        ep->functions = functions;
    }
    jsLexOpen(ep);
    ep->recursionDepth = 0;

    /*
        Define standard constants
     */
    jsSetGlobalVar(ep->jid, "null", NULL);
    return ep->jid;
}


PUBLIC void jsCloseEngine(int jid)
{
    Js  *ep;
    int i;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }

    wfreeset(ep->error);

    wfreeset(ep->result);

    jsLexClose(ep);

    /*
        Free every block frame. The global frame (0) is only freed if the engine created it as the caller owns it
        otherwise.
     */
    for (i = ep->variableMax - 1; i >= 0; i--) {
        if (i > 0 || (ep->flags & FLAGS_VARIABLES)) {
            hashFree(ep->variables[i] - JS_OFFSET);
        }
    }
    wfreeset(ep->variables);
    ep->variableMax = 0;
    if (ep->flags & FLAGS_FUNCTIONS) {
        hashFree(ep->functions);
    }
    jsMax = wfreeHandle(&jsHandles, ep->jid);
    wfree(ep);
}


/*
    Create a new variable scope block so that consecutive jsEval calls may be made with the same variable scope. This
    space MUST be closed with jsCloseBlock when the evaluations are complete.
 */
PUBLIC int jsOpenBlock(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return pushFrame(ep, hashCreate(64));
}


/*
    Close a variable scope block and any block opened after it. The global frame cannot be closed here, it is owned
    by the engine and is released by jsCloseEngine.
 */
PUBLIC int jsCloseBlock(int jid, int vid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL || vid <= 0 || vid >= ep->variableMax) {
        return -1;
    }
    while (ep->variableMax > vid) {
        hashFree(ep->variables[--ep->variableMax] - JS_OFFSET);
    }
    return 0;
}


/*
    Create a new variable scope block and evaluate a script. All variables
    created during this context will be automatically deleted when complete.
 */
PUBLIC char *jsEvalBlock(int jid, cchar *script, char **emsg)
{
    char *returnVal;
    int  vid;

    assert(script);

    vid = jsOpenBlock(jid);
    returnVal = jsEval(jid, script, emsg);
    jsCloseBlock(jid, vid);
    return returnVal;
}


/*
    Parse and evaluate Javascript.
 */
PUBLIC char *jsEval(int jid, cchar *script, char **emsg)
{
    Js      *ep;
    JsInput *oldBlock;
    int     state;
    void    *endlessLoopTest;
    int     loopCounter;


    assert(script);

    if (emsg) {
        *emsg = NULL;
    }
    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    setString(&ep->result, "");

    /*
        Allocate a new evaluation block, and save the old one
     */
    oldBlock = ep->input;
    if (!oldBlock) {
        ep->steps = 0;
    }
    jsLexOpenScript(ep, script);

    /*
        Do the actual parsing and evaluation
     */
    loopCounter = 0;
    endlessLoopTest = NULL;

    do {
        state = parse(ep, STATE_BEGIN, FLAGS_EXE);

        if (state == STATE_RET) {
            state = STATE_EOF;
        }
        /*
            prevent parser from going into infinite loop.  If parsing the same line 10 times then fail and report Syntax
            error.  Most normal error are caught in the parser itself.
         */
        if (endlessLoopTest == ep->input->script.servp) {
            if (loopCounter++ > 10) {
                state = STATE_ERR;
                jsError(ep, "Syntax error");
            }
        } else {
            endlessLoopTest = ep->input->script.servp;
            loopCounter = 0;
        }
    } while (state != STATE_EOF && state != STATE_ERR);

    jsLexCloseScript(ep);

    /*
        Return any error string to the user
     */
    if (state == STATE_ERR && emsg) {
        *emsg = sclone(ep->error);
    }

    /*
        Restore the old evaluation block
     */
    ep->input = oldBlock;

    if (state == STATE_EOF) {
        return ep->result;
    }
    if (state == STATE_ERR) {
        return NULL;
    }
    return ep->result;
}



/*
    Recursive descent parser for Javascript
 */
static int parse(Js *ep, int state, int flags)
{
    assert(ep);

    if (ep->steps >= ME_GOAHEAD_LIMIT_SCRIPT_STEPS) {
        jsError(ep, "Script step limit exceeded");
        return STATE_ERR;
    }
    ep->steps++;
    ep->recursionDepth++;
    if (ep->recursionDepth > ME_GOAHEAD_LIMIT_SCRIPT_RECURSE) {
        jsError(ep, "Script recursion too deep");
        ep->recursionDepth--;
        return STATE_ERR;
    }
    switch (state) {
    /*
        Any statement, function arguments or conditional expressions
     */
    case STATE_STMT:
        if ((state = parseStmt(ep, state, flags)) != STATE_STMT_DONE &&
            state != STATE_EOF && state != STATE_STMT_BLOCK_DONE &&
            state != STATE_RET) {
            state = STATE_ERR;
        }
        break;

    case STATE_DEC:
        if ((state = parseStmt(ep, state, flags)) != STATE_DEC_DONE &&
            state != STATE_EOF) {
            state = STATE_ERR;
        }
        break;

    case STATE_EXPR:
        if ((state = parseStmt(ep, state, flags)) != STATE_EXPR_DONE &&
            state != STATE_EOF) {
            state = STATE_ERR;
        }
        break;

    /*
        Variable declaration list
     */
    case STATE_DEC_LIST:
        state = parseDeclaration(ep, state, flags);
        break;

    /*
        Function argument string
     */
    case STATE_ARG_LIST:
        state = parseFunctionArgs(ep, state, flags);
        break;

    /*
        Logical condition list (relational operations separated by &&, ||)
     */
    case STATE_COND:
        state = parseCond(ep, state, flags);
        break;

    /*
       j  Expression list
     */
    case STATE_RELEXP:
        state = parseExpr(ep, state, flags);
        break;
    }

    if (state == STATE_ERR && ep->error == NULL) {
        jsError(ep, "Syntax error");
    }
    ep->recursionDepth--;
    return state;
}


/*
    Parse any statement including functions and simple relational operations
 */
static int parseStmt(Js *ep, int state, int flags)
{
    JsFun  func;
    JsFun  *saveFunc;
    JsLoop *loop;
    cchar  *value;
    char   *identifier;
    bool   cond, truth;
    int    done, expectSemi, thenFlags, elseFlags, tid, forFlags;
    int    jsVarType;

    assert(ep);

    loop = NULL;
    expectSemi = 0;
    saveFunc = NULL;

    for (done = 0; !done; ) {
        tid = jsLexGetToken(ep, state);

        switch (tid) {
        default:
            if (state == STATE_STMT) {
                goto error;
            }
            jsLexPutbackToken(ep, TOK_EXPR, ep->token);
            done++;
            break;

        case TOK_ERR:
            state = STATE_ERR;
            done++;
            break;

        case TOK_EOF:
            state = STATE_EOF;
            done++;
            break;

        case TOK_NEWLINE:
            break;

        case TOK_SEMI:
            /*
                This case is when we discover no statement and just a lone ';'
             */
            if (state != STATE_STMT) {
                jsLexPutbackToken(ep, tid, ep->token);
            }
            done++;
            break;

        case TOK_ID:
            /*
                This could either be a reference to a variable or an assignment
             */
            identifier = NULL;
            setString(&identifier, ep->token);
            /*
                Peek ahead to see if this is an assignment
             */
            tid = jsLexGetToken(ep, state);
            if (tid == TOK_ASSIGNMENT) {
                if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                    clearString(&identifier);
                    goto error;
                }
                if (flags & FLAGS_EXE) {
                    if ( state == STATE_DEC ) {
                        jsSetLocalVar(ep->jid, identifier, ep->result);
                    } else {
                        jsVarType = jsGetVar(ep->jid, identifier, &value);
                        if (jsVarType > 0) {
                            jsSetLocalVar(ep->jid, identifier, ep->result);
                        } else {
                            jsSetGlobalVar(ep->jid, identifier, ep->result);
                        }
                    }
                }

            } else if (tid == TOK_INC_DEC ) {
                value = NULL;
                if (flags & FLAGS_EXE) {
                    jsVarType = jsGetVar(ep->jid, identifier, &value);
                    if (jsVarType < 0 || !value) {
                        jsError(ep, "Undefined variable %s\n", identifier);
                        clearString(&identifier);
                        goto error;
                    }
                    setString(&ep->result, value);
                    if (evalExpr(ep, value, (int) *ep->token, "1") < 0) {
                        clearString(&identifier);
                        state = STATE_ERR;
                        break;
                    }
                    if (jsVarType > 0) {
                        jsSetLocalVar(ep->jid, identifier, ep->result);
                    } else {
                        jsSetGlobalVar(ep->jid, identifier, ep->result);
                    }
                }

            } else {
                /*
                    If we are processing a declaration, allow undefined vars
                 */
                value = NULL;
                if (state == STATE_DEC) {
                    if (jsGetVar(ep->jid, identifier, &value) > 0) {
                        jsError(ep, "Variable already declared: %s", identifier);
                        clearString(&identifier);
                        goto error;
                    }
                    jsSetLocalVar(ep->jid, identifier, NULL);
                    /*
                        jsGetVar may have returned a value owned by an outer frame. Declaring the variable can
                        replace and free that value, so it must not be used here. A declaration yields undefined.
                     */
                    value = NULL;
                } else {
                    if (flags & FLAGS_EXE) {
                        if (jsGetVar(ep->jid, identifier, &value) < 0) {
                            jsError(ep, "Undefined variable %s\n", identifier);
                            clearString(&identifier);
                            goto error;
                        }
                    }
                }
                setString(&ep->result, value);
                jsLexPutbackToken(ep, tid, ep->token);
            }
            clearString(&identifier);

            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_LITERAL:
            /*
                Set the result to the literal (number or string constant)
             */
            setString(&ep->result, ep->token);
            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_FUNCTION:
            /*
                We must save any current ep->func value for the current stack frame
             */
            if (ep->func) {
                saveFunc = ep->func;
            }
            memset(&func, 0, sizeof(JsFun));
            setString(&func.fname, ep->token);
            ep->func = &func;

            setString(&ep->result, "");
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                freeFunc(&func);
                ep->func = saveFunc;
                goto error;
            }
            if (parse(ep, STATE_ARG_LIST, flags) != STATE_ARG_LIST_DONE) {
                freeFunc(&func);
                ep->func = saveFunc;
                goto error;
            }
            /*
                Evaluate the function if required
             */
            if (flags & FLAGS_EXE && evalFunction(ep) < 0) {
                freeFunc(&func);
                ep->func = saveFunc;
                goto error;
            }
            freeFunc(&func);
            ep->func = saveFunc;

            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }
            if (state == STATE_STMT) {
                expectSemi++;
            }
            done++;
            break;

        case TOK_IF:
            if (state != STATE_STMT) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                goto error;
            }
            /*
                Evaluate the entire condition list "(condition)"
             */
            if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }
            /*
                This is the "then" case. We need to always parse both cases and execute only the relevant case.
             */
            if (isTrue(ep, ep->result, flags, &truth) < 0) {
                goto error;
            }
            if (truth) {
                thenFlags = flags;
                elseFlags = flags & ~FLAGS_EXE;
            } else {
                thenFlags = flags & ~FLAGS_EXE;
                elseFlags = flags;
            }
            /*
                Process the "then" case.  Allow for RETURN statement
             */
            switch (parse(ep, STATE_STMT, thenFlags)) {
            case STATE_RET:
                return STATE_RET;
            case STATE_STMT_DONE:
                break;
            default:
                goto error;
            }
            /*
                check to see if there is an "else" case
             */
            jsRemoveNewlines(ep, state);
            tid = jsLexGetToken(ep, state);
            if (tid != TOK_ELSE) {
                jsLexPutbackToken(ep, tid, ep->token);
                done++;
                break;
            }
            /*
                Process the "else" case.  Allow for return.
             */
            switch (parse(ep, STATE_STMT, elseFlags)) {
            case STATE_RET:
                return STATE_RET;
            case STATE_STMT_DONE:
                break;
            default:
                goto error;
            }
            done++;
            break;

        case TOK_FOR:
            /*
                Format for the expression is:
                    for (initial; condition; incr) {
                        body;
                    }
             */
            if (state != STATE_STMT) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_LPAREN) {
                goto error;
            }

            /*
                Evaluate the for loop initialization statement
             */
            if (parse(ep, STATE_EXPR, flags) != STATE_EXPR_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_SEMI) {
                goto error;
            }

            /*
                The first time through, we save the current input context just to each step: prior to the conditional,
                the loop increment and the loop body.
             */
            if (loop == NULL) {
                loop = walloc(sizeof(JsLoop));
                memset(loop, 0, sizeof(JsLoop));
            }
            jsLexSaveInputState(ep, &loop->cond);
            if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                goto error;
            }
            if (isTrue(ep, ep->result, flags, &cond) < 0) {
                goto error;
            }

            if (jsLexGetToken(ep, state) != TOK_SEMI) {
                goto error;
            }

            /*
                Don't execute the loop increment statement or the body first time
             */
            forFlags = flags & ~FLAGS_EXE;
            jsLexSaveInputState(ep, &loop->incr);
            if (parse(ep, STATE_EXPR, forFlags) != STATE_EXPR_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                goto error;
            }

            /*
                Parse the body and remember the end of the body script
             */
            jsLexSaveInputState(ep, &loop->body);
            if (parse(ep, STATE_STMT, forFlags) != STATE_STMT_DONE) {
                goto error;
            }
            jsLexSaveInputState(ep, &loop->end);

            /*
                Now actually do the for loop. Note loop has been rotated
             */
            while (cond && (flags & FLAGS_EXE) ) {
                /*
                    Evaluate the body
                 */
                jsLexRestoreInputState(ep, &loop->body);

                switch (parse(ep, STATE_STMT, flags)) {
                case STATE_RET:
                    state = STATE_RET;
                    goto doneParse;
                case STATE_STMT_DONE:
                    break;
                default:
                    goto error;
                }
                /*
                    Evaluate the increment script
                 */
                jsLexRestoreInputState(ep, &loop->incr);
                if (parse(ep, STATE_EXPR, flags) != STATE_EXPR_DONE) {
                    goto error;
                }
                /*
                    Evaluate the condition
                 */
                jsLexRestoreInputState(ep, &loop->cond);
                if (parse(ep, STATE_COND, flags) != STATE_COND_DONE) {
                    goto error;
                }
                if (isTrue(ep, ep->result, flags, &cond) < 0) {
                    goto error;
                }
            }
            jsLexRestoreInputState(ep, &loop->end);
            done++;
            break;

        case TOK_VAR:
            if (parse(ep, STATE_DEC_LIST, flags) != STATE_DEC_LIST_DONE) {
                goto error;
            }
            done++;
            break;

        case TOK_COMMA:
            jsLexPutbackToken(ep, TOK_EXPR, ep->token);
            done++;
            break;

        case TOK_LPAREN:
            if (state == STATE_EXPR) {
                if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                    goto error;
                }
                if (jsLexGetToken(ep, state) != TOK_RPAREN) {
                    goto error;
                }
                return STATE_EXPR_DONE;
            }
            done++;
            break;

        case TOK_RPAREN:
            jsLexPutbackToken(ep, tid, ep->token);
            return STATE_EXPR_DONE;

        case TOK_LBRACE:
            /*
                This handles any code in braces except "if () {} else {}"
             */
            if (state != STATE_STMT) {
                goto error;
            }

            /*
                Parse will return STATE_STMT_BLOCK_DONE when the RBRACE is seen
             */
            do {
                state = parse(ep, STATE_STMT, flags);
            } while (state == STATE_STMT_DONE);

            /*
                Allow return statement.
             */
            if (state == STATE_RET) {
                return state;
            }

            if (state != STATE_STMT_BLOCK_DONE) {
                goto error;
            }
            if (jsLexGetToken(ep, state) != TOK_RBRACE) {
                goto error;
            }
            return STATE_STMT_DONE;

        case TOK_RBRACE:
            if (state == STATE_STMT) {
                jsLexPutbackToken(ep, tid, ep->token);
                return STATE_STMT_BLOCK_DONE;
            }
            goto error;

        case TOK_RETURN:
            if (parse(ep, STATE_RELEXP, flags) != STATE_RELEXP_DONE) {
                goto error;
            }
            if (flags & FLAGS_EXE) {
                while ( jsLexGetToken(ep, state) != TOK_EOF );
                done++;
                return STATE_RET;
            }
            break;
        }
    }

    if (expectSemi) {
        tid = jsLexGetToken(ep, state);
        if (tid != TOK_SEMI && tid != TOK_NEWLINE) {
            goto error;
        }
        /*
            Skip newline after semi-colon
         */
        jsRemoveNewlines(ep, state);
    }

doneParse:
    if (loop) {
        jsLexFreeInputState(ep, &loop->cond);
        jsLexFreeInputState(ep, &loop->incr);
        jsLexFreeInputState(ep, &loop->end);
        jsLexFreeInputState(ep, &loop->body);
        wfree(loop);
    }
    if (state == STATE_STMT) {
        return STATE_STMT_DONE;
    } else if (state == STATE_DEC) {
        return STATE_DEC_DONE;
    } else if (state == STATE_EXPR) {
        return STATE_EXPR_DONE;
    } else if (state == STATE_EOF || state == STATE_RET) {
        return state;
    } else {
        return STATE_ERR;
    }

error:
    state = STATE_ERR;
    goto doneParse;
}


/*
    Parse variable declaration list
 */
static int parseDeclaration(Js *ep, int state, int flags)
{
    int tid;

    assert(ep);

    /*
        Declarations can be of the following forms:
                var x;
                var x, y, z;
                var x = 1 + 2 / 3, y = 2 + 4;

        We set the variable to NULL if there is no associated assignment.
     */
    do {
        if ((tid = jsLexGetToken(ep, state)) != TOK_ID) {
            return STATE_ERR;
        }
        jsLexPutbackToken(ep, tid, ep->token);

        /*
            Parse the entire assignment or simple identifier declaration
         */
        if (parse(ep, STATE_DEC, flags) != STATE_DEC_DONE) {
            return STATE_ERR;
        }

        /*
            Peek at the next token, continue if comma seen
         */
        tid = jsLexGetToken(ep, state);
        if (tid == TOK_SEMI) {
            return STATE_DEC_LIST_DONE;
        } else if (tid != TOK_COMMA) {
            return STATE_ERR;
        }
    } while (tid == TOK_COMMA);

    if (tid != TOK_SEMI) {
        return STATE_ERR;
    }
    return STATE_DEC_LIST_DONE;
}


/*
    Parse function arguments
 */
static int parseFunctionArgs(Js *ep, int state, int flags)
{
    int tid, aid;

    assert(ep);

    do {
        state = parse(ep, STATE_RELEXP, flags);
        if (state == STATE_EOF || state == STATE_ERR) {
            return state;
        }
        if (state == STATE_RELEXP_DONE) {
            aid = wallocHandle(&ep->func->args);
            ep->func->args[aid] = sclone(ep->result);
            ep->func->nArgs++;
        }
        /*
            Peek at the next token, continue if more args (ie. comma seen)
         */
        tid = jsLexGetToken(ep, state);
        if (tid != TOK_COMMA) {
            jsLexPutbackToken(ep, tid, ep->token);
        }
    } while (tid == TOK_COMMA);

    if (tid != TOK_RPAREN && state != STATE_RELEXP_DONE) {
        return STATE_ERR;
    }
    return STATE_ARG_LIST_DONE;
}


/*
    Parse conditional expression (relational ops separated by ||, &&)
 */
static int parseCond(Js *ep, int state, int flags)
{
    char *lhs, *rhs;
    int  tid, operator;

    assert(ep);

    setString(&ep->result, "");
    rhs = lhs = NULL;
    operator = 0;

    do {
        /*
            Recurse to handle one side of a conditional. Accumulate the left hand side and the final result in
               ep->result.
         */
        state = parse(ep, STATE_RELEXP, flags);
        if (state != STATE_RELEXP_DONE) {
            state = STATE_ERR;
            break;
        }

        if (operator > 0) {
            setString(&rhs, ep->result);
            if (evalCond(ep, lhs, operator, rhs, flags) < 0) {
                state = STATE_ERR;
                break;
            }
        }
        setString(&lhs, ep->result);

        tid = jsLexGetToken(ep, state);
        if (tid == TOK_LOGICAL) {
            operator = (int) *ep->token;

        } else if (tid == TOK_RPAREN || tid == TOK_SEMI) {
            jsLexPutbackToken(ep, tid, ep->token);
            state = STATE_COND_DONE;
            break;

        } else {
            jsLexPutbackToken(ep, tid, ep->token);
        }

    } while (state == STATE_RELEXP_DONE);

    if (lhs) {
        wfree(lhs);
    }
    if (rhs) {
        wfree(rhs);
    }
    return state;
}


/*
    Parse expression (leftHandSide operator rightHandSide)
 */
static int parseExpr(Js *ep, int state, int flags)
{
    char *lhs, *rhs;
    int  rel, tid;

    assert(ep);

    setString(&ep->result, "");
    rhs = lhs = NULL;
    rel = 0;
    tid = 0;

    do {
        /*
            This loop will handle an entire expression list. We call parse to evaluate each term which returns the
            result in ep->result.
         */
        if (tid == TOK_LOGICAL) {
            if ((state = parse(ep, STATE_RELEXP, flags)) != STATE_RELEXP_DONE) {
                state = STATE_ERR;
                break;
            }
        } else {
            if ((state = parse(ep, STATE_EXPR, flags)) != STATE_EXPR_DONE) {
                state = STATE_ERR;
                break;
            }
        }

        if (rel > 0) {
            setString(&rhs, ep->result);
            if (tid == TOK_LOGICAL) {
                if (evalCond(ep, lhs, rel, rhs, flags) < 0) {
                    state = STATE_ERR;
                    break;
                }
            } else {
                if (evalExpr(ep, lhs, rel, rhs) < 0) {
                    state = STATE_ERR;
                    break;
                }
            }
        }
        setString(&lhs, ep->result);

        if ((tid = jsLexGetToken(ep, state)) == TOK_EXPR ||
            tid == TOK_INC_DEC || tid == TOK_LOGICAL) {
            rel = (int) *ep->token;

        } else {
            jsLexPutbackToken(ep, tid, ep->token);
            state = STATE_RELEXP_DONE;
        }

    } while (state == STATE_EXPR_DONE);

    if (rhs) {
        wfree(rhs);
    }
    if (lhs) {
        wfree(lhs);
    }
    return state;
}


/*
    Convert a numeric operand without invoking overflow in the C library conversion.
 */
static int parseInteger(Js *ep, cchar *value, int *result)
{
    char *end;
    long number;

    if (!*value) {
        *result = 0;
        return 0;
    }
    errno = 0;
    number = strtol(value, &end, 10);
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    if (errno == ERANGE || *end || number < INT_MIN || number > INT_MAX) {
        jsError(ep, "Integer operand out of range");
        return -1;
    }
    *result = (int) number;
    return 0;
}


/*
    True when value is a number: optional spaces, an optional sign, digits, and optional spaces
 */
static bool isNumber(cchar *value)
{
    cchar *cp;

    for (cp = value; *cp == ' ' || *cp == '\t'; cp++) { }
    if (*cp == '+' || *cp == '-') {
        cp++;
    }
    if (!isdigit((uchar) *cp)) {
        return 0;
    }
    while (isdigit((uchar) *cp)) {
        cp++;
    }
    while (*cp == ' ' || *cp == '\t') {
        cp++;
    }
    return *cp == '\0';
}


/*
    Conditions accept empty as false and nonzero numbers as true. Other values fail; skipped branches are not evaluated.
 */
static int isTrue(Js *ep, cchar *value, int flags, bool *truth)
{
    int number;

    *truth = 0;
    if (!(flags & FLAGS_EXE) || !value || !*value) {
        return 0;
    }
    if (!isNumber(value)) {
        jsError(ep, "Condition must be numeric: %s", value);
        return -1;
    }
    if (parseInteger(ep, value, &number) < 0) {
        return -1;
    }
    *truth = number != 0;
    return 0;
}



static int evalCond(Js *ep, cchar *lhs, int rel, cchar *rhs, int flags)
{
    char buf[16];
    bool l, r;
    int  lval;

    assert(lhs);
    assert(rhs);
    assert(rel > 0);

    if (isTrue(ep, lhs, flags, &l) < 0 || isTrue(ep, rhs, flags, &r) < 0) {
        return -1;
    }
    switch (rel) {
    case COND_AND:
        lval = l && r;
        break;
    case COND_OR:
        lval = l || r;
        break;
    default:
        jsError(ep, "Bad operator %d", rel);
        return -1;
    }
    itosbuf(buf, sizeof(buf), lval, 10);
    setString(&ep->result, buf);
    return 0;
}


/*
    Evaluate an operation
 */
static int evalExpr(Js *ep, cchar *lhs, int rel, cchar *rhs)
{
    char  buf[16];
    bool  lnum, rnum;
    int   numeric, l, r, lval;
    int64 wide;

    assert(lhs);
    assert(rhs);
    assert(rel > 0);

    lval = 0;
    /*
        Compare numeric operands numerically; empty values act as zero beside numbers. Reject number/non-number
        comparisons to prevent string ordering from bypassing numeric bounds.
     */
    lnum = isNumber(lhs);
    rnum = isNumber(rhs);
    numeric = (lnum || !*lhs) && (rnum || !*rhs);
    if (!numeric && (lnum || rnum) && rel != EXPR_PLUS) {
        jsError(ep, "Operand is not a number: %s", lnum ? rhs : lhs);
        return -1;
    }
    if (numeric) {
        if (parseInteger(ep, lhs, &l) < 0 || parseInteger(ep, rhs, &r) < 0) {
            return -1;
        }
        wide = 0;
        switch (rel) {
        case EXPR_PLUS:
            wide = (int64) l + r;
            break;
        case EXPR_INC:
            wide = (int64) l + 1;
            break;
        case EXPR_MINUS:
            wide = (int64) l - r;
            break;
        case EXPR_DEC:
            wide = (int64) l - 1;
            break;
        case EXPR_MUL:
            wide = (int64) l * r;
            break;
        case EXPR_DIV:
            wide = r ? (int64) l / r : 0;
            break;
        case EXPR_MOD:
            wide = r ? (int64) l % r : 0;
            break;
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
            if (r < 0 || r >= (int) (sizeof(int) * CHAR_BIT) || l < 0) {
                jsError(ep, "Invalid shift operand");
                return -1;
            }
            if (rel == EXPR_LSHIFT) {
                wide = (int64) l << r;
            } else {
                wide = l >> r;
            }
            break;
        case EXPR_EQ:
            lval = l == r;
            break;
        case EXPR_NOTEQ:
            lval = l != r;
            break;
        case EXPR_LESS:
            lval = (l < r) ? 1 : 0;
            break;
        case EXPR_LESSEQ:
            lval = (l <= r) ? 1 : 0;
            break;
        case EXPR_GREATER:
            lval = (l > r) ? 1 : 0;
            break;
        case EXPR_GREATEREQ:
            lval = (l >= r) ? 1 : 0;
            break;
        case EXPR_BOOL_COMP:
            lval = (r == 0) ? 1 : 0;
            break;
        default:
            jsError(ep, "Bad operator %d", rel);
            return -1;
        }
        if (rel == EXPR_PLUS || rel == EXPR_MINUS || rel == EXPR_MUL || rel == EXPR_DIV || rel == EXPR_MOD ||
            rel == EXPR_INC || rel == EXPR_DEC || rel == EXPR_LSHIFT || rel == EXPR_RSHIFT) {
            if (wide < INT_MIN || wide > INT_MAX) {
                jsError(ep, "Integer result out of range");
                return -1;
            }
            lval = (int) wide;
        }
    } else {
        switch (rel) {
        case EXPR_PLUS:
            clearString(&ep->result);
            if (appendString(ep, &ep->result, lhs) < 0 || appendString(ep, &ep->result, rhs) < 0) {
                return -1;
            }
            return 0;
        case EXPR_LESS:
            lval = strcmp(lhs, rhs) < 0;
            break;
        case EXPR_LESSEQ:
            lval = strcmp(lhs, rhs) <= 0;
            break;
        case EXPR_GREATER:
            lval = strcmp(lhs, rhs) > 0;
            break;
        case EXPR_GREATEREQ:
            lval = strcmp(lhs, rhs) >= 0;
            break;
        case EXPR_EQ:
            lval = strcmp(lhs, rhs) == 0;
            break;
        case EXPR_NOTEQ:
            lval = strcmp(lhs, rhs) != 0;
            break;
        case EXPR_INC:
        case EXPR_DEC:
        case EXPR_MINUS:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        default:
            jsError(ep, "Bad operator");
            return -1;
        }
    }
    itosbuf(buf, sizeof(buf), lval, 10);
    setString(&ep->result, buf);
    return 0;
}


/*
    Callbacks are stored as JsProc despite their registered types; exempt this indirect call from clang's type check.
 */
static JS_NO_SANITIZE_FUNCTION int evalFunction(Js *ep)
{
    WebsKey *sp;

    int (*fn)(int jid, void *handle, int argc, char **argv);

    if ((sp = hashLookup(ep->functions, ep->func->fname)) == NULL) {
        jsError(ep, "Undefined procedure %s", ep->func->fname);
        return -1;
    }
    fn = (int (*)(int, void*, int, char**)) sp->content.value.symbol;
    if (fn == NULL) {
        jsError(ep, "Undefined procedure %s", ep->func->fname);
        return -1;
    }
    return (*fn)(ep->jid, ep->userHandle, ep->func->nArgs, ep->func->args);
}


/*
    Output a parse js_error message
 */
PUBLIC void jsError(Js *ep, cchar *fmt, ...)
{
    va_list args;
    JsInput *ip;
    char    *errbuf, *msgbuf;

    assert(fmt);
    ip = ep ? ep->input : NULL;

    va_start(args, fmt);
    msgbuf = sfmtv(fmt, args);
    va_end(args);

    if (ep && ip) {
        errbuf = sfmt("%s\n At line %d, line => \n\n%s\n", msgbuf, ip->lineNumber, ip->line);
        wfreeset(ep->error);
        ep->error = errbuf;
    } else {
        error("%s", msgbuf);
    }
    wfree(msgbuf);
}


static void clearString(char **ptr)
{
    assert(ptr);

    if (*ptr) {
        wfree(*ptr);
    }
    *ptr = NULL;
}


static void setString(char **ptr, cchar *s)
{
    assert(ptr);

    if (*ptr) {
        wfree(*ptr);
    }
    *ptr = sclone(s);
}


static int appendString(Js *ep, char **ptr, cchar *s)
{
    size_t len, oldlen;

    assert(ptr);

    len = strlen(s);
    oldlen = *ptr ? strlen(*ptr) : 0;
    if (oldlen > ME_GOAHEAD_LIMIT_SCRIPT_RESULT || len > ME_GOAHEAD_LIMIT_SCRIPT_RESULT - oldlen) {
        jsError(ep, "Script result limit exceeded");
        return -1;
    }
    *ptr = wrealloc(*ptr, (ssize) (len + oldlen + 1));
    memcpy(*ptr + oldlen, s, len + 1);
    return 0;
}


/*
    Define a function
 */
PUBLIC int jsSetGlobalFunction(int jid, cchar *name, JsProc fn)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return jsSetGlobalFunctionDirect(ep->functions, name, fn);
}


/*
    Define a function directly into the function symbol table.
 */
PUBLIC int jsSetGlobalFunctionDirect(WebsHash functions, cchar *name, JsProc fn)
{
    if (hashEnter(functions, name, valueSymbol(fn), 0) == NULL) {
        return -1;
    }
    return 0;
}


/*
    Remove ("undefine") a function
 */
PUBLIC int jsRemoveGlobalFunction(int jid, cchar *name)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return hashDelete(ep->functions, name);
}


PUBLIC void *jsGetGlobalFunction(int jid, cchar *name)
{
    Js      *ep;
    WebsKey *sp;

    int (*fn)(int jid, void *handle, int argc, char **argv);

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }

    if ((sp = hashLookup(ep->functions, name)) != NULL) {
        fn = (int (*)(int, void*, int, char**)) sp->content.value.symbol;
        return (void*) fn;
    }
    return NULL;
}


/*
    Utility routine to crack Javascript arguments. Return the number of args
    seen. This routine only supports %s and %d type args.

    Typical usage:

        if (jsArgs(argc, argv, "%s %d", &name, &age) < 2) {
            error("Insufficient args");
            return -1;
        }
 */
PUBLIC int jsArgs(int argc, char **argv, cchar *fmt, ...)
{
    va_list vargs;
    cchar   *cp;
    char    **sp;
    int     *ip;
    int     argn;

    if (argv == NULL) {
        return 0;
    }

    va_start(vargs, fmt);
    for (argn = 0, cp = fmt; cp && *cp && argv[argn]; ) {
        if (*cp++ != '%') {
            continue;
        }
        switch (*cp) {
        case 'd':
            ip = va_arg(vargs, int*);
            *ip = atoi(argv[argn]);
            break;

        case 's':
            sp = va_arg(vargs, char**);
            *sp = argv[argn];
            break;

        default:
            /*
                Unsupported
             */
            assert(0);
        }
        argn++;
    }

    va_end(vargs);
    return argn;
}


PUBLIC void jsSetUserHandle(int jid, void *handle)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    ep->userHandle = handle;
}


void * jsGetUserHandle(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    return ep->userHandle;
}


PUBLIC int jsGetLineNumber(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL || ep->input == NULL) {
        return -1;
    }
    return ep->input->lineNumber;
}


PUBLIC void jsSetResult(int jid, cchar *s)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    setString(&ep->result, s);
}


PUBLIC cchar *jsGetResult(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return NULL;
    }
    return ep->result;
}

/*
    Set a variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in the
    top-most variable frame.
 */
PUBLIC void jsSetVar(int jid, cchar *var, cchar *value)
{
    Js        *ep;
    WebsValue v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[ep->variableMax - 1] - JS_OFFSET, var, v, 0);
}


/*
    Set a local variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in
    the top-most variable frame.
 */
PUBLIC void jsSetLocalVar(int jid, cchar *var, cchar *value)
{
    Js        *ep;
    WebsValue v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[ep->variableMax - 1] - JS_OFFSET, var, v, 0);
}


/*
    Set a global variable. Note: a variable with a value of NULL means declared but undefined. The value is defined in
    the global variable frame.
 */
PUBLIC void jsSetGlobalVar(int jid, cchar *var, cchar *value)
{
    Js        *ep;
    WebsValue v;

    assert(var && *var);

    if ((ep = jsPtr(jid)) == NULL) {
        return;
    }
    if (value == NULL) {
        v = valueString(value, 0);
    } else {
        v = valueString(value, VALUE_ALLOCATE);
    }
    hashEnter(ep->variables[0] - JS_OFFSET, var, v, 0);
}


/*
    Get a variable
 */
PUBLIC int jsGetVar(int jid, cchar *var, cchar **value)
{
    Js      *ep;
    WebsKey *sp;
    int     i;

    assert(var && *var);
    assert(value);

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    i = ep->variableMax - 1;
    if ((sp = hashLookup(ep->variables[i] - JS_OFFSET, var)) == NULL) {
        i = 0;
        if ((sp = hashLookup(ep->variables[0] - JS_OFFSET, var)) == NULL) {
            return -1;
        }
    }
    assert(sp->content.type == string);
    *value = sp->content.value.string;
    return i;
}


WebsHash jsGetVariableTable(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return *ep->variables;
}


WebsHash jsGetFunctionTable(int jid)
{
    Js *ep;

    if ((ep = jsPtr(jid)) == NULL) {
        return -1;
    }
    return ep->functions;
}


/*
    Free an argument list
 */
static void freeFunc(JsFun *func)
{
    char *arg;
    int  i;

    /*
        Free the handle while its slot is occupied; retain the argument because the map may be freed.
     */
    for (i = func->nArgs - 1; i >= 0; i--) {
        arg = func->args[i];
        func->nArgs = wfreeHandle(&func->args, i);
        wfree(arg);
    }
    if (func->fname) {
        wfreeset(func->fname);
    }
}


/*
    Get Javascript engine pointer
 */
static Js *jsPtr(int jid)
{
    assert(0 <= jid && jid < jsMax);

    if (jid < 0 || jid >= jsMax || jsHandles[jid] == NULL) {
        jsError(NULL, "Bad handle %d", jid);
        return NULL;
    }
    return jsHandles[jid];
}


/*
    This function removes any new lines.  Used for else cases, etc.
 */
static void jsRemoveNewlines(Js *ep, int state)
{
    int tid;

    do {
        tid = jsLexGetToken(ep, state);
    } while (tid == TOK_NEWLINE);
    jsLexPutbackToken(ep, tid, ep->token);
}


PUBLIC int jsLexOpen(Js *ep)
{
    return 0;
}


PUBLIC void jsLexClose(Js *ep)
{
}


PUBLIC int jsLexOpenScript(Js *ep, cchar *script)
{
    JsInput *ip;

    assert(ep);
    assert(script);

    if ((ep->input = walloc(sizeof(JsInput))) == NULL) {
        return -1;
    }
    ip = ep->input;
    memset(ip, 0, sizeof(*ip));

    assert(ip);
    assert(ip->putBackToken == NULL);
    assert(ip->putBackTokenId == 0);

    /*
        Create the parse token buffer
     */
    if (bufCreate(&ip->tokbuf, JS_MAX_TOKEN, 0) < 0) {
        return -1;
    }

    /*
        Put the Javascript into a ring queue for easy parsing
     */
    int len = (int) slen(script);
    if (bufCreate(&ip->script, len, 0) < 0) {
        return -1;
    }
    bufPutStr(&ip->script, script);

    ip->lineNumber = 1;
    ip->lineLength = 0;
    ip->lineColumn = 0;
    ip->line = NULL;

    return 0;
}


PUBLIC void jsLexCloseScript(Js *ep)
{
    JsInput *ip;

    assert(ep);

    ip = ep->input;
    assert(ip);

    if (ip->putBackToken) {
        wfreeset(ip->putBackToken);
    }
    ip->putBackTokenId = 0;

    if (ip->line) {
        wfreeset(ip->line);
    }
    bufFree(&ip->tokbuf);
    bufFree(&ip->script);
    wfree(ip);
}


PUBLIC void jsLexSaveInputState(Js *ep, JsInput *state)
{
    JsInput *ip;

    assert(ep);

    ip = ep->input;
    *state = *ip;

    if (ip->putBackToken) {
        state->putBackToken = sclone(ip->putBackToken);
    }
    if (ip->line) {
        //  The line is terminated at lineColumn, but may hold earlier nulls from the script
        state->line = walloc(ip->lineColumn + 1);
        memcpy(state->line, ip->line, ip->lineColumn + 1);
    }
}


PUBLIC void jsLexRestoreInputState(Js *ep, JsInput *state)
{
    JsInput *ip;

    assert(ep);

    ip = ep->input;
    assert(ip);

    ip->tokbuf = state->tokbuf;
    ip->script = state->script;
    ip->putBackTokenId = state->putBackTokenId;
    if (ip->putBackToken) {
        wfreeset(ip->putBackToken);
    }
    if (state->putBackToken) {
        ip->putBackToken = sclone(state->putBackToken);
    }
    ip->lineNumber = state->lineNumber;
    ip->lineColumn = state->lineColumn;
    if (ip->line) {
        memcpy(ip->line, state->line ? state->line : "", state->lineColumn + 1);
    }
}


PUBLIC void jsLexFreeInputState(Js *ep, JsInput *state)
{
    if (state->putBackToken) {
        wfreeset(state->putBackToken);
    }
    wfreeset(state->line);
}


PUBLIC int jsLexGetToken(Js *ep, int state)
{
    ep->tid = getLexicalToken(ep, state);
    return ep->tid;
}


static int getLexicalToken(Js *ep, int state)
{
    WebsBuf *tokq;
    JsInput *ip;
    int     done, tid, c, quote, style;

    assert(ep);
    ip = ep->input;
    assert(ip);

    tokq = &ip->tokbuf;
    ep->tid = -1;
    tid = -1;
    ep->token = "";
    bufFlush(tokq);

    if (ip->putBackTokenId > 0) {
        bufPutStr(tokq, ip->putBackToken);
        tid = ip->putBackTokenId;
        ip->putBackTokenId = 0;
        ep->token = (char*) tokq->servp;
        return tid;
    }
    if ((c = inputGetc(ep)) < 0) {
        return TOK_EOF;
    }
    for (done = 0; !done; ) {
        switch (c) {
        case -1:
            return TOK_EOF;

        case ' ':
        case '\t':
        case '\r':
            do {
                if ((c = inputGetc(ep)) < 0)
                    break;
            } while (c == ' ' || c == '\t' || c == '\r');
            break;

        case '\n':
            return TOK_NEWLINE;

        case '(':
            tokenAddChar(ep, c);
            return TOK_LPAREN;

        case ')':
            tokenAddChar(ep, c);
            return TOK_RPAREN;

        case '{':
            tokenAddChar(ep, c);
            return TOK_LBRACE;

        case '}':
            tokenAddChar(ep, c);
            return TOK_RBRACE;

        case '+':
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '+' ) {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_PLUS);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_INC);
            return TOK_INC_DEC;

        case '-':
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '-' ) {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_MINUS);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_DEC);
            return TOK_INC_DEC;

        case '*':
            tokenAddChar(ep, EXPR_MUL);
            return TOK_EXPR;

        case '%':
            tokenAddChar(ep, EXPR_MOD);
            return TOK_EXPR;

        case '/':
            /*
                Handle the division operator and comments
             */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c != '*' && c != '/') {
                inputPutback(ep, c);
                tokenAddChar(ep, EXPR_DIV);
                return TOK_EXPR;
            }
            style = c;
            /*
                Eat comments. Both C and C++ comment styles are supported.
             */
            while (1) {
                if ((c = inputGetc(ep)) < 0) {
                    jsError(ep, "Syntax Error");
                    return TOK_ERR;
                }
                if (c == '\n' && style == '/') {
                    break;
                } else if (c == '*') {
                    c = inputGetc(ep);
                    if (style == '/') {
                        if (c == '\n') {
                            break;
                        }
                    } else {
                        if (c == '/') {
                            break;
                        }
                    }
                }
            }
            /*
                Continue looking for a token, so get the next character
             */
            if ((c = inputGetc(ep)) < 0) {
                return TOK_EOF;
            }
            break;

        case '<':                                   /* < and <= */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '<') {
                tokenAddChar(ep, EXPR_LSHIFT);
                return TOK_EXPR;
            } else if (c == '=') {
                tokenAddChar(ep, EXPR_LESSEQ);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_LESS);
            inputPutback(ep, c);
            return TOK_EXPR;

        case '>':                                   /* > and >= */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '>') {
                tokenAddChar(ep, EXPR_RSHIFT);
                return TOK_EXPR;
            } else if (c == '=') {
                tokenAddChar(ep, EXPR_GREATEREQ);
                return TOK_EXPR;
            }
            tokenAddChar(ep, EXPR_GREATER);
            inputPutback(ep, c);
            return TOK_EXPR;

        case '=':                                   /* "==" */
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '=') {
                tokenAddChar(ep, EXPR_EQ);
                return TOK_EXPR;
            }
            inputPutback(ep, c);
            return TOK_ASSIGNMENT;

        case '!':                                   /* "!=" or "!"*/
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            if (c == '=') {
                tokenAddChar(ep, EXPR_NOTEQ);
                return TOK_EXPR;
            }
            inputPutback(ep, c);
            tokenAddChar(ep, EXPR_BOOL_COMP);
            return TOK_EXPR;

        case ';':
            tokenAddChar(ep, c);
            return TOK_SEMI;

        case ',':
            tokenAddChar(ep, c);
            return TOK_COMMA;

        case '|':                                   /* "||" */
            if ((c = inputGetc(ep)) < 0 || c != '|') {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            tokenAddChar(ep, COND_OR);
            return TOK_LOGICAL;

        case '&':                                   /* "&&" */
            if ((c = inputGetc(ep)) < 0 || c != '&') {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }
            tokenAddChar(ep, COND_AND);
            return TOK_LOGICAL;

        case '\"':                                  /* String quote */
        case '\'':
            quote = c;
            if ((c = inputGetc(ep)) < 0) {
                jsError(ep, "Syntax Error");
                return TOK_ERR;
            }

            while (c != quote) {
                /*
                    check for escape sequence characters
                 */
                if (c == '\\') {
                    c = inputGetc(ep);

                    if (isdigit((uchar) c)) {
                        /*
                            octal support, \101 maps to 65 = 'A'. put first char back so converter will work properly.
                         */
                        inputPutback(ep, c);
                        c = charConvert(ep, OCTAL, 3);

                    } else {
                        switch (c) {
                        case 'n':
                            c = '\n'; break;
                        case 'b':
                            c = '\b'; break;
                        case 'f':
                            c = '\f'; break;
                        case 'r':
                            c = '\r'; break;
                        case 't':
                            c = '\t'; break;
                        case 'x':
                            /*
                                hex support, \x41 maps to 65 = 'A'
                             */
                            c = charConvert(ep, HEX, 2);
                            break;
                        case 'u':
                            /*
                                unicode support, \x0401 maps to 65 = 'A'
                             */
                            c = charConvert(ep, HEX, 2);
                            c = c * 16 + charConvert(ep, HEX, 2);

                            break;
                        case '\'':
                        case '\"':
                        case '\\':
                            break;
                        default:
                            jsError(ep, "Invalid Escape Sequence");
                            return TOK_ERR;
                        }
                    }
                    if (tokenAddChar(ep, c) < 0) {
                        return TOK_ERR;
                    }
                } else {
                    if (tokenAddChar(ep, c) < 0) {
                        return TOK_ERR;
                    }
                }
                if ((c = inputGetc(ep)) < 0) {
                    jsError(ep, "Unmatched Quote");
                    return TOK_ERR;
                }
            }
            return TOK_LITERAL;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            do {
                if (tokenAddChar(ep, c) < 0) {
                    return TOK_ERR;
                }
                if ((c = inputGetc(ep)) < 0)
                    break;
            } while (isdigit((uchar) c));
            inputPutback(ep, c);
            return TOK_LITERAL;

        default:
            /*
                Identifiers or a function names
             */
            while (1) {
                if (c == '\\') {
                    /*
                        just ignore any \ characters.
                     */
                } else if (tokenAddChar(ep, c) < 0) {
                    break;
                }
                if ((c = inputGetc(ep)) < 0) {
                    break;
                }
                if (!isalnum((uchar) c) && c != '$' && c != '_' && c != '\\') {
                    break;
                }
            }
            if (!isalpha((uchar) *tokq->servp) && *tokq->servp != '$' && *tokq->servp != '_') {
                jsError(ep, "Invalid identifier %s", tokq->servp);
                return TOK_ERR;
            }
            /*
                Check for reserved words (only "if", "else", "var", "for" and "return" at the moment)
             */
            if (state == STATE_STMT) {
                if (strcmp(ep->token, "if") == 0) {
                    return TOK_IF;
                } else if (strcmp(ep->token, "else") == 0) {
                    return TOK_ELSE;
                } else if (strcmp(ep->token, "var") == 0) {
                    return TOK_VAR;
                } else if (strcmp(ep->token, "for") == 0) {
                    return TOK_FOR;
                } else if (strcmp(ep->token, "return") == 0) {
                    if ((c == ';') || (c == '(')) {
                        inputPutback(ep, c);
                    }
                    return TOK_RETURN;
                }
            }
            /*
                Skip white space after token to find out whether this is a function or not.
             */
            while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if ((c = inputGetc(ep)) < 0)
                    break;
            }

            tid = (c == '(') ? TOK_FUNCTION : TOK_ID;
            done++;
        }
    }

    /*
        Putback the last extra character for next time
     */
    inputPutback(ep, c);
    return tid;
}


PUBLIC void jsLexPutbackToken(Js *ep, int tid, cchar *str)
{
    JsInput *ip;

    assert(ep);
    ip = ep->input;
    assert(ip);

    if (ip->putBackToken) {
        wfreeset(ip->putBackToken);
    }
    ip->putBackTokenId = tid;
    ip->putBackToken = sclone(str);
}


static int tokenAddChar(Js *ep, int c)
{
    JsInput *ip;

    assert(ep);
    ip = ep->input;
    assert(ip);

    if (bufPutc(&ip->tokbuf, (char) c) < 0) {
        jsError(ep, "Token too big");
        return -1;
    }
    *((char*) ip->tokbuf.endp) = '\0';
    ep->token = (char*) ip->tokbuf.servp;

    return 0;
}


static int inputGetc(Js *ep)
{
    JsInput *ip;
    ssize   len;
    int     c;

    assert(ep);
    ip = ep->input;

    if ((len = bufLen(&ip->script)) == 0) {
        return -1;
    }
    c = bufGetc(&ip->script);
    if (c == '\n') {
        ip->lineNumber++;
        ip->lineColumn = 0;
    } else {
        if ((ip->lineColumn + 2) >= ip->lineLength) {
            ip->lineLength += JS_INC;
            ip->line = wrealloc(ip->line, ip->lineLength * sizeof(char));
        }
        ip->line[ip->lineColumn++] = (char) c;
        ip->line[ip->lineColumn] = '\0';
    }
    return c;
}


static void inputPutback(Js *ep, int c)
{
    JsInput *ip;

    assert(ep);

    ip = ep->input;
    bufInsertc(&ip->script, (char) c);
    /* Fix by Fred Sauer, 2002/12/23 */
    if (ip->lineColumn > 0) {
        ip->lineColumn--;
    }
    ip->line[ip->lineColumn] = '\0';
}


/*
    Convert a hex or octal character back to binary, return original char if not a hex digit
 */
static int charConvert(Js *ep, int base, int maxDig)
{
    int i, c, lval, convChar;

    lval = 0;
    for (i = 0; i < maxDig; i++) {
        if ((c = inputGetc(ep)) < 0) {
            break;
        }
        /*
            Initialize to out of range value
         */
        convChar = base;
        if (isdigit((uchar) c)) {
            convChar = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            convChar = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            convChar = c - 'A' + 10;
        }
        /*
            if unexpected character then return it to buffer.
         */
        if (convChar >= base) {
            inputPutback(ep, c);
            break;
        }
        lval = (lval * base) + convChar;
    }
    return lval;
}

#endif /* ME_GOAHEAD_JAVASCRIPT */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/jst.c ************/

/*
    jst.c -- JavaScript templates

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"
#include    "js.h"

#if ME_GOAHEAD_JAVASCRIPT
/********************************** Locals ************************************/

static WebsHash websJstFunctions = -1;  /* Symbol table of functions */

/***************************** Forward Declarations ***************************/

static char *strtokcmp(char *s1, char *s2);
static char *skipWhite(char *s);
static int jstEscape(int jid, Webs *wp, int argc, char **argv);
static int jstEscapeUri(int jid, Webs *wp, int argc, char **argv);
static int jstEscapeJs(int jid, Webs *wp, int argc, char **argv);
static int jstWriteHtml(int jid, Webs *wp, int argc, char **argv);

/************************************* Code ***********************************/
/*
    Keep template source and interpreter details out of client responses. A native callback
    may already have finalized an error response; never append another response in that case.
 */
static void jstError(Webs *wp)
{
    if (!wp->finalized) {
        if (wp->flags & WEBS_HEADERS_CREATED) {
            websWrite(wp, "<h2>Javascript Error</h2>\n");
        } else {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Javascript Error");
        }
    }
}


/*
    Process requests and expand all scripting commands. We read the entire web page into memory and then process. If
    you have really big documents, it is better to make them plain HTML files rather than Javascript web pages.
    Return true to indicate the request was handled, even for errors.
 */
static bool jstHandler(Webs *wp)
{
    WebsFileInfo sbuf;
    char         *lang, *token, *result, *ep, *cp, *buf, *nextp, *last;
    ssize        len;
    int          rc, jid, vid;

    assert(websValid(wp));
    assert(wp->filename && *wp->filename);
    assert(wp->ext && *wp->ext);

    buf = 0;
    vid = -1;
    if ((jid = jsOpenEngine(wp->vars, websJstFunctions)) < 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create JavaScript engine");
        goto done;
    }
    /*
        The engine's global variable frame is the request variable table (wp->vars) which owns its values.
        Run the page script in a variable frame of its own so that script declarations shadow request query
        and form variables instead of replacing and freeing them. Reads still fall back to the global frame
        and assignments to undeclared names still define request variables, so templates are unaffected.
        The frame spans all the <% %> blocks in the page as they share the one engine.
     */
    if ((vid = jsOpenBlock(jid)) < 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create JavaScript variable scope");
        goto done;
    }
    jsSetUserHandle(jid, wp);

    if (websPageStat(wp, &sbuf) < 0) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot stat %s", wp->filename);
        goto done;
    }
    if (websPageOpen(wp, O_RDONLY | O_BINARY, 0666) < 0) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot open URL: %s", wp->filename);
        goto done;
    }
    /*
        Create a buffer to hold the web page in-memory
     */
    if (sbuf.size >= MAXSSIZE) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Template too large: %s", wp->filename);
        goto done;
    }
    len = (ssize) sbuf.size;
    if ((buf = walloc(len + 1)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get memory");
        goto done;
    }
    buf[len] = '\0';

    if (websPageReadData(wp, buf, len) != len) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot read %s", wp->filename);
        goto done;
    }
    websPageClose(wp);
    wp->flags |= WEBS_JST_HEADERS;

    /*
        Scan for the next "<%"
     */
    last = buf;
    for (rc = 0; rc == 0 && *last && ((nextp = strstr(last, "<%")) != NULL); ) {
        websWriteBlock(wp, last, (nextp - last));
        nextp = skipWhite(nextp + 2);
        /*
            Decode the language
         */
        token = "language";
        if ((lang = strtokcmp(nextp, token)) != NULL) {
            if ((cp = strtokcmp(lang, "=javascript")) != NULL) {
                /* Ignore */;
            } else {
                cp = nextp;
            }
            nextp = cp;
        }

        /*
            Find tailing bracket and then evaluate the script
         */
        if ((ep = strstr(nextp, "%>")) != NULL) {

            *ep = '\0';
            last = ep + 2;
            nextp = skipWhite(nextp);
            /*
                Handle backquoted newlines
             */
            for (cp = nextp; *cp; ) {
                if (*cp == '\\' && (cp[1] == '\r' || cp[1] == '\n')) {
                    *cp++ = ' ';
                    while (*cp == '\r' || *cp == '\n') {
                        *cp++ = ' ';
                    }
                } else {
                    cp++;
                }
            }
            if (*nextp) {
                result = NULL;
                /*
                    SECURITY Acceptable: executing embedded <% %> JavaScript from .jst templates is the
                    purpose of the JST engine. Templates are trusted operator-supplied content served from
                    the document root; there is no default remote write primitive (routes enforce methods and
                    PUT/DELETE are not enabled by default), and filesystem integrity is out of scope.
                 */
                if (jsEval(jid, nextp, &result) == 0) {
                    if (result) {
                        trace(4, "JST evaluation failed: %s", result);
                        wfree(result);
                    }
                    jstError(wp);
                    goto done;
                }
                if (wp->finalized) {
                    goto done;
                }
            }

        } else {
            trace(4, "Unterminated JST script");
            jstError(wp);
            goto done;
        }
    }
    /*
        Output any trailing HTML page text
     */
    if (last && *last && rc == 0) {
        websWriteBlock(wp, last, strlen(last));
    }

/*
    Common exit and cleanup
 */
done:
    /*
        Close the page scope before the engine. The caller retains ownership of the global request variables;
        the engine releases any remaining block frames.
     */
    if (jid >= 0) {
        if (vid >= 0) {
            jsCloseBlock(jid, vid);
        }
        jsCloseEngine(jid);
    }
    if (websValid(wp)) {
        websPageClose(wp);
    }
    websDone(wp);
    wfree(buf);
    return 1;
}


static void closeJst(void)
{
    if (websJstFunctions != -1) {
        hashFree(websJstFunctions);
        websJstFunctions = -1;
    }
}


PUBLIC int websJstOpen(void)
{
    websJstFunctions = hashCreate(WEBS_HASH_INIT * 2);
    websDefineJst("write", websJstWrite);
    websDefineJst("writeHtml", jstWriteHtml);
    websDefineJst("escape", jstEscape);
    websDefineJst("escapeUri", jstEscapeUri);
    websDefineJst("escapeJs", jstEscapeJs);
    websDefineHandler("jst", 0, jstHandler, closeJst, 0);
    return 0;
}


/*
    Define a Javascript function. Bind an Javascript name to a C procedure.
 */
PUBLIC int websDefineJst(cchar *name, WebsJstProc fn)
{
    return jsSetGlobalFunctionDirect(websJstFunctions, name, (JsProc) fn);
}


/*
    Javascript write command. This implements <% write("text"); %> command
 */
PUBLIC int websJstWrite(int jid, Webs *wp, int argc, char **argv)
{
    int i;

    assert(websValid(wp));

    for (i = 0; i < argc; ) {
        assert(argv);
        if (websWriteBlock(wp, argv[i], strlen(argv[i])) < 0) {
            return -1;
        }
        if (++i < argc) {
            if (websWriteBlock(wp, " ", 1) < 0) {
                return -1;
            }
        }
    }
    return 0;
}


/*
    Write HTML-escaped arguments separated by spaces.
 */
static int jstWriteHtml(int jid, Webs *wp, int argc, char **argv)
{
    char *escaped;
    int  i;

    for (i = 0; i < argc; i++) {
        escaped = websEscapeHtml(argv[i]);
        if (websWriteBlock(wp, escaped, slen(escaped)) < 0) {
            wfree(escaped);
            return -1;
        }
        wfree(escaped);
        if (i + 1 < argc && websWriteBlock(wp, " ", 1) < 0) {
            return -1;
        }
    }
    return 0;
}


/*
    Return HTML-escaped text.
 */
static int jstEscape(int jid, Webs *wp, int argc, char **argv)
{
    char *escaped;

    if (argc != 1) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "escape() takes one argument");
        return -1;
    }
    escaped = websEscapeHtml(argv[0]);
    jsSetResult(jid, escaped);
    wfree(escaped);
    return 0;
}


/*
    Encode a whole URI while preserving delimiters. HTML-escape the result when writing attributes.
 */
static int jstEscapeUri(int jid, Webs *wp, int argc, char **argv)
{
    char *escaped;

    if (argc != 1) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "escapeUri() takes one argument");
        return -1;
    }
    escaped = websEscapeUri(argv[0]);
    jsSetResult(jid, escaped);
    wfree(escaped);
    return 0;
}



static int jstEscapeJs(int jid, Webs *wp, int argc, char **argv)
{
    char *escaped;

    if (argc != 1) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "escapeJs() takes one argument");
        return -1;
    }
    escaped = websEscapeJs(argv[0]);
    jsSetResult(jid, escaped);
    wfree(escaped);
    return 0;
}


/*
    Find s2 in s1. We skip leading white space in s1.  Return a pointer to the location in s1 after s2 ends.
 */
static char *strtokcmp(char *s1, char *s2)
{
    ssize len;

    s1 = skipWhite(s1);
    len = strlen(s2);
    for (len = strlen(s2); len > 0 && (tolower((uchar) *s1) == tolower((uchar) *s2)); len--) {
        if (*s2 == '\0') {
            return s1;
        }
        s1++;
        s2++;
    }
    if (len == 0) {
        return s1;
    }
    return NULL;
}


static char *skipWhite(char *s)
{
    assert(s);

    if (s == NULL) {
        return s;
    }
    while (*s && isspace((uchar) *s)) {
        s++;
    }
    return s;
}

#endif /* ME_GOAHEAD_JAVASCRIPT */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/mbedtls.c ************/

/*
    mbedtls.c - MbedTLS interface to GoAhead

    This code requires MbedTLS 3.6.7 or later.

    To build MbedTLS, use:
        git checkout RELEASE-TAG
        cmake -DCMAKE_BUILD_TYPE=Debug .
        make VERBOSE=1

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/************************************ Include *********************************/

#include    "goahead.h"

#if ME_COM_MBEDTLS
    #include "mbedtls/mbedtls_config.h"
    #include "mbedtls/ssl.h"
    #include "mbedtls/ssl_cache.h"
    #include "mbedtls/ssl_ticket.h"
    #include "mbedtls/ctr_drbg.h"
    #include "mbedtls/net_sockets.h"
    #include "psa/crypto.h"
    #include "mbedtls/debug.h"
    #include "mbedtls/error.h"
    #include "mbedtls/check_config.h"

#if MBEDTLS_VERSION_NUMBER < 0x03060700
    #error "GoAhead requires MbedTLS 3.6.7 or later. Earlier releases carry known certificate verification flaws."
#endif

/************************************* Defines ********************************/

typedef struct MbedConfig {
    mbedtls_x509_crt ca;                        /* Certificate authority bundle to verify peer */
    mbedtls_ssl_cache_context cache;            /* Session cache context */
    mbedtls_ssl_config conf;                    /* SSL configuration */
    mbedtls_x509_crt cert;                      /* Certificate (own) */
    mbedtls_ctr_drbg_context ctr;               /* Counter random generator state */
    mbedtls_ssl_ticket_context tickets;         /* Session tickets */
    mbedtls_entropy_context entropy;            /* Entropy context */
    mbedtls_x509_crl revoke;                    /* Certificate authority bundle to verify peer */
    mbedtls_pk_context pkey;                    /* Private key */
    int *ciphers;                               /* Set of acceptable ciphers - null terminated */
    bool ownCert;                               /* Own certificate and key are installed */
} MbedConfig;

/*
    Per socket state
 */
typedef struct MbedSocket {
    mbedtls_ssl_context ctx;                    /* SSL state */
    mbedtls_ssl_session session;                /* SSL sessions */
} MbedSocket;

static MbedConfig cfg;

/*
    GoAhead Log level to start SSL tracing
 */
static int mbedLogLevel = ME_GOAHEAD_SSL_LOG_LEVEL;

/*
    Runtime-settable cert/key paths. Override the compile-time ME_GOAHEAD_SSL_CERTIFICATE /
    ME_GOAHEAD_SSL_KEY macros when websSetSslCertFile / websSetSslKeyFile are called
    before sslOpen(). Needed by test drivers that ship their own cert alongside the binary.
 */
static char *sslCertPath;
static char *sslKeyPath;

/************************************ Forwards ********************************/

static int *getCipherSuite(cchar *ciphers);
static int  mbedHandshake(Webs *wp);
static int  parseCert(mbedtls_x509_crt *cert, char *file);
static int  parseCrl(mbedtls_x509_crl *crl, char *path);
static int  parseKey(mbedtls_pk_context *key, char *path);
static void merror(int rc, PRINTF_FORMAT cchar *fmt, ...) PRINTF_ATTRIBUTE(2, 3);
static char *replaceHyphen(char *cipher, char from, char to);
static void traceMbed(void *context, int level, cchar *file, int line, cchar *str);
static bool sameName(mbedtls_x509_buf *a, mbedtls_x509_buf *b);
static int  verifyChain(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags);

/************************************** Code **********************************/

PUBLIC int sslOpen()
{
    mbedtls_ssl_config           *conf;
    mbedtls_ssl_protocol_version minVersion, maxVersion;
    cuchar                       dhm_p[] = MBEDTLS_DHM_RFC3526_MODP_2048_P_BIN;
    cuchar                       dhm_g[] = MBEDTLS_DHM_RFC3526_MODP_2048_G_BIN;
    char                         *keyPath, *certPath;
    int                          rc;

    trace(7, "Initializing MbedTLS SSL");

    psa_crypto_init();
    mbedtls_entropy_init(&cfg.entropy);

    conf = &cfg.conf;
    mbedtls_ssl_config_init(conf);

    mbedtls_ssl_conf_dbg(conf, traceMbed, NULL);
    if (websGetLogLevel() >= mbedLogLevel) {
        mbedtls_debug_set_threshold(websGetLogLevel() - mbedLogLevel);
    }
    mbedtls_pk_init(&cfg.pkey);
    mbedtls_ssl_cache_init(&cfg.cache);
    mbedtls_ctr_drbg_init(&cfg.ctr);
    mbedtls_x509_crt_init(&cfg.cert);
    mbedtls_ssl_ticket_init(&cfg.tickets);

    if ((rc =
             mbedtls_ctr_drbg_seed(&cfg.ctr, mbedtls_entropy_func, &cfg.entropy, (cuchar*) ME_NAME,
                                   slen(ME_NAME))) < 0) {
        merror(rc, "Cannot seed rng");
        return -1;
    }

    /*
        Set the server certificate and key files. Prefer runtime-provided paths
        (set via websSetSsl{Cert,Key}File) over the compile-time defaults.
     */
    keyPath = sslKeyPath ? sslKeyPath : (char*) ME_GOAHEAD_SSL_KEY;
    certPath = sslCertPath ? sslCertPath : (char*) ME_GOAHEAD_SSL_CERTIFICATE;
    if (keyPath && *keyPath) {
        /*
            Load a decrypted PEM format private key. The last arg is the private key.
         */
        if (parseKey(&cfg.pkey, keyPath) < 0) {
            return -1;
        }
    }
    if (certPath && *certPath) {
        /*
            Load a PEM format certificate file
         */
        if (parseCert(&cfg.cert, certPath) < 0) {
            return -1;
        }
    }
    if (*ME_GOAHEAD_SSL_AUTHORITY) {
        if (parseCert(&cfg.ca, ME_GOAHEAD_SSL_AUTHORITY) != 0) {
            return -1;
        }
    }
    if (*ME_GOAHEAD_SSL_REVOKE) {
        /*
            Load a PEM format certificate file
         */
        if (parseCrl(&cfg.revoke, (char*) ME_GOAHEAD_SSL_REVOKE) != 0) {
            return -1;
        }
    }
    cfg.ciphers = getCipherSuite((ME_GOAHEAD_SSL_CIPHERS && *ME_GOAHEAD_SSL_CIPHERS) ?
                                 ME_GOAHEAD_SSL_CIPHERS : ME_GOAHEAD_SSL_DEFAULT_CIPHERS);
    if (!cfg.ciphers) {
        error("No supported and permitted cipher in the cipher list");
        return -1;
    }

    if ((rc = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) < 0) {
        merror(rc, "Cannot set mbedtls defaults");
        return -1;
    }
    mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &cfg.ctr);

    /*
        Configure larger DH parameters
     */
    if ((rc = mbedtls_ssl_conf_dh_param_bin(conf, dhm_p, sizeof(dhm_p), dhm_g, sizeof(dhm_g))) < 0) {
        merror(rc, "Cannot set DH params");
        return -1;
    }

    /*
        Allow unknown issuers when configured, but still require a client certificate.
     */
    if (!ME_GOAHEAD_SSL_VERIFY_PEER) {
        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
    } else if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    } else {
        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    }
    mbedtls_ssl_conf_verify(conf, verifyChain, NULL);

    /*
        Apply configured protocol versions supported by this MbedTLS build.
     */
    minVersion = ME_GOAHEAD_SSL_NO_V1_2 ? MBEDTLS_SSL_VERSION_TLS1_3 : MBEDTLS_SSL_VERSION_TLS1_2;
    maxVersion = ME_GOAHEAD_SSL_NO_V1_3 ? MBEDTLS_SSL_VERSION_TLS1_2 : MBEDTLS_SSL_VERSION_TLS1_3;
#if !defined(MBEDTLS_SSL_PROTO_TLS1_2)
    minVersion = MBEDTLS_SSL_VERSION_TLS1_3;
#endif
#if !defined(MBEDTLS_SSL_PROTO_TLS1_3)
    maxVersion = MBEDTLS_SSL_VERSION_TLS1_2;
#endif
    if (minVersion > maxVersion) {
        error("No TLS version is enabled: check ME_GOAHEAD_SSL_NO_V1_2, ME_GOAHEAD_SSL_NO_V1_3 and the MbedTLS build");
        return -1;
    }
    mbedtls_ssl_conf_min_tls_version(conf, minVersion);
    mbedtls_ssl_conf_max_tls_version(conf, maxVersion);

    /*
        Disable resumption with client certificates so every connection verifies them.
     */
    if (ME_GOAHEAD_SSL_TICKET && !ME_GOAHEAD_SSL_VERIFY_PEER) {
        if ((rc = mbedtls_ssl_ticket_setup(&cfg.tickets, mbedtls_ctr_drbg_random, &cfg.ctr,
                                           MBEDTLS_CIPHER_AES_256_GCM, ME_GOAHEAD_SSL_TIMEOUT)) < 0) {
            merror(rc, "Cannot setup ticketing sessions");
            return -1;
        }
        mbedtls_ssl_conf_session_tickets_cb(conf, mbedtls_ssl_ticket_write, mbedtls_ssl_ticket_parse, &cfg.tickets);
    }

    /*
        Configure server-side session cache
     */
    if (ME_GOAHEAD_SSL_CACHE && !ME_GOAHEAD_SSL_VERIFY_PEER) {
        mbedtls_ssl_conf_session_cache(conf, &cfg.cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
        mbedtls_ssl_cache_set_max_entries(&cfg.cache, ME_GOAHEAD_SSL_CACHE);
        mbedtls_ssl_cache_set_timeout(&cfg.cache, ME_GOAHEAD_SSL_TIMEOUT);
    }

    mbedtls_ssl_conf_ciphersuites(conf, cfg.ciphers);

    /*
        Configure CA certificate bundle and revocation list
     */
    mbedtls_ssl_conf_ca_chain(conf, *ME_GOAHEAD_SSL_AUTHORITY ? &cfg.ca : NULL,
                              *ME_GOAHEAD_SSL_REVOKE ? &cfg.revoke : NULL);

    /*
        Configure server cert and key
     */
    if (keyPath && *keyPath && certPath && *certPath) {
        if ((rc = mbedtls_pk_check_pair(&cfg.cert.pk, &cfg.pkey, mbedtls_ctr_drbg_random, &cfg.ctr)) != 0) {
            merror(rc, "Private key %s does not match certificate %s", keyPath, certPath);
            return -1;
        }
        if ((rc = mbedtls_ssl_conf_own_cert(conf, &cfg.cert, &cfg.pkey)) < 0) {
            merror(rc, "Cannot define certificate and private key");
            return -1;
        }
        cfg.ownCert = 1;
    }
    if (websGetLogLevel() >= 5) {
        char cipher[80];
        cint *cp;
        trace(5, "mbedtls: Supported Ciphers");
        for (cp = mbedtls_ssl_list_ciphersuites(); *cp; cp++) {
            scopy(cipher, sizeof(cipher), (char*) mbedtls_ssl_get_ciphersuite_name(*cp));
            replaceHyphen(cipher, '-', '_');
            trace(5, "mbedtls: %s (0x%04X)", cipher, *cp);
        }
    }
    return 0;
}


/*
    Enforce runtime chain depth. With revocation enabled, require each issuer's CRL; MbedTLS skips missing CRLs.
 */
static int verifyChain(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
    mbedtls_x509_crl *crl;

    if (depth > ME_GOAHEAD_SSL_VERIFY_DEPTH) {
        *flags |= MBEDTLS_X509_BADCERT_OTHER;
    }
    if (crt && *ME_GOAHEAD_SSL_REVOKE && !sameName(&crt->subject_raw, &crt->issuer_raw)) {
        for (crl = &cfg.revoke; crl && crl->version; crl = crl->next) {
            if (sameName(&crl->issuer_raw, &crt->issuer_raw)) {
                return 0;
            }
        }
        logmsg(2, "No revocation list for issuer of certificate at depth %d", depth);
        *flags |= MBEDTLS_X509_BADCRL_NOT_TRUSTED;
    }
    return 0;
}


static bool sameName(mbedtls_x509_buf *a, mbedtls_x509_buf *b)
{
    return a->len == b->len && memcmp(a->p, b->p, a->len) == 0;
}


PUBLIC void sslClose()
{
    mbedtls_ctr_drbg_free(&cfg.ctr);
    mbedtls_pk_free(&cfg.pkey);
    mbedtls_x509_crt_free(&cfg.cert);
    mbedtls_x509_crt_free(&cfg.ca);
    mbedtls_x509_crl_free(&cfg.revoke);
    mbedtls_ssl_cache_free(&cfg.cache);
    mbedtls_ssl_ticket_free(&cfg.tickets);
    mbedtls_ssl_config_free(&cfg.conf);
    mbedtls_entropy_free(&cfg.entropy);
    mbedtls_psa_crypto_free();
    wfreeset(cfg.ciphers);
    cfg.ownCert = 0;
    wfree(sslCertPath);
    sslCertPath = NULL;
    wfree(sslKeyPath);
    sslKeyPath = NULL;
}


PUBLIC bool sslHasCertificate(void)
{
    return cfg.ownCert;
}


/*
    Runtime override for the server certificate path. Must be called before sslOpen().
 */
PUBLIC void websSetSslCertFile(cchar *path)
{
    wfree(sslCertPath);
    sslCertPath = path ? sclone(path) : NULL;
}


/*
    Runtime override for the server key path. Must be called before sslOpen().
 */
PUBLIC void websSetSslKeyFile(cchar *path)
{
    wfree(sslKeyPath);
    sslKeyPath = path ? sclone(path) : NULL;
}


PUBLIC int sslUpgrade(Webs *wp)
{
    MbedSocket          *mb;
    WebsSocket          *sp;
    mbedtls_ssl_context *ctx;
    int                 rc;

    assert(wp);
    if ((mb = walloc(sizeof(MbedSocket))) == 0) {
        return -1;
    }
    memset(mb, 0, sizeof(MbedSocket));
    wp->ssl = mb;
    sp = socketPtr(wp->sid);
    ctx = &mb->ctx;

    mbedtls_ssl_init(ctx);
    if ((rc = mbedtls_ssl_setup(ctx, &cfg.conf)) != 0) {
        error("Cannot configure TLS connection: %d", rc);
        mbedtls_ssl_free(ctx);
        wfreeset(wp->ssl);
        return -1;
    }
    mbedtls_ssl_set_bio(ctx, &sp->sock, mbedtls_net_send, mbedtls_net_recv, 0);

    if (mbedHandshake(wp) < 0) {
        return -1;
    }
    return 0;
}


PUBLIC void sslFree(Webs *wp)
{
    MbedSocket *mb;
    WebsSocket *sp;

    mb = wp->ssl;
    sp = socketPtr(wp->sid);
    if (mb) {
        if (!(sp->flags & SOCKET_EOF)) {
            mbedtls_ssl_close_notify(&mb->ctx);
        }
        mbedtls_ssl_free(&mb->ctx);
        wfree(mb);
        wp->ssl = 0;
    }
}


/*
    Initiate or continue SSL handshaking with the peer. This routine does not block.
    Return -1 on errors, 0 incomplete and awaiting I/O, 1 if successful
 */
static int mbedHandshake(Webs *wp)
{
    WebsSocket *sp;
    MbedSocket *mb;
    int        rc, vrc;

    mb = (MbedSocket*) wp->ssl;
    rc = 0;

    sp = socketPtr(wp->sid);
    sp->flags |= SOCKET_HANDSHAKING;

    while ((rc = mbedtls_ssl_handshake(&mb->ctx)) != 0) {
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return 0;
        }
        break;
    }
    sp->flags &= ~SOCKET_HANDSHAKING;

    /*
        Analyze the handshake result
     */
    if (rc < 0) {
        if (rc == MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED && !cfg.ownCert) {
            error("Missing required certificate and key");
        } else if (rc == MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED) {
            trace(2, "Server requires a client certificate");
        } else {
            char ebuf[256];
            mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
            trace(2, "%s: error -0x%x", ebuf, -rc);
        }
        sp->flags |= SOCKET_EOF;
        errno = EPROTO;
        return -1;
    }
    vrc = (int) mbedtls_ssl_get_verify_result(&mb->ctx);
    if (!ME_GOAHEAD_SSL_VERIFY_ISSUER) {
        /*
            SECURITY Acceptable: without issuer verification any client certificate is accepted, including
            self-signed and forged-issuer certificates, as in the OpenSSL backend. Other failures are kept.
         */
        vrc &= ~MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    }
    if (vrc != 0) {
        if (vrc & MBEDTLS_X509_BADCERT_MISSING) {
            logmsg(2, "Certificate missing");

        } else if (vrc & MBEDTLS_X509_BADCERT_EXPIRED) {
            logmsg(2, "Certificate expired");

        } else if (vrc & MBEDTLS_X509_BADCERT_REVOKED) {
            logmsg(2, "Certificate revoked");

        } else if (vrc & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
            logmsg(2, "Certificate common name mismatch");

        } else if (vrc & MBEDTLS_X509_BADCERT_KEY_USAGE || vrc & MBEDTLS_X509_BADCERT_EXT_KEY_USAGE) {
            logmsg(2, "Unauthorized key use in certificate");

        } else if (vrc & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
            logmsg(2, "Certificate not trusted");

        } else if (vrc & MBEDTLS_X509_BADCRL_NOT_TRUSTED) {
            logmsg(2, "Certificate revocation list missing or not trusted");

        } else if (vrc & MBEDTLS_X509_BADCERT_SKIP_VERIFY) {
            /*
                MBEDTLS_SSL_VERIFY_NONE requested, so ignore error
             */
            vrc = 0;

        } else {
            if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                logmsg(2, "Peer disconnected");

            } else {
                logmsg(2, "Cannot handshake: error -0x%x", -rc);
            }
        }
        if (vrc != 0 && ME_GOAHEAD_SSL_VERIFY_PEER) {
            if (mbedtls_ssl_get_peer_cert(&mb->ctx) == 0) {
                logmsg(2, "Peer did not provide a certificate");
            }
            sp->flags |= SOCKET_EOF;
            errno = EPROTO;
            return -1;
        }
    }
    return 1;
}


PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket *sp;
    MbedSocket *mb;
    int        rc;

    if (!wp->ssl) {
        assert(0);
        return -1;
    }
    mb = (MbedSocket*) wp->ssl;
    assert(mb);
    sp = socketPtr(wp->sid);

    if (sp->flags & SOCKET_HANDSHAKING) {
        if ((rc = mbedHandshake(wp)) <= 0) {
            return rc;
        }
    }
    while (1) {
        rc = mbedtls_ssl_read(&mb->ctx, buf, (int) len);
        if (rc < 0) {
            if (rc == MBEDTLS_ERR_SSL_WANT_READ ||
                rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
                rc == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ||
                rc == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS ||
                rc == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
                rc = 0;
                break;
            } else if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                sp->flags |= SOCKET_EOF;
                return -1;
            } else if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                trace(5, "mbedtls: connection reset");
                sp->flags |= SOCKET_EOF;
                return -1;
            } else {
                trace(4, "mbedtls: read error -0x%x", -rc);
                sp->flags |= SOCKET_EOF;
                return -1;
            }
        } else if (rc == 0) {
            sp->flags |= SOCKET_EOF;
            return -1;
        }
        break;
    }
    socketHiddenData(sp, mbedtls_ssl_get_bytes_avail(&mb->ctx), SOCKET_READABLE);
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    MbedSocket *mb;
    WebsSocket *sp;
    ssize      totalWritten;
    int        rc;

    if (wp->ssl == 0 || len <= 0) {
        assert(0);
        return -1;
    }
    mb = (MbedSocket*) wp->ssl;
    sp = socketPtr(wp->sid);

    if (!mbedtls_ssl_is_handshake_over(&mb->ctx)) {
        //  Continuing the handshake here would read from the peer inside a blocking flush
        sp->flags |= SOCKET_EOF;
        socketSetError(ECONNRESET);
        return -1;
    }
    totalWritten = 0;
    do {
        rc = mbedtls_ssl_write(&mb->ctx, (uchar*) buf, (int) len);
        trace(7, "mbedtls write: wrote %d of %zd", rc, len);
        if (rc <= 0) {
            if (rc == MBEDTLS_ERR_SSL_WANT_READ ||
                rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
                rc == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ||
                rc == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS ||
                rc == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
                break;
            }
            if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                trace(4, "mbedtls_ssl_write peer closed");
            } else {
                trace(4, "mbedtls_ssl_write failed rc %d", rc);
            }
            /* Set a hard socket error so a stale EAGAIN cannot cause retries. */
            sp->flags |= SOCKET_EOF;
            socketSetError(ECONNRESET);
            return -1;
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= rc;
        }
    } while (len > 0);

    //  Output MbedTLS still holds is flushed when the socket is next writable, and the mark goes once it is sent
    socketHiddenData(sp, (ssize) mb->ctx.MBEDTLS_PRIVATE(out_left), SOCKET_WRITABLE);
    /*
        Return bytes already sent so a later EAGAIN cannot cause retransmission.
     */
    if (totalWritten == 0 && len > 0) {
        socketSetError(EAGAIN);
        return -1;
    }
    return totalWritten;
}


/*
    Reject static RSA, anonymous, null, DES, 3DES, RC4, and MD5 TLS 1.2 suites.
    Inspect names because MbedTLS keeps cipher fields private.
 */
static bool belowFloor(cchar *name)
{
    return sstarts(name, "TLS-RSA-WITH-") || strstr(name, "-anon-") || strstr(name, "-NULL-") ||
           strstr(name, "-DES-") || strstr(name, "-3DES-") || strstr(name, "-RC4-") || strstr(name, "-MD5");
}


/*
    Return zero-terminated supported suite codes, excluding weak suites and duplicates; NULL if empty.
 */
static int *getCipherSuite(cchar *ciphers)
{
    char  *cipher, *list, *name, *next;
    cchar *cp;
    int   i, j, ntokens, *result, code;

    if (!ciphers || *ciphers == 0) {
        return 0;
    }
    //  Every name yields at most one suite, so the separators bound the result
    for (ntokens = 1, cp = ciphers; *cp; cp++) {
        if (strchr(":, \t", *cp)) {
            ntokens++;
        }
    }
    result = walloc((ntokens + 1) * sizeof(int));

    next = list = sclone(ciphers);
    for (i = 0; (cipher = stok(next, ":, \t", &next)) != 0; ) {
        replaceHyphen(cipher, '_', '-');
        //  TLS 1.3 suites have no WITH in their IANA name and are called TLS1-3-xxx by MbedTLS
        if (sstarts(cipher, "TLS-") && !strstr(cipher, "-WITH-")) {
            name = sfmt("TLS1-3-%s", &cipher[4]);
        } else {
            name = sclone(cipher);
        }
        if ((code = mbedtls_ssl_get_ciphersuite_id(name)) <= 0) {
            error("Unsupported cipher \"%s\"", cipher);
        } else if (belowFloor(mbedtls_ssl_get_ciphersuite_name(code))) {
            error("Cipher \"%s\" is below the minimum TLS policy and is not enabled", cipher);
        } else {
            for (j = 0; j < i && result[j] != code; j++) { }
            if (j == i) {
                result[i++] = code;
            }
        }
        wfree(name);
    }
    result[i] = 0;
    wfree(list);
    if (i == 0) {
        wfree(result);
        return 0;
    }
    return result;
}


static int parseCert(mbedtls_x509_crt *cert, char *path)
{
    char  *buf;
    ssize len;
    int   rc;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read certificate %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        /* Looks PEM encoded so count the null in the length */
        len++;
    }
    rc = mbedtls_x509_crt_parse(cert, (uchar*) buf, len);
    memset(buf, 0, len);
    wfree(buf);
    if (rc != 0) {
        error("Unable to parse certificate %s", path);
        return -1;
    }
    return 0;
}


static int parseKey(mbedtls_pk_context *key, char *path)
{
    char  *buf;
    ssize len;
    int   rc;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read key %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        len++;
    }
    //  The generator derives the public point of a key stored without it
    rc = mbedtls_pk_parse_key(key, (uchar*) buf, len, NULL, 0, mbedtls_ctr_drbg_random, &cfg.ctr);
    memset(buf, 0, len);
    wfree(buf);
    if (rc != 0) {
        if (rc == MBEDTLS_ERR_PK_PASSWORD_REQUIRED) {
            error("Key %s is encrypted. GoAhead requires an unencrypted private key", path);
        } else {
            error("Unable to parse key %s", path);
        }
        return -1;
    }
    return 0;
}


static int parseCrl(mbedtls_x509_crl *crl, char *path)
{
    char  *buf;
    ssize len;
    int   rc;

    if ((buf = websReadWholeFile(path)) == 0) {
        error("Unable to read crl %s", path);
        return -1;
    }
    len = slen(buf);
    if (strstr(buf, "-----BEGIN ")) {
        len++;
    }
    rc = mbedtls_x509_crl_parse(crl, (uchar*) buf, len);
    memset(buf, 0, len);
    wfree(buf);
    if (rc != 0) {
        error("Unable to parse crl %s", path);
        return -1;
    }
    return 0;
}


static void traceMbed(void *context, int level, cchar *file, int line, cchar *str)
{
    ssize len;

    level += mbedLogLevel;
    if (level <= websGetLogLevel()) {
        len = slen(str);
        if (len > 0 && str[len - 1] == '\n') {
            len--;
        }
        trace(level, "%.*s", (int) len, str);
    }
}


static void merror(int rc, PRINTF_FORMAT cchar *fmt, ...)
{
    va_list ap;
    char    ebuf[ME_MAX_BUFFER], *msg;

    va_start(ap, fmt);
    msg = sfmtv(fmt, ap);
    va_end(ap);
    mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
    error("mbedtls error: -0x%x %s %s", -rc, msg, ebuf);
    wfree(msg);
}


static char *replaceHyphen(char *cipher, char from, char to)
{
    char *cp;

    for (cp = cipher; *cp; cp++) {
        if (*cp == from) {
            *cp = to;
        }
    }
    return cipher;
}

#else
void mbedDummy()
{
}
#endif /* ME_COM_MBEDTLS */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/openssl.c ************/

/*
    openssl.c - OpensSSL socket layer for GoAhead

    This is the interface between GoAhead and the OpenSSL stack.
    This code requires OpenSSL 3.0 or later.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/************************************ Include *********************************/

#include    "goahead.h"

#if ME_COM_OPENSSL

#if ME_UNIX_LIKE
/*
    The DH parameter API used for TLS 1.2 DHE is deprecated in OpenSSL 3. Suppress those warnings.
 */
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#if _MSC_VER
/*
    Suppress OpenSSL 3.0 deprecated-declaration warnings for legacy DH/RSA APIs used below.
 */
    #pragma warning(disable : 4996)
#endif

/* Clashes with WinCrypt.h */
#undef OCSP_RESPONSE


 #include    <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    #error "GoAhead requires OpenSSL 3.0 or later. See SECURITY-DEPLOYMENT.md for the supported TLS library versions."
#endif

 #include    <openssl/ssl.h>
 #include    <openssl/evp.h>
 #include    <openssl/rand.h>
 #include    <openssl/err.h>
 #include    <openssl/dh.h>
 #include    <openssl/core_names.h>

/************************************* Defines ********************************/
/*
    Ciphers never permitted for TLS 1.2 and earlier, whatever the configured list says
 */
#define OPENSSL_CIPHER_FLOOR "!aNULL:!eNULL:!EXPORT:!DES:!3DES:!RC4:!MD5:!kRSA"

/*
    Map Iana names to OpenSSL names
 */
typedef struct CipherMap {
    int code;
    char *name;
    char *ossName;
} CipherMap;

static CipherMap cipherMap[] = {
    { 0x0004, "TLS_RSA_WITH_RC4_128_MD5", "RC4-MD5" },
    { 0x0005, "TLS_RSA_WITH_RC4_128_SHA", "RC4-SHA" },
    { 0x0007, "TLS_RSA_WITH_IDEA_CBC_SHA", "IDEA-CBC-SHA" },
    { 0x0009, "TLS_RSA_WITH_DES_CBC_SHA", "DES-CBC-SHA" },
    { 0x000A, "TLS_RSA_WITH_3DES_EDE_CBC_SHA", "DES-CBC3-SHA" },
    { 0x000C, "TLS_DH_DSS_WITH_DES_CBC_SHA", "DH-DSS-DES-CBC-SHA" },
    { 0x000D, "TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA", "DH-DSS-DES-CBC3-SHA" },
    { 0x000F, "TLS_DH_RSA_WITH_DES_CBC_SHA", "DH-RSA-DES-CBC-SHA" },
    { 0x0010, "TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA", "DH-RSA-DES-CBC3-SHA" },
    { 0x0012, "TLS_DHE_DSS_WITH_DES_CBC_SHA", "EDH-DSS-DES-CBC-SHA" },
    { 0x0013, "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA", "EDH-DSS-DES-CBC3-SHA" },
    { 0x0015, "TLS_DHE_RSA_WITH_DES_CBC_SHA", "EDH-RSA-DES-CBC-SHA" },
    { 0x0016, "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA", "EDH-RSA-DES-CBC3-SHA" },
    { 0x002F, "TLS_RSA_WITH_AES_128_CBC_SHA", "AES128-SHA" },
    { 0x0030, "TLS_DH_DSS_WITH_AES_128_CBC_SHA", "DH-DSS-AES128-SHA" },
    { 0x0031, "TLS_DH_RSA_WITH_AES_128_CBC_SHA", "DH-RSA-AES128-SHA" },
    { 0x0032, "TLS_DHE_DSS_WITH_AES_128_CBC_SHA", "DHE-DSS-AES128-SHA" },
    { 0x0033, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA", "DHE-RSA-AES128-SHA" },
    { 0x0035, "TLS_RSA_WITH_AES_256_CBC_SHA", "AES256-SHA" },
    { 0x0036, "TLS_DH_DSS_WITH_AES_256_CBC_SHA", "DH-DSS-AES256-SHA" },
    { 0x0037, "TLS_DH_RSA_WITH_AES_256_CBC_SHA", "DH-RSA-AES256-SHA" },
    { 0x0038, "TLS_DHE_DSS_WITH_AES_256_CBC_SHA", "DHE-DSS-AES256-SHA" },
    { 0x0039, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA", "DHE-RSA-AES256-SHA" },
    { 0x003C, "TLS_RSA_WITH_AES_128_CBC_SHA256", "AES128-SHA256" },
    { 0x003D, "TLS_RSA_WITH_AES_256_CBC_SHA256", "AES256-SHA256" },
    { 0x003E, "TLS_DH_DSS_WITH_AES_128_CBC_SHA256", "DH-DSS-AES128-SHA256" },
    { 0x003F, "TLS_DH_RSA_WITH_AES_128_CBC_SHA256", "DH-RSA-AES128-SHA256" },
    { 0x0040, "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256", "DHE-DSS-AES128-SHA256" },
    { 0x0041, "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA", "CAMELLIA128-SHA" },
    { 0x0042, "TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA", "DH-DSS-CAMELLIA128-SHA" },
    { 0x0043, "TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA", "DH-RSA-CAMELLIA128-SHA" },
    { 0x0044, "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA", "DHE-DSS-CAMELLIA128-SHA" },
    { 0x0045, "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA", "DHE-RSA-CAMELLIA128-SHA" },
    { 0x0067, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256", "DHE-RSA-AES128-SHA256" },
    { 0x0068, "TLS_DH_DSS_WITH_AES_256_CBC_SHA256", "DH-DSS-AES256-SHA256" },
    { 0x0069, "TLS_DH_RSA_WITH_AES_256_CBC_SHA256", "DH-RSA-AES256-SHA256" },
    { 0x006A, "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256", "DHE-DSS-AES256-SHA256" },
    { 0x006B, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256", "DHE-RSA-AES256-SHA256" },
    { 0x0084, "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA", "CAMELLIA256-SHA" },
    { 0x0085, "TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA", "DH-DSS-CAMELLIA256-SHA" },
    { 0x0086, "TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA", "DH-RSA-CAMELLIA256-SHA" },
    { 0x0087, "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA", "DHE-DSS-CAMELLIA256-SHA" },
    { 0x0088, "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA", "DHE-RSA-CAMELLIA256-SHA" },
    { 0x008A, "TLS_PSK_WITH_RC4_128_SHA", "PSK-RC4-SHA" },
    { 0x008B, "TLS_PSK_WITH_3DES_EDE_CBC_SHA", "PSK-3DES-EDE-CBC-SHA" },
    { 0x008C, "TLS_PSK_WITH_AES_128_CBC_SHA", "PSK-AES128-CBC-SHA" },
    { 0x008D, "TLS_PSK_WITH_AES_256_CBC_SHA", "PSK-AES256-CBC-SHA" },
    { 0x0096, "TLS_RSA_WITH_SEED_CBC_SHA", "SEED-SHA" },
    { 0x0097, "TLS_DH_DSS_WITH_SEED_CBC_SHA", "DH-DSS-SEED-SHA" },
    { 0x0098, "TLS_DH_RSA_WITH_SEED_CBC_SHA", "DH-RSA-SEED-SHA" },
    { 0x0099, "TLS_DHE_DSS_WITH_SEED_CBC_SHA", "DHE-DSS-SEED-SHA" },
    { 0x009A, "TLS_DHE_RSA_WITH_SEED_CBC_SHA", "DHE-RSA-SEED-SHA" },
    { 0x009C, "TLS_RSA_WITH_AES_128_GCM_SHA256", "AES128-GCM-SHA256" },
    { 0x009D, "TLS_RSA_WITH_AES_256_GCM_SHA384", "AES256-GCM-SHA384" },
    { 0x009E, "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256", "DHE-RSA-AES128-GCM-SHA256" },
    { 0x009F, "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384", "DHE-RSA-AES256-GCM-SHA384" },
    { 0x00A0, "TLS_DH_RSA_WITH_AES_128_GCM_SHA256", "DH-RSA-AES128-GCM-SHA256" },
    { 0x00A1, "TLS_DH_RSA_WITH_AES_256_GCM_SHA384", "DH-RSA-AES256-GCM-SHA384" },
    { 0x00A2, "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256", "DHE-DSS-AES128-GCM-SHA256" },
    { 0x00A3, "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384", "DHE-DSS-AES256-GCM-SHA384" },
    { 0x00A4, "TLS_DH_DSS_WITH_AES_128_GCM_SHA256", "DH-DSS-AES128-GCM-SHA256" },
    { 0x00A5, "TLS_DH_DSS_WITH_AES_256_GCM_SHA384", "DH-DSS-AES256-GCM-SHA384" },
    { 0xC002, "TLS_ECDH_ECDSA_WITH_RC4_128_SHA", "ECDH-ECDSA-RC4-SHA" },
    { 0xC003, "TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA", "ECDH-ECDSA-DES-CBC3-SHA" },
    { 0xC004, "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA", "ECDH-ECDSA-AES128-SHA" },
    { 0xC005, "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA", "ECDH-ECDSA-AES256-SHA" },
    { 0xC007, "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA", "ECDHE-ECDSA-RC4-SHA" },
    { 0xC008, "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA", "ECDHE-ECDSA-DES-CBC3-SHA" },
    { 0xC009, "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA", "ECDHE-ECDSA-AES128-SHA" },
    { 0xC00A, "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA", "ECDHE-ECDSA-AES256-SHA" },
    { 0xC00C, "TLS_ECDH_RSA_WITH_RC4_128_SHA", "ECDH-RSA-RC4-SHA" },
    { 0xC00D, "TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA", "ECDH-RSA-DES-CBC3-SHA" },
    { 0xC00E, "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA", "ECDH-RSA-AES128-SHA" },
    { 0xC00F, "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA", "ECDH-RSA-AES256-SHA" },
    { 0xC011, "TLS_ECDHE_RSA_WITH_RC4_128_SHA", "ECDHE-RSA-RC4-SHA" },
    { 0xC012, "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA", "ECDHE-RSA-DES-CBC3-SHA" },
    { 0xC013, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA", "ECDHE-RSA-AES128-SHA" },
    { 0xC014, "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA", "ECDHE-RSA-AES256-SHA" },
    { 0xC01A, "TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA", "SRP-3DES-EDE-CBC-SHA" },
    { 0xC01B, "TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA", "SRP-RSA-3DES-EDE-CBC-SHA" },
    { 0xC01C, "TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA", "SRP-DSS-3DES-EDE-CBC-SHA" },
    { 0xC01D, "TLS_SRP_SHA_WITH_AES_128_CBC_SHA", "SRP-AES-128-CBC-SHA" },
    { 0xC01E, "TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA", "SRP-RSA-AES-128-CBC-SHA" },
    { 0xC01F, "TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA", "SRP-DSS-AES-128-CBC-SHA" },
    { 0xC020, "TLS_SRP_SHA_WITH_AES_256_CBC_SHA", "SRP-AES-256-CBC-SHA" },
    { 0xC021, "TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA", "SRP-RSA-AES-256-CBC-SHA" },
    { 0xC022, "TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA", "SRP-DSS-AES-256-CBC-SHA" },
    { 0xC023, "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256", "ECDHE-ECDSA-AES128-SHA256" },
    { 0xC024, "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384", "ECDHE-ECDSA-AES256-SHA384" },
    { 0xC025, "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256", "ECDH-ECDSA-AES128-SHA256" },
    { 0xC026, "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384", "ECDH-ECDSA-AES256-SHA384" },
    { 0xC027, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256", "ECDHE-RSA-AES128-SHA256" },
    { 0xC028, "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384", "ECDHE-RSA-AES256-SHA384" },
    { 0xC029, "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256", "ECDH-RSA-AES128-SHA256" },
    { 0xC02A, "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384", "ECDH-RSA-AES256-SHA384" },
    { 0xC02B, "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256", "ECDHE-ECDSA-AES128-GCM-SHA256" },
    { 0xC02C, "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384", "ECDHE-ECDSA-AES256-GCM-SHA384" },
    { 0xC02D, "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256", "ECDH-ECDSA-AES128-GCM-SHA256" },
    { 0xC02E, "TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384", "ECDH-ECDSA-AES256-GCM-SHA384" },
    { 0xC02F, "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256", "ECDHE-RSA-AES128-GCM-SHA256" },
    { 0xC030, "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384", "ECDHE-RSA-AES256-GCM-SHA384" },
    { 0xC031, "TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256", "ECDH-RSA-AES128-GCM-SHA256" },
    { 0xC032, "TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384", "ECDH-RSA-AES256-GCM-SHA384" },
    { 0x0000, 0, 0 },
};

/*
    OpenSSL context (singleton)
 */
static SSL_CTX *sslctx = NULL;

typedef struct RandBuf {
    time_t now;
    int pid;
} RandBuf;


/*
    Used for OpenSSL versions < 1.0.2
 */
#ifndef ME_GOAHEAD_SSL_CURVE
    #define ME_GOAHEAD_SSL_CURVE "prime256v1"
#endif

/*
    DH parameters
 */
static DH  *dhKey;
static int maxHandshakes;

/*
    Runtime-settable cert/key paths. Override the compile-time ME_GOAHEAD_SSL_CERTIFICATE /
    ME_GOAHEAD_SSL_KEY macros when websSetSslCertFile / websSetSslKeyFile are called
    before sslOpen(). Needed by test drivers that ship their own cert alongside the binary.
 */
static char *sslCertPath;
static char *sslKeyPath;

/*
    Session ticket keys, the current one first and the one it replaced second. See ticketKeyCallback.
 */
typedef struct TicketKey {
    uchar name[16];
    uchar cipher[32];
    uchar mac[32];
    time_t created;                     /* Zero for no key */
} TicketKey;

static TicketKey ticketKeys[2];

/************************************ Forwards ********************************/

static DH *dhcallback(SSL *handle, int is_export, int keylength);
static DH *getDhKey();
static char *mapCipherNames(char *ciphers);
static int  setCiphers(cchar *ciphers);
static int  sslSetCertFile(char *certFile);
static int  sslSetKeyFile(char *keyFile);
static int  verifyClientCertificate(int ok, X509_STORE_CTX *ctx);
static int  noPassphrase(char *buf, int size, int rwflag, void *arg);
static void infoCallback(const SSL *ssl, int where, int rc);
static int  rotateTicketKeys(void);
static int  ticketKeyCallback(SSL *ssl, uchar *name, uchar *iv, EVP_CIPHER_CTX *ctx, EVP_MAC_CTX *hctx, int enc);

/************************************** Code **********************************/
/*
    Open the SSL module
 */
PUBLIC int sslOpen()
{
    RandBuf    randBuf;
    X509_STORE *store;
    uchar      resume[16];
    char       *ciphers;
    int        rc;

    trace(7, "Initializing SSL");

    randBuf.now = time(0);
    randBuf.pid = getpid();
    RAND_seed((void*) &randBuf, sizeof(randBuf));
#if ME_UNIX_LIKE
    trace(6, "OpenSsl: Before calling RAND_load_file");
    RAND_load_file("/dev/urandom", 256);
    trace(6, "OpenSsl: After calling RAND_load_file");
#endif
#if !ME_WIN_LIKE
    OpenSSL_add_all_algorithms();
#endif
    SSL_library_init();
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();

    if ((sslctx = SSL_CTX_new(TLS_server_method())) == 0) {
        error("Unable to create SSL context");
        return -1;
    }

    /*
        Apply the configured protocol versions. TLS 1.1 and earlier are never offered.
     */
    if (!SSL_CTX_set_min_proto_version(sslctx, ME_GOAHEAD_SSL_NO_V1_2 ? TLS1_3_VERSION : TLS1_2_VERSION) ||
        !SSL_CTX_set_max_proto_version(sslctx, ME_GOAHEAD_SSL_NO_V1_3 ? TLS1_2_VERSION : TLS1_3_VERSION)) {
        error("Unable to set the TLS protocol versions");
        sslClose();
        return -1;
    }

    /*
          Set the server certificate and key files. Prefer runtime-provided paths
          (set via websSetSsl{Cert,Key}File) over the compile-time defaults.
     */
    {
        char *keyPath = sslKeyPath ? sslKeyPath : (char*) ME_GOAHEAD_SSL_KEY;
        char *certPath = sslCertPath ? sslCertPath : (char*) ME_GOAHEAD_SSL_CERTIFICATE;

        if (keyPath && *keyPath && sslSetKeyFile(keyPath) < 0) {
            sslClose();
            return -1;
        }
        if (certPath && *certPath && sslSetCertFile(certPath) < 0) {
            sslClose();
            return -1;
        }
    }

    if (ME_GOAHEAD_SSL_VERIFY_PEER) {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyClientCertificate);
        SSL_CTX_set_verify_depth(sslctx, ME_GOAHEAD_SSL_VERIFY_DEPTH);
    } else {
        SSL_CTX_set_verify(sslctx, SSL_VERIFY_NONE, verifyClientCertificate);
    }
    /*
        Verify client certificates only against the configured authority, never the system public CA store. Without
        an authority, refuse issuer verification.
     */
    if (ME_GOAHEAD_SSL_AUTHORITY && *ME_GOAHEAD_SSL_AUTHORITY) {
        if (!SSL_CTX_load_verify_locations(sslctx, ME_GOAHEAD_SSL_AUTHORITY, NULL)) {
            error("Unable to read cert verification locations");
            sslClose();
            return -1;
        }
        /*
            Define the list of CA certificates to send to the client before they send
            their client certificate for validation
         */
        SSL_CTX_set_client_CA_list(sslctx, SSL_load_client_CA_file(ME_GOAHEAD_SSL_AUTHORITY));
    } else if (ME_GOAHEAD_SSL_VERIFY_PEER && ME_GOAHEAD_SSL_VERIFY_ISSUER) {
        error("Client certificate issuers are verified but ME_GOAHEAD_SSL_AUTHORITY is not set: every client "
              "certificate will be refused");
    }
    if (ME_GOAHEAD_SSL_REVOKE && *ME_GOAHEAD_SSL_REVOKE) {
        store = SSL_CTX_get_cert_store(sslctx);
        if (!X509_STORE_load_locations(store, ME_GOAHEAD_SSL_REVOKE, 0)) {
            error("Cannot load certificate revoke list: %s", ME_GOAHEAD_SSL_REVOKE);
            sslClose();
            return -1;
        }
        /*
            Check revocation for the full chain; require a CRL for each CA.
         */
        X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    }

    /*
        Configure DH parameters
     */
    dhKey = getDhKey();
    SSL_CTX_set_tmp_dh_callback(sslctx, dhcallback);

    /*
        Configure cipher suite
     */
    if (ME_GOAHEAD_SSL_CIPHERS && *ME_GOAHEAD_SSL_CIPHERS) {
        ciphers = ME_GOAHEAD_SSL_CIPHERS;
    } else {
        ciphers = ME_GOAHEAD_SSL_DEFAULT_CIPHERS;
    }
    ciphers = mapCipherNames(ciphers);
    trace(5, "Using OpenSSL ciphers: %s", ciphers);
    rc = setCiphers(ciphers);
    wfree(ciphers);
    if (rc < 0) {
        sslClose();
        return -1;
    }

    /*
        Define default OpenSSL options
     */
    SSL_CTX_set_options(sslctx, SSL_OP_ALL);

    /*
        Ensure we generate a new private key for each connection
     */
    SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_DH_USE);

    /*
        Define a session reuse context
     */
    if (RAND_bytes(resume, sizeof(resume)) != 1) {
        error("Cannot get random bytes for the TLS session context");
        return -1;
    }
    SSL_CTX_set_session_id_context(sslctx, resume, sizeof(resume));

    /*
        Elliptic Curve initialization
     */
#if SSL_OP_SINGLE_ECDH_USE
    #ifdef SSL_CTX_set_ecdh_auto
    SSL_CTX_set_ecdh_auto(sslctx, 1);
    #else
    {
        EC_KEY *ecdh;
        cchar  *name;
        int    nid;

        name = ME_GOAHEAD_SSL_CURVE;
        if ((nid = OBJ_sn2nid(name)) == 0) {
            error("Unknown curve name \"%s\"", name);
            sslClose();
            return -1;
        }
        if ((ecdh = EC_KEY_new_by_curve_name(nid)) == 0) {
            error("Unable to create curve \"%s\"", name);
            sslClose();
            return -1;
        }
        SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_ECDH_USE);
        SSL_CTX_set_tmp_ecdh(sslctx, ecdh);
        EC_KEY_free(ecdh);
    }
    #endif
#endif

    SSL_CTX_set_mode(sslctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_AUTO_RETRY | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    SSL_CTX_set_options(sslctx, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(sslctx, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
    SSL_CTX_set_options(sslctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
#endif

#if defined(SSL_OP_NO_TICKET)
    /*
        Ticket based session reuse is enabled by default
     */
    #if defined(ME_GOAHEAD_SSL_TICKET)
    if (ME_GOAHEAD_SSL_TICKET) {
        SSL_CTX_clear_options(sslctx, SSL_OP_NO_TICKET);
    } else {
        SSL_CTX_set_options(sslctx, SSL_OP_NO_TICKET);
    }
    #else
    SSL_CTX_clear_options(sslctx, SSL_OP_NO_TICKET);
    #endif
#endif
    SSL_CTX_set_timeout(sslctx, ME_GOAHEAD_SSL_TIMEOUT);
    if (rotateTicketKeys() < 0) {
        error("Cannot get random bytes for the session ticket key");
        return -1;
    }
    SSL_CTX_set_tlsext_ticket_key_evp_cb(sslctx, ticketKeyCallback);

    /*
        Disable resumption with client certificates so expiry, revocation, and chain checks run each time.
     */
    if (ME_GOAHEAD_SSL_VERIFY_PEER) {
        SSL_CTX_set_options(sslctx, SSL_OP_NO_TICKET);
        SSL_CTX_set_session_cache_mode(sslctx, SSL_SESS_CACHE_OFF);
        SSL_CTX_set_num_tickets(sslctx, 0);
    }

#if defined(SSL_OP_NO_COMPRESSION)
    /*
        CRIME attack targets compression
     */
    SSL_CTX_set_options(sslctx, SSL_OP_NO_COMPRESSION);
#endif

#ifdef SSL_OP_NO_RENEGOTIATION
    /* Disallow renegotiation entirely */
    SSL_CTX_set_options(sslctx, SSL_OP_NO_RENEGOTIATION);
#endif

#if defined(ME_GOAHEAD_SSL_HANDSHAKES)
    maxHandshakes = ME_GOAHEAD_SSL_HANDSHAKES;
#endif

#if defined(SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)
    /*
        Disables a countermeasure against a SSL 3.0/TLS 1.0 protocol vulnerability affecting CBC ciphers.
        Defaults to true.
     */
    #if defined(ME_GOAHEAD_SSL_EMPTY_FRAGMENTS)
    if (ME_GOAHEAD_SSL_EMPTY_FRAGMENTS) {
        /* SSL_OP_ALL disables empty fragments. Only needed for ancient browsers like IE-6 on SSL-3.0/TLS-1.0 */
        SSL_CTX_clear_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    } else {
        SSL_CTX_set_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    }
    #else
    SSL_CTX_set_options(sslctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    #endif
#endif

#if defined(ME_GOAHEAD_SSL_CACHE)
    /*
        Set the number of sessions supported. Default in OpenSSL is 20K.
     */
    SSL_CTX_sess_set_cache_size(sslctx, ME_GOAHEAD_SSL_CACHE);
#else
    SSL_CTX_sess_set_cache_size(sslctx, 256);
#endif
    return 0;
}


/*
    Close the SSL module
 */
PUBLIC void sslClose()
{
    if (sslctx != NULL) {
        SSL_CTX_free(sslctx);
        sslctx = NULL;
        if (dhKey) {
            DH_free(dhKey);
            dhKey = NULL;
        }
    }
    OPENSSL_cleanse(ticketKeys, sizeof(ticketKeys));
    wfree(sslCertPath);
    sslCertPath = NULL;
    wfree(sslKeyPath);
    sslKeyPath = NULL;
}


PUBLIC bool sslHasCertificate(void)
{
    return sslctx && SSL_CTX_get0_certificate(sslctx) && SSL_CTX_get0_privatekey(sslctx);
}


/*
    Runtime override for the server certificate path. Must be called before sslOpen().
 */
PUBLIC void websSetSslCertFile(cchar *path)
{
    wfree(sslCertPath);
    sslCertPath = path ? sclone(path) : NULL;
}


/*
    Runtime override for the server key path. Must be called before sslOpen().
 */
PUBLIC void websSetSslKeyFile(cchar *path)
{
    wfree(sslKeyPath);
    sslKeyPath = path ? sclone(path) : NULL;
}


/*
    Upgrade a socket to use SSL
 */
PUBLIC int sslUpgrade(Webs *wp)
{
    WebsSocket *sptr;
    BIO        *bio;

    assert(wp);

    sptr = socketPtr(wp->sid);
    if ((wp->ssl = SSL_new(sslctx)) == 0) {
        return -1;
    }
    /*
        Create a socket bio. We don't use the BIO except as storage for the fd.
     */
    if ((bio = BIO_new_socket((int) sptr->sock, BIO_NOCLOSE)) == 0) {
        return -1;
    }
    SSL_set_bio(wp->ssl, bio, bio);
    SSL_set_accept_state(wp->ssl);
    SSL_set_app_data(wp->ssl, (void*) wp);
    if (ME_GOAHEAD_SSL_HANDSHAKES) {
        SSL_CTX_set_info_callback(sslctx, infoCallback);
    }
    return 0;
}

static void infoCallback(const SSL *ssl, int where, int rc)
{
    Webs       *wp;
    WebsSocket *sp;

    if (where & SSL_CB_HANDSHAKE_START) {
        if ((wp = (Webs*) SSL_get_app_data(ssl)) == 0) {
            return;
        }
        if ((sp = socketPtr(wp->sid)) != 0) {
            sp->handshakes++;
        }
    }
}


PUBLIC void sslFree(Webs *wp)
{
    WebsSocket *sp;

    if (wp->ssl) {
        //  Tell the peer the stream is complete, unless the handshake did not finish or the connection failed
        sp = (wp->sid >= 0) ? socketPtr(wp->sid) : NULL;
        if (sp && !(sp->flags & SOCKET_EOF) && SSL_is_init_finished(wp->ssl)) {
            SSL_shutdown(wp->ssl);
            ERR_clear_error();
        }
        //  Marks the session resumable
        SSL_set_shutdown(wp->ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        SSL_free(wp->ssl);
        wp->ssl = 0;
    }
}


/*
    Return the number of bytes read. Return -1 on errors and EOF. Distinguish EOF via socketEof().
    If non-blocking, may return zero if no data or still handshaking.
 */
PUBLIC ssize sslRead(Webs *wp, void *buf, ssize len)
{
    WebsSocket *sp;
    char       ebuf[ME_GOAHEAD_LIMIT_STRING];
    ulong      serror;
    int        rc, err, retries, i;

    if (wp->ssl == 0 || len <= 0) {
        return -1;
    }
    /*
        Limit retries on WANT_READ. If non-blocking and no data, then this can spin forever.
     */
    sp = socketPtr(wp->sid);
    retries = 5;
    for (i = 0; i < retries; i++) {
        rc = SSL_read(wp->ssl, buf, (int) len);
        if (rc <= 0) {
            err = SSL_get_error(wp->ssl, rc);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_CONNECT || err == SSL_ERROR_WANT_ACCEPT) {
                continue;
            }
            if (err != SSL_ERROR_ZERO_RETURN) {
                serror = ERR_get_error();
                ERR_error_string_n(serror, ebuf, sizeof(ebuf) - 1);
                trace(5, "SSL_read %s", ebuf);
            }
        }
        break;
    }
    if (maxHandshakes && sp->handshakes > maxHandshakes) {
        error("TLS renegotiation attack");
        rc = -1;
        sp->flags |= SOCKET_EOF;
        return -1;
    }
    if (rc <= 0) {
        err = SSL_get_error(wp->ssl, rc);
        if (err == SSL_ERROR_WANT_READ) {
            rc = 0;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            rc = 0;
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            sp->flags |= SOCKET_EOF;
            rc = -1;
        } else if (err == SSL_ERROR_SYSCALL) {
            sp->flags |= SOCKET_EOF;
            rc = -1;
        } else {
            serror = ERR_get_error();
            ERR_error_string_n(serror, ebuf, sizeof(ebuf) - 1);
            trace(4, "OpenSSL: connection with protocol error: %s", ebuf);
            rc = -1;
            sp->flags |= SOCKET_EOF;
        }
    } else if (SSL_pending(wp->ssl) > 0) {
        socketHiddenData(sp, SSL_pending(wp->ssl), SOCKET_READABLE);
    }
    return rc;
}


PUBLIC ssize sslWrite(Webs *wp, void *buf, ssize len)
{
    WebsSocket *sp;
    ssize      totalWritten;
    int        err, rc;

    if (wp->ssl == 0 || len <= 0) {
        return -1;
    }
    sp = socketPtr(wp->sid);
    if (!SSL_is_init_finished(wp->ssl)) {
        //  SSL_write would continue the handshake, reading from the peer inside a blocking flush
        sp->flags |= SOCKET_EOF;
        socketSetError(ECONNRESET);
        return -1;
    }
    totalWritten = 0;
    err = 0;
    ERR_clear_error();

    do {
        rc = SSL_write(wp->ssl, buf, (int) len);
        trace(7, "OpenSSL: written %d, requested len %zd", rc, len);
        if (rc <= 0) {
            err = SSL_get_error(wp->ssl, rc);
            if (err == SSL_ERROR_NONE || err == SSL_ERROR_WANT_WRITE) {
                break;
            }
            trace(7, "OpenSSL: error %d", err);
            /* Set a hard socket error so a stale EAGAIN cannot cause retries. */
            sp->flags |= SOCKET_EOF;
            socketSetError(ECONNRESET);
            return -1;
        } else if (maxHandshakes && sp->handshakes > maxHandshakes) {
            error("TLS renegotiation attack");
            sp->flags |= SOCKET_EOF;
            socketSetError(ECONNRESET);
            return -1;
        }
        totalWritten += rc;
        buf = (void*) ((char*) buf + rc);
        len -= rc;
        trace(7, "OpenSSL: write: len %zd, written %d, total %zd", len, rc, totalWritten);
    } while (len > 0);

    if (totalWritten == 0 && err == SSL_ERROR_WANT_WRITE) {
        socketSetError(EAGAIN);
        return -1;
    }
    return totalWritten;
}


/*
    Load the leaf and following intermediate certificates for the server chain.
 */
static int sslSetCertFile(char *certFile)
{
    X509          *cert, *extra;
    BIO           *bio;
    char          *buf;
    unsigned long err;
    int           rc;

    assert(sslctx);
    assert(certFile);

    rc = -1;
    bio = 0;
    buf = 0;
    cert = 0;

    if (sslctx == NULL) {
        return rc;
    }
    if ((buf = websReadWholeFile(certFile)) == 0) {
        error("Unable to read certificate %s", certFile);

    } else if ((bio = BIO_new_mem_buf(buf, -1)) == 0) {
        error("Unable to allocate memory for certificate %s", certFile);

    } else if ((cert = PEM_read_bio_X509(bio, NULL, noPassphrase, NULL)) == 0) {
        error("Unable to parse certificate %s", certFile);

    } else if (SSL_CTX_use_certificate(sslctx, cert) != 1) {
        error("Unable to use certificate %s", certFile);

    } else if (!SSL_CTX_check_private_key(sslctx)) {
        error("Unable to check certificate key %s", certFile);

    } else {
        rc = 0;
        SSL_CTX_clear_extra_chain_certs(sslctx);
        while ((extra = PEM_read_bio_X509(bio, NULL, noPassphrase, NULL)) != 0) {
            if (SSL_CTX_add_extra_chain_cert(sslctx, extra) != 1) {
                error("Unable to use chain certificate in %s", certFile);
                X509_free(extra);
                rc = -1;
                break;
            }
        }
        /*
            Only no-start-line at EOF is normal; report other chain parse failures.
         */
        err = ERR_peek_last_error();
        if (rc == 0 && err && !(ERR_GET_LIB(err) == ERR_LIB_PEM && ERR_GET_REASON(err) == PEM_R_NO_START_LINE)) {
            error("Unable to parse chain certificate in %s", certFile);
            rc = -1;
        }
        ERR_clear_error();
    }
    wfree(buf);
    if (bio) {
        BIO_free(bio);
    }
    if (cert) {
        X509_free(cert);
    }
    return rc;
}


/*
    Set the private key file for the SSL context. Any key type OpenSSL supports: RSA, EC or Ed25519.
 */
static int sslSetKeyFile(char *keyFile)
{
    EVP_PKEY *key;
    BIO      *bio;
    char     *buf;
    bool     encrypted;
    int      rc;

    assert(sslctx);
    assert(keyFile);

    key = 0;
    bio = 0;
    buf = 0;
    encrypted = 0;
    rc = -1;

    if (sslctx == NULL) {
        ;

    } else if ((buf = websReadWholeFile(keyFile)) == 0) {
        error("Unable to read key %s", keyFile);

    } else if ((bio = BIO_new_mem_buf(buf, -1)) == 0) {
        error("Unable to allocate memory for key %s", keyFile);

    } else if ((key = PEM_read_bio_PrivateKey(bio, NULL, noPassphrase, &encrypted)) == 0) {
        if (encrypted) {
            error("Key %s is encrypted. GoAhead requires an unencrypted private key", keyFile);
        } else {
            error("Unable to parse key %s", keyFile);
        }

    } else if (SSL_CTX_use_PrivateKey(sslctx, key) != 1) {
        error("Unable to use key %s", keyFile);

    } else {
        rc = 0;
    }
    wfree(buf);
    if (bio) {
        BIO_free(bio);
    }
    if (key) {
        EVP_PKEY_free(key);
    }
    return rc;
}


/*
    Refuse to prompt on the console for a passphrase. Record in arg, when given, that one was asked for.
 */
static int noPassphrase(char *buf, int size, int rwflag, void *arg)
{
    if (arg) {
        *(bool*) arg = 1;
    }
    return -1;
}


/*
    Make a new session ticket key and keep the current one as the previous.
 */
static int rotateTicketKeys(void)
{
    ticketKeys[1] = ticketKeys[0];
    if (RAND_bytes(ticketKeys[0].name, sizeof(ticketKeys[0].name)) != 1 ||
        RAND_bytes(ticketKeys[0].cipher, sizeof(ticketKeys[0].cipher)) != 1 ||
        RAND_bytes(ticketKeys[0].mac, sizeof(ticketKeys[0].mac)) != 1) {
        OPENSSL_cleanse(&ticketKeys[0], sizeof(ticketKeys[0]));
        return -1;
    }
    ticketKeys[0].created = time(0);
    return 0;
}


/*
    Rotate ticket keys each timeout. Accept the previous key for one more timeout and renew its tickets.
 */
static int ticketKeyCallback(SSL *ssl, uchar *name, uchar *iv, EVP_CIPHER_CTX *ctx, EVP_MAC_CTX *hctx, int enc)
{
    TicketKey  *key;
    OSSL_PARAM params[3];
    time_t     now;
    int        i;

    now = time(0);
    if (enc) {
        if (ticketKeys[0].created == 0 || now - ticketKeys[0].created >= ME_GOAHEAD_SSL_TIMEOUT) {
            if (rotateTicketKeys() < 0) {
                return -1;
            }
        }
        key = &ticketKeys[0];
        if (RAND_bytes(iv, EVP_CIPHER_get_iv_length(EVP_aes_256_cbc())) != 1) {
            return -1;
        }
        memcpy(name, key->name, sizeof(key->name));
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key->cipher, iv)) {
            return -1;
        }
    } else {
        for (i = 0, key = 0; i < 2 && !key; i++) {
            if (ticketKeys[i].created && now - ticketKeys[i].created < 2 * ME_GOAHEAD_SSL_TIMEOUT &&
                memcmp(name, ticketKeys[i].name, sizeof(ticketKeys[i].name)) == 0) {
                key = &ticketKeys[i];
            }
        }
        if (!key) {
            //  Unknown or retired key: make a full handshake
            return 0;
        }
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key->cipher, iv)) {
            return -1;
        }
    }
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY, key->mac, sizeof(key->mac));
    params[1] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, (char*) "SHA256", 0);
    params[2] = OSSL_PARAM_construct_end();
    if (!EVP_MAC_CTX_set_params(hctx, params)) {
        return -1;
    }
    return (enc || key == &ticketKeys[0]) ? 1 : 2;
}


static int verifyClientCertificate(int ok, X509_STORE_CTX *xContext)
{
    X509 *cert;
    char subject[260], issuer[260];
    int  error, depth;

    subject[0] = issuer[0] = '\0';

    if ((cert = X509_STORE_CTX_get_current_cert(xContext)) == NULL) {
        logmsg(3, "Certificate verification failed: no current certificate");
        return 0;
    }
    error = X509_STORE_CTX_get_error(xContext);
    depth = X509_STORE_CTX_get_error_depth(xContext);

    ok = 1;
    //  The names are only logged and never decide acceptance
    if (X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) - 1) == NULL) {
        subject[0] = '\0';
    }
    if (X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) - 1) == NULL) {
        issuer[0] = '\0';
    }
    /*
        Enforce the configured chain depth limit unconditionally. The underlying verifier may surface an
        over-depth chain under a variety of error codes (for example, some LibreSSL releases report the
        chain as X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY instead of X509_V_ERR_CERT_CHAIN_TOO_LONG).
        Rejecting here based on X509_STORE_CTX_get_error_depth avoids relying on backend-specific error
        mapping.
     */
    if (ME_GOAHEAD_SSL_VERIFY_DEPTH < depth) {
        logmsg(3, "Certificate chain too long (depth %d exceeds limit %d)", depth, ME_GOAHEAD_SSL_VERIFY_DEPTH);
        trace(4, "OpenSSL: Subject: %s", subject);
        trace(4, "OpenSSL: Issuer: %s", issuer);
        return 0;
    }
    /*
        SECURITY Acceptable: ME_GOAHEAD_SSL_VERIFY_PEER requests certificates. Without VERIFY_ISSUER,
        accept untrusted and forged-issuer certificates without revocation checks; only key possession is proven.
        Still reject invalid dates, malformed certificates, and excessive chain depth.
     */
    switch (error) {
    case X509_V_OK:
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
            logmsg(3, "Self-signed certificate");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
            logmsg(3, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        if (ME_GOAHEAD_SSL_VERIFY_ISSUER) {
            logmsg(3, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_INVALID_CA:
    default:
        logmsg(3, "Certificate verification error %d", error);
        ok = 0;
        break;
    }
    if (ok) {
        trace(3, "OpenSSL: Certificate verified: subject %s", subject);
    } else {
        trace(1, "OpenSSL: Certification failed: subject %s (more trace at level 4)", subject);
        trace(4, "OpenSSL: Error: %d: %s", error, X509_verify_cert_error_string(error));
    }
    trace(4, "OpenSSL: Issuer: %s", issuer);
    return ok;
}



/*
    Map iana names to OpenSSL names so users can provide IANA names as well as OpenSSL cipher names
 */
/*
    Split TLS 1.3 and earlier cipher names between OpenSSL APIs.
    Apply the minimum cipher restrictions to earlier versions; fail if either list is invalid.
 */
static int setCiphers(cchar *ciphers)
{
    WebsBuf tls13, legacy;
    char    *list, *cipher, *next;
    int     rc;

    bufCreate(&tls13, 0, -1);
    bufCreate(&legacy, 0, -1);
    list = sclone(ciphers);
    for (next = list; (cipher = stok(next, ":", &next)) != 0; ) {
        if (sstarts(cipher, "TLS_")) {
            bufPut(&tls13, "%s%s", bufLen(&tls13) ? ":" : "", cipher);
        } else {
            bufPut(&legacy, "%s:", cipher);
        }
    }
    wfree(list);
    bufPut(&legacy, "%s%s", bufLen(&legacy) ? "" : "DEFAULT:", OPENSSL_CIPHER_FLOOR);

    rc = 0;
    if (bufLen(&tls13) && SSL_CTX_set_ciphersuites(sslctx, bufStart(&tls13)) != 1) {
        error("Unable to set TLS 1.3 ciphers \"%s\"", bufStart(&tls13));
        rc = -1;
    }
    if (rc == 0 && SSL_CTX_set_cipher_list(sslctx, bufStart(&legacy)) != 1) {
        error("Unable to set cipher list \"%s\"", bufStart(&legacy));
        rc = -1;
    }
    bufFree(&tls13);
    bufFree(&legacy);
    return rc;
}


static char *mapCipherNames(char *ciphers)
{
    WebsBuf   buf;
    CipherMap *cp;
    char      *cipher, *next, *str;

    if (!ciphers || *ciphers == 0) {
        return 0;
    }
    bufCreate(&buf, 0, 0);
    ciphers = sclone(ciphers);
    for (next = ciphers; (cipher = stok(next, ":, \t", &next)) != 0; ) {
        for (cp = cipherMap; cp->name; cp++) {
            if (smatch(cp->name, cipher)) {
                bufPut(&buf, "%s:", cp->ossName);
                break;
            }
        }
        if (cp->name == 0) {
            bufPut(&buf, "%s:", cipher);
        }
    }
    wfree(ciphers);
    str = sclone(bufStart(&buf));
    bufFree(&buf);
    return str;
}


/*
    Get the DH parameters
 */

static DH *getDhKey()
{
    static unsigned char dh2048_p[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
        0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1, 0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
        0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
        0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
        0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45, 0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
        0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
        0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
        0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D, 0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
        0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
        0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
        0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D, 0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
        0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
        0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
        0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9, 0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
        0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
        0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    static unsigned char dh2048_g[] = {
        0x02,
    };
    DH                   *dh;
    BIGNUM               *p, *g;

    if ((dh = DH_new()) == 0) {
        return 0;
    }
    p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
    g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
    if (!DH_set0_pqg(dh, p, NULL, g)) {
        BN_free(p);
        BN_free(g);
        DH_free(dh);
        return 0;
    }
    return dh;
}


/*
    Set the ephemeral DH key
 */
static DH *dhcallback(SSL *handle, int isExport, int keyLength)
{
    return dhKey;
}

#else
void opensslDummy()
{
}
#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/options.c ************/

/*
    options.c -- Options and Trace handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Code *************************************/

/*
    Handle OPTIONS and TRACE methods.
    Return true to indicate the request was handled, even for errors.
 */
static bool optionsHandler(Webs *wp)
{
#if !ME_GOAHEAD_STEALTH
    char *body;
#endif

    assert(wp);

#if !ME_GOAHEAD_STEALTH
    if (smatch(wp->method, "OPTIONS")) {
        websSetStatus(wp, HTTP_CODE_OK);
        websWriteHeaders(wp, 0, 0);
        websWriteHeader(wp, "Allow", "DELETE,GET,HEAD,OPTIONS,POST,PUT,TRACE");
        websWriteEndHeaders(wp);
        websDone(wp);
        return 1;

    } else if (smatch(wp->method, "TRACE")) {
        /*
            Format first so Content-Length includes the whole echo.
         */
        body = sfmt("%s %s %s\r\n", wp->method, wp->url, wp->protoVersion);
        websSetStatus(wp, HTTP_CODE_OK);
        websWriteHeaders(wp, slen(body), 0);
        websWriteHeader(wp, "Content-Type", "message/http");
        websWriteEndHeaders(wp);
        websWriteBlock(wp, body, slen(body));
        wfree(body);
        websDone(wp);
        return 1;
    }
#endif
    websResponse(wp, HTTP_CODE_NOT_ACCEPTABLE, "Unsupported method");
    return 1;
}


PUBLIC int websOptionsOpen(void)
{
    websDefineHandler("options", 0, optionsHandler, 0, 0);
    return 0;
}


/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/osdep.c ************/

/*
    osdep.c -- O/S dependent code

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/

#if ME_WIN_LIKE
static HINSTANCE appInstance;
PUBLIC void syslog(int priority, char *format, ...);
#endif

/*
    The resolved staging directory, computed on first use
 */
static char *tempDir;

/************************************* Code ***********************************/

PUBLIC int websOsOpen(void)
{
#if ME_UNIX_LIKE
    int fd;

    /*
        Reserve closed descriptors 0-2 with inheritable /dev/null so logs cannot reach sockets and CGI inherits safe
        stdio.
     */
    while ((fd = open("/dev/null", O_RDWR)) >= 0 && fd <= 2) { }
    if (fd > 2) {
        close(fd);
    }
#endif
#if SOLARIS
    openlog(ME_NAME, LOG_LOCAL0);
#elif ME_UNIX_LIKE
    openlog(ME_NAME, 0, LOG_LOCAL0);
#endif
#if WINDOWS || VXWORKS || TIDSP
    rand();
#else
    random();
#endif
    return 0;
}


/*
    Restrict DLL searches to system and application directories.
 */
PUBLIC void websHardenProcess(void)
{
#if ME_WIN_LIKE && !WINCE
    typedef BOOL (WINAPI *SetDefaultDllDirectoriesProc)(DWORD flags);
    SetDefaultDllDirectoriesProc setDefaultDllDirectories;
    HMODULE                      kernel;

    #ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
        #define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x200
    #endif
    #ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
        #define LOAD_LIBRARY_SEARCH_SYSTEM32        0x800
    #endif

    /* An empty directory removes the current directory from the legacy search order */
    if (!SetDllDirectoryA("")) {
        error("Cannot remove the current directory from the DLL search path");
    }
    /*
        Resolve at runtime for older headers and Windows 7 systems without KB2533623.
     */
    kernel = GetModuleHandleA("kernel32.dll");
    setDefaultDllDirectories = kernel ?
                               (SetDefaultDllDirectoriesProc) (void*) GetProcAddress(kernel,
                                                                                     "SetDefaultDllDirectories") : NULL;
    if (setDefaultDllDirectories == NULL) {
        error("Cannot restrict the DLL search path: SetDefaultDllDirectories is not available");
    } else if (!setDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_APPLICATION_DIR)) {
        error("Cannot restrict the DLL search path");
    }
#endif
}


PUBLIC void websOsClose(void)
{
    wfreeset(tempDir);
#if ME_UNIX_LIKE
    closelog();
#endif
}


PUBLIC cchar *websGetTempDir(void)
{
    cchar *dir;

#if ME_WIN_LIKE && !WINCE
    char  buf[MAX_PATH + 1];
    DWORD len;
#endif

    if (!tempDir) {
        dir = ME_GOAHEAD_TEMP_DIR;
#if ME_WIN_LIKE && !WINCE
        if (*dir == '\0') {
            len = GetTempPathA((DWORD) sizeof(buf), buf);
            if (len > 0 && len < sizeof(buf)) {
                dir = buf;
            }
        }
#endif
        tempDir = websAbsolutePath(dir);
    }
    return tempDir;
}


/*
    Create a temporary file and return its allocated path name.

    The file is created here, empty and private, before the name is returned. The name carries 64 bits
    from the system CSPRNG so a client cannot predict where another request's data is staged, and the
    create is exclusive and refuses a symlink so a local attacker can neither pre-create the path nor
    aim it at a file the server may not write. The caller opens the returned path, and must unlink the
    file and free the name when done.

    Returns NULL if no temporary file could be created. Callers must fail the request rather than
    proceed with an unnamed staging file.
 */
PUBLIC char *websTempFile(cchar *dir, cchar *prefix)
{
    cchar *hexDigits = "0123456789abcdef";
    uchar randomBytes[8];
    char  name[sizeof(randomBytes) * 2 + 1], *path;
    char  sep;
    int   fd, i, tries;

#if WINCE
    sep = '\\';
#elif ME_WIN_LIKE
    sep = '\\';
#else
    sep = '/';
#endif
    /*
        Never fall back to the working directory; it may expose staged request data.
     */
    if (!dir || *dir == '\0') {
        dir = websGetTempDir();
    }
    if (!prefix) {
        prefix = "tmp";
    }
    for (tries = 0; tries < 8; tries++) {
        if (websGetRandomBytes((char*) randomBytes, sizeof(randomBytes), 0) < 0) {
            error("Cannot read random bytes for a temp file name");
            return 0;
        }
        for (i = 0; i < (int) sizeof(randomBytes); i++) {
            name[i * 2] = hexDigits[randomBytes[i] >> 4];
            name[i * 2 + 1] = hexDigits[randomBytes[i] & 0xf];
        }
        name[sizeof(name) - 1] = '\0';
        path = sfmt("%s%c%s-%s.tmp", dir, sep, prefix, name);
        if ((fd = open(path, O_CREAT | O_EXCL | O_RDWR | O_BINARY | O_NOFOLLOW, 0600)) >= 0) {
            close(fd);
            return path;
        }
        wfree(path);
        if (errno != EEXIST) {
            /*
                A missing or unwritable temp directory. Retrying cannot help and would only spin.
             */
            break;
        }
    }
    error("Cannot create a temp file in \"%s\". Set ME_GOAHEAD_TEMP_DIR to a writable directory "
          "outside the documents directory.", dir);
    return 0;
}


#if VXWORKS
/*
    Get absolute path.  In VxWorks, functions like chdir, ioctl for mkdir and ioctl for rmdir, require an absolute path.
    This function will take the path argument and convert it to an absolute path.  It is the caller's responsibility to
    deallocate the returned string.
 */
static char *getAbsolutePath(char *path)
{
#if _WRS_VXWORKS_MAJOR >= 6
    const char *tail;
#else
    char *tail;
#endif
    char cwd[ME_GOAHEAD_LIMIT_FILENAME];

    /*
        Determine if path is relative or absolute.  If relative, prepend the current working directory to the name.
        Otherwise, use it.  Note the getcwd call below must not be getcwd or else we go into an infinite loop
     */
    if (iosDevFind(path, &tail) != NULL && path != tail) {
        return sclone(path);
    }
#if ME_ROM
    cwd[0] = '\0';
#else
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        cwd[0] = '\0';
    }
#endif
    return sfmt("%s/%s", cwd, path);
}


PUBLIC int vxchdir(char *dirname)
{
    char *path;
    int  rc;

    path = getAbsolutePath(dirname);
    #undef chdir
    rc = chdir(path);
    wfree(path);
    return rc;
}
#endif


#if ECOS
PUBLIC int send(int s, const void *buf, size_t len, int flags)
{
    return write(s, buf, len);
}


PUBLIC int recv(int s, void *buf, size_t len, int flags)
{
    return read(s, buf, len);
}
#endif


#if ME_WIN_LIKE
PUBLIC void websSetInst(HINSTANCE inst)
{
    appInstance = inst;
}


HINSTANCE websGetInst()
{
    return appInstance;
}


PUBLIC void syslog(int priority, char *format, ...)
{
    va_list    args;
    HKEY       hkey;
    void       *event;
    long       errorType;
    ulong      exists;
    char       *buf, logName[ME_GOAHEAD_LIMIT_STRING], *lines[9], *cp, *value;
    int        type;
    static int once = 0;

    va_start(args, format);
    buf = sfmtv(format, args);
    va_end(args);

    for (cp = &buf[slen(buf)]; cp > buf && cp[-1] == '\n'; cp--) {
        cp[-1] = '\0';
    }
    type = EVENTLOG_ERROR_TYPE;
    lines[0] = buf;
    lines[1] = 0;
    lines[2] = lines[3] = lines[4] = lines[5] = 0;
    lines[6] = lines[7] = lines[8] = 0;

    if (once == 0) {
        /*  Initialize the registry */
        once = 1;
        hkey = 0;
        fmt(logName, sizeof(logName), "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", ME_NAME);

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, logName, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &exists) ==
            ERROR_SUCCESS) {
            value = "%SystemRoot%\\System32\\netmsg.dll";
            if (RegSetValueEx(hkey, "EventMessageFile", 0, REG_EXPAND_SZ,
                              (uchar*) value, (int) slen(value) + 1) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                wfree(buf);
                return;
            }
            errorType = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
            if (RegSetValueEx(hkey, "TypesSupported", 0, REG_DWORD, (uchar*) &errorType, sizeof(DWORD)) !=
                ERROR_SUCCESS) {
                RegCloseKey(hkey);
                wfree(buf);
                return;
            }
            RegCloseKey(hkey);
        }
    }
    event = RegisterEventSource(0, ME_NAME);
    if (event) {
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, 3299, NULL, sizeof(lines) / sizeof(char*), 0, (LPCSTR*) lines, 0);
        DeregisterEventSource(event);
    }
    wfree(buf);
}


PUBLIC void sleep(int secs)
{
    Sleep(secs / 1000);
}
#endif

/*
    "basename" returns a pointer to the last component of a pathname LINUX, LynxOS and Mac OS X have their own basename
 */
#if !ME_UNIX_LIKE
PUBLIC char *basename(char *name)
{
    char *cp;

#if ME_WIN_LIKE
    if (((cp = strrchr(name, '\\')) == NULL) && ((cp = strrchr(name, '/')) == NULL)) {
        return name;
#else
    if ((cp = strrchr(name, '/')) == NULL) {
        return name;
#endif
    } else if (*(cp + 1) == '\0' && cp == name) {
        return name;
    } else if (*(cp + 1) == '\0' && cp != name) {
        return "";
    } else {
        return ++cp;
    }
}
#endif


#if TIDSP
static char _inet_result[16];

char *inet_ntoa(struct in_addr addr)
{
    uchar *bytes;

    bytes = (uchar*) &addr;
    snprintf(_inet_result, sizeof(_inet_result), "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    return _inet_result;
}


struct hostent * gethostbyname(char *name)
{
    static char buffer[ME_MAX_PATH];

    if (!DNSGetHostByName(name, buffer, ME_MAX_PATH)) {
        return 0;
    }
    return (struct hostent*) buffer;
}


int gethostname(char *host, int bufSize)
{
    return DNSGetHostname(host, bufSize);
}


int closesocket(SOCKET s)
{
    return fdClose(s);
}


int select(int maxfds, fd_set *readFds, fd_set *writeFds, fd_set *exceptFds, struct timeval *timeVal)
{
    return fdSelect(maxfds, readFds, writeFds, exceptFds, timeVal);
}

#endif /* TIDSP */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/rom.c ************/

/*
   rom.c
   Compiled by webcomp: Mon Jan 14 14:13:54 2013
 */


#if ME_ROM
WebsRomIndex websRomIndex[] = {
    { 0, 0, 0, 0, 0 }
};
#else
WebsRomIndex websRomIndex[] = {
    { 0, 0, 0, 0, 0 }
};
#endif


/********* Start of file src/route.c ************/

/*
    route.c -- Route Management

    This module implements the loading of a route configuration file
    and the routing of requests.

    The route configuration is loaded from a text file of space-separated key=value directives
    (see route.txt). The supported directives are:
        route uri=URI [protocol=PROTO] [methods=METHOD,...] [handler=NAME] [extensions=EXT,...]
              [abilities=ABILITY,...] [redirect=STATUS@URI] [dir=DIR] [auth=TYPE]
        user name=NAME password=PASSWORD roles=ROLE,...
        role name=NAME abilities=ABILITY,...

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static WebsRoute **routes = 0;
static WebsHash  handlers = -1;
static int       routeCount = 0;
static int       routeMax = 0;
static bool      strictMode = ME_GOAHEAD_STRICT;
static WebsRoute **retiredRoutes;                  /* Removed routes still held by in-flight requests */
static int       retiredCount;
static WebsHash  routeMethods = -1;                 /* Methods named by a route's methods= beyond the standard set */

#define WEBS_MAX_ROUTE     16           /* Maximum passes over route set */

/* Verify at startup that a build matching routes with case does not serve a file system that ignores it */
#define CHECK_CASE_FOLDING (!ME_GOAHEAD_CASE_INSENSITIVE_FS && !ME_ROM && !ME_WIN_LIKE)

/********************************** Forwards **********************************/

static void addKnownMethods(WebsHash methods);
static bool continueHandler(Webs *wp);
static void freeRoute(WebsRoute *route);
static void growRoutes(void);
static int lookupRoute(cchar *uri);
static bool redirectHandler(Webs *wp);

/************************************ Code ************************************/
/* Return the action name and length from the route prefix, or zero for a route handling otherwise unnamed actions. */
static ssize routeActionName(WebsRoute *route, cchar **name)
{
    cchar *cp;

    *name = "";
    if (!route->handler || !smatch(route->handler->name, "action") || (cp = strchr(&route->prefix[1], '/')) == 0) {
        return 0;
    }
    *name = ++cp;
    return (ssize) strcspn(cp, "/");
}


/* Named action routes handle only their action; unnamed routes exclude actions claimed by named routes. */
PUBLIC bool websRouteServesAction(WebsRoute *route, cchar *name)
{
    cchar *own;
    ssize len;
    int   i;

    if (route == 0 || name == 0) {
        return 0;
    }
    if ((len = routeActionName(route, &own)) > 0) {
        return slen(name) == len && sncmp(own, name, len) == 0;
    }
    for (i = 0; i < routeCount; i++) {
        if ((len = routeActionName(routes[i], &own)) > 0 && slen(name) == len && sncmp(own, name, len) == 0) {
            return 0;
        }
    }
    return 1;
}

/*
    Fold ASCII case on case-insensitive filesystems to keep route authorization aligned with file lookup. Preserve
    the original path for open and CGI; validCanonicalPath rejects other filesystem aliases.
 */
static int matchRoutePrefix(cchar *path, cchar *prefix, ssize len)
{
#if ME_GOAHEAD_CASE_INSENSITIVE_FS
    return sncaselesscmp(path, prefix, len);
#else
    return sncmp(path, prefix, len);
#endif
}


/*
    Does a route's methods= admit the method? HEAD follows GET (RFC 9110 9.3.2). A route without methods= admits
    the safe methods GET, HEAD and POST.
 */
static bool routeAllowsMethod(WebsRoute *route, cchar *method)
{
    if (route->methods < 0) {
        return smatch(method, "GET") || smatch(method, "HEAD") || smatch(method, "POST");
    }
    return hashLookup(route->methods, method) || (smatch(method, "HEAD") && hashLookup(route->methods, "GET"));
}



static int routeRunsCode(WebsRoute *route)
{
    cchar *name;

    name = route->handler ? route->handler->name : "";
    return smatch(name, "jst") || smatch(name, "cgi") || smatch(name, "action");
}


/*
    Route the request. If wp->route is already set, test routes after that route
 */

PUBLIC void websRouteRequest(Webs *wp)
{
    WebsRoute   *route;
    WebsHandler *handler;
    ssize       plen, len;
    bool        safeMethod, gateOnly;
    int         i;

    assert(wp);
    if (!wp->path || !wp->method || !wp->protocol) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Incomplete request");
        return;
    }
#if ME_GOAHEAD_SECURE_ONLY
    if (!(wp->flags & WEBS_HTTPS)) {
        websRedirect(wp, "https://");
        return;
    }
#endif
    /*
        Entering with a route set means this is a re-route: a handler declined and websPump routed again.
        Every re-entry counts against the loop limit, however it was reached. Without this, a handler that
        declines forever spins the event loop and the single threaded server never services another
        connection.
     */
    if (wp->route && ++wp->routeCount >= WEBS_MAX_ROUTE) {
        wp->route = 0;
        trace(2, "Route loop for %s", wp->url);
        websError(wp, HTTP_CODE_NOT_FOUND, "Cannot find suitable route for request.");
        return;
    }
    safeMethod = smatch(wp->method, "POST") || smatch(wp->method, "GET") || smatch(wp->method, "HEAD");

    /*
        Resume routing from after the last matched route. This permits the legacy service() callbacks to
        return false and continue routing. A resumed scan that runs off the end means no further route
        matched, so the scan below does not run and the request fails with a 404. It must never wrap back
        to the start: that re-offers routes the request has already been given.
     */
    if (wp->route && !(wp->flags & WEBS_REROUTE)) {
        for (i = 0; i < routeCount; i++) {
            if (wp->route == routes[i]) {
                i++;
                break;
            }
        }
    } else {
        i = 0;
    }
    wp->route = 0;

    for (; i < routeCount; i++) {
        route = routes[i];
        assert(route->prefix && route->prefixLen > 0);

        /*
            Recompute path length each iteration because match callbacks may rewrite it.
         */
        plen = slen(wp->path);
        if (plen < route->prefixLen) continue;
        len = min(route->prefixLen, plen);
        trace(5, "Examine route %s", route->prefix);
        /*
            Match route
         */
        if (matchRoutePrefix(wp->path, route->prefix, len) != 0) {
            continue;
        }
        if (route->protocol && !smatch(route->protocol, wp->protocol)) {
            trace(5, "Route %s does not match protocol %s", route->prefix, wp->protocol);
            continue;
        }
        gateOnly = 0;
        if (route->methods >= 0) {
            if (!routeAllowsMethod(route, wp->method)) {
                trace(5, "Route %s does not match method %s", route->prefix, wp->method);
                continue;
            }
        } else if (!safeMethod) {
            /* A route without methods= authenticates every method, even though it serves only safe methods. */
            if (!route->authType && route->abilities < 0) {
                continue;
            }
            gateOnly = 1;
        }
        if (route->extensions >= 0 && (wp->ext == 0 || !hashLookup(route->extensions, &wp->ext[1]))) {
            trace(5, "Route %s doesn match extension %s", route->prefix, wp->ext ? wp->ext : "");
            continue;
        }

        wp->route = route;
        /*
            Reject cross-site state-changing requests before route authentication and handling; browsers attach
            credentials automatically.
         */
        if (!(route->flags & WEBS_ROUTE_CROSS_SITE) && !smatch(wp->method, "GET") && !smatch(wp->method, "HEAD") &&
            !websSameOrigin(wp)) {
#if ME_GOAHEAD_AUTH
            if (route->authType) {
                websLogAuth(wp, 0, "refused", "request from another site");
            }
#endif
            websError(wp, HTTP_CODE_FORBIDDEN, "Request from another site refused");
            wp->route = 0;
            return;
        }
#if ME_GOAHEAD_AUTH
        /*
            Clear wp->route on a deny so that "a route was selected" can never be read as "the
            request was authorized". Any caller that resumes from wp->route after this point must
            find nothing to dispatch.
         */
        if (route->authType && !websAuthenticate(wp)) {
            wp->route = 0;
            return;
        }
        if (route->abilities >= 0 && !websCan(wp, route->abilities)) {
            wp->route = 0;
            return;
        }
#if ME_GOAHEAD_DIGEST
        websAcceptDigestCount(wp);
#endif
#endif
        if (gateOnly) {
            wp->route = 0;
            continue;
        }
        if ((handler = route->handler) == 0) {
            continue;
        }
        if (!handler->match || (*handler->match)(wp)) {
            /* Handler matches */
            return;
        }
        wp->route = 0;
        if (wp->flags & WEBS_REROUTE) {
            wp->flags &= ~WEBS_REROUTE;
            if (++wp->routeCount >= WEBS_MAX_ROUTE) {
                break;
            }
            // Rescan from route 0: the loop increment follows
            i = -1;
        }
    }
    if (wp->routeCount >= WEBS_MAX_ROUTE) {
        trace(2, "Route loop for %s", wp->url);
    }
    websError(wp, HTTP_CODE_NOT_FOUND, "Cannot find suitable route for request.");
    assert(wp->route == 0);
}


PUBLIC bool websRunRequest(Webs *wp)
{
    WebsRoute *route;

    assert(wp);
    /*
        Never dispatch a handler for a request whose response has already been written. The state
        machine is the primary guard; this is defence in depth for any future caller.
     */
    if (wp->finalized) {
        return 1;
    }
    if (!wp->path || !wp->method || !wp->protocol) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Incomplete request");
        return 1;
    }
    if ((route = wp->route) == 0) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no route for request");
        return 1;
    }
    if (!route->handler) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no handler for route");
        return 1;
    }
    if (!wp->filename || route->dir) {
        wfreeset(wp->filename);
        wp->filename = sfmt("%s%s", route->dir ? route->dir : websGetDocuments(), wp->path);
    }
    if (!(wp->flags & WEBS_VARS_ADDED)) {
        wp->flags |= WEBS_VARS_ADDED;
        if (websSetQueryVars(wp) < 0 || websSetFormVars(wp) < 0) {
            websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Too many request parameters");
            return 1;
        }
    }
    wp->state = WEBS_RUNNING;
    trace(5, "Route %s calls handler %s", route->prefix, route->handler->name);

#if ME_GOAHEAD_LEGACY
    if (route->handler->legacy) {
        return (*route->handler->legacy)(wp, route->prefix, route->dir, route->flags) == 0;
    } else
#endif
    if (!route->handler->service) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Configuration error - no handler service callback");
        return 1;
    }
    return (*route->handler->service)(wp);
}


#if ME_GOAHEAD_AUTH
static bool can(Webs *wp, char *ability)
{
    assert(wp);
    assert(ability && *ability);

    if (wp->user && hashLookup(wp->user->abilities, ability)) {
        return 1;
    }
    return 0;
}



static bool canAny(Webs *wp, cchar *abilities)
{
    char *copy, *ability, *tok;
    bool ok;

    ok = 0;
    copy = sclone(abilities);
    for (ability = stok(copy, "|", &tok); ability && !ok; ability = stok(NULL, "|", &tok)) {
        ok = can(wp, ability);
    }
    wfree(copy);
    return ok;
}


PUBLIC bool websCan(Webs *wp, WebsHash abilities)
{
    WebsKey *key;
    char    *ability;

    assert(wp);
    assert(abilities >= 0);

    if (!wp->user) {
        if (wp->authType) {
            if (!wp->username) {
                websLogAuth(wp, 0, "refused", "user not logged in");
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. User not logged in.");
                return 0;
            }
            if ((wp->user = websLookupUser(wp->username)) == 0) {
                websLogAuth(wp, 0, "refused", "unknown user");
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Unknown user.");
                return 0;
            }
        }
    }
    if (abilities >= 0) {
        if (!wp->user && wp->username) {
            wp->user = websLookupUser(wp->username);
        }
        for (key = hashFirst(abilities); key; key = hashNext(abilities, key)) {
            ability = key->name.value.string;
            if (strchr(ability, '|') ? !canAny(wp, ability) : !can(wp, ability)) {
                websLogAuth(wp, 0, "refused", "insufficient abilities");
                websError(wp, HTTP_CODE_UNAUTHORIZED, "Access Denied. Insufficient capabilities.");
                return 0;
            }
        }
    }
    return 1;
}
#endif


/*
    If pos is < 0, then add to the end. Otherwise insert at specified position
 */
PUBLIC WebsRoute *websAddRoute(cchar *uri, cchar *handler, int pos)
{
    WebsRoute *route;
    WebsKey   *key;

    if (uri == 0 || *uri == '\0') {
        error("Route has bad URI");
        return 0;
    }
    if ((route = walloc(sizeof(WebsRoute))) == 0) {
        return 0;
    }
    memset(route, 0, sizeof(WebsRoute));
    route->prefix = sclone(uri);
    route->prefixLen = slen(uri);
    route->abilities = route->extensions = route->methods = route->redirects = -1;
    if (!handler) {
        handler = "file";
    }
    if ((key = hashLookup(handlers, handler)) == 0) {
        error("Cannot find route handler %s", handler);
        wfreeset(route->prefix);
        wfree(route);
        return 0;
    }
    route->handler = key->content.value.symbol;
    growRoutes();
    if (pos < 0) {
        pos = routeCount;
    }
    if (pos < routeCount) {
        memmove(&routes[pos + 1], &routes[pos], sizeof(WebsRoute*) * (routeCount - pos));
    }
    routes[pos] = route;
    routeCount++;
    return route;
}


/*
    Return an unmatchable selector value: lowercase method, uppercase extension, or leading extension dot. Otherwise NULL.
 */
static cchar *badSelector(WebsHash hash, bool upper)
{
    WebsKey *kp;
    cchar   *cp;

    if (hash < 0) {
        return 0;
    }
    for (kp = hashFirst(hash); kp; kp = hashNext(hash, kp)) {
        cp = kp->name.value.string;
        if (!upper && *cp == '.') {
            return cp;
        }
        for (; *cp; cp++) {
            if (upper ? islower((uchar) *cp) : isupper((uchar) *cp)) {
                return kp->name.value.string;
            }
        }
    }
    return 0;
}


PUBLIC int websSetRouteMatch(WebsRoute *route, cchar *dir, cchar *protocol, WebsHash methods, WebsHash extensions,
                             WebsHash abilities, WebsHash redirects)
{
    cchar *bad;

    assert(route);

    if (dir) {
        route->dir = sclone(dir);
    }
    route->protocol = protocol ? sclone(protocol) : 0;
    if (abilities >= 0 && hashFirst(abilities) == 0) {
        //  An empty ability set, such as abilities=none, is no requirement
        hashFree(abilities);
        abilities = -1;
    }
    route->abilities = abilities;
    route->extensions = extensions;
    route->methods = methods;
    route->redirects = redirects;
    addKnownMethods(methods);

    /* Reject selector spellings that cannot match normalized method, extension or protocol values. */
    if (protocol && !smatch(protocol, "http") && !smatch(protocol, "https")) {
        error("Route \"%s\" has protocol=%s, which never matches. Use protocol=http or protocol=https.",
              route->prefix, protocol);
        return -1;
    }
    if ((bad = badSelector(methods, 1)) != 0) {
        error("Route \"%s\" has method \"%s\", which never matches. Write methods in upper case.", route->prefix, bad);
        return -1;
    }
    if ((bad = badSelector(extensions, 0)) != 0) {
        error("Route \"%s\" has extension \"%s\", which never matches. Write extensions in lower case without the "
              "leading dot.", route->prefix, bad);
        return -1;
    }

#if !ME_GOAHEAD_AUTH
    /*
        This build compiles out the authorization gate, so an ability requirement can only be
        recorded on the route and never consulted. Refuse it here - the single choke point through
        which every ability set reaches a route - rather than let the route table describe a policy
        the binary cannot enforce. The hashes are attached to the route above before failing so that
        websCloseRoute still owns and frees them.
     */
    if (abilities >= 0) {
        error("Route \"%s\" requires abilities, but this build has ME_GOAHEAD_AUTH=0 and cannot "
              "enforce them. Rebuild with ME_GOAHEAD_AUTH=1 or remove the abilities requirement.",
              route->prefix);
        return -1;
    }
#endif
    return 0;
}


static void growRoutes(void)
{
    if (routeCount >= routeMax) {
        routeMax += 16;
        if ((routes = wrealloc(routes, sizeof(WebsRoute*) * routeMax)) == 0) {
            error("Cannot grow routes");
        }
    }
}


static int lookupRoute(cchar *uri)
{
    WebsRoute *route;
    int       i;

    assert(uri && *uri);

    for (i = 0; i < routeCount; i++) {
        route = routes[i];
        if (smatch(route->prefix, uri)) {
            return i;
        }
    }
    return -1;
}


static void freeRoute(WebsRoute *route)
{
    assert(route);

    if (route->abilities >= 0) {
        hashFree(route->abilities);
    }
    if (route->extensions >= 0) {
        hashFree(route->extensions);
    }
    if (route->methods >= 0) {
        hashFree(route->methods);
    }
    if (route->redirects >= 0) {
        hashFree(route->redirects);
    }
    wfreeset(route->prefix);
    wfreeset(route->dir);
    wfreeset(route->protocol);
    wfreeset(route->authType);
    wfree(route);
}


PUBLIC int websRemoveRoute(cchar *uri)
{
    WebsRoute *route;
    int       i;

    assert(uri && *uri);

    if ((i = lookupRoute(uri)) < 0) {
        return -1;
    }
    route = routes[i];
    for (; i < routeCount - 1; i++) {
        routes[i] = routes[i + 1];
    }
    routeCount--;
    if (websRouteHolders(route) == 0) {
        freeRoute(route);
        return 0;
    }
    route->flags |= WEBS_ROUTE_REMOVED;
    retiredRoutes = wrealloc(retiredRoutes, sizeof(WebsRoute*) * (retiredCount + 1));
    retiredRoutes[retiredCount++] = route;
    return 0;
}


PUBLIC void websReapRoutes(void)
{
    int i;

    for (i = retiredCount - 1; i >= 0; i--) {
        if (websRouteHolders(retiredRoutes[i]) == 0) {
            freeRoute(retiredRoutes[i]);
            retiredRoutes[i] = retiredRoutes[--retiredCount];
        }
    }
    if (retiredCount == 0) {
        wfreeset(retiredRoutes);
    }
}


#if ME_GOAHEAD_AUTH
/*
    Does this route gate access behind authentication or an ability check?
 */
/*
    Identify authentication, ability, and protocol restrictions that broader routes could bypass.
 */
static int routeRequiresAuth(WebsRoute *route)
{
    return route->authType || route->abilities >= 0 || route->protocol != 0;
}


/*
    Does this route narrow itself with a selector? A qualified route claims only a subset of the requests
    its prefix covers, so it shadows less than a bare prefix route does.
 */
static int routeIsQualified(WebsRoute *route)
{
    return route->extensions >= 0 || route->methods >= 0 || route->protocol != 0;
}


/*
    Check whether every request under the prefix runs code; a named action route handles only that action.
 */
static int routeRunsAllCode(WebsRoute *route)
{
    cchar *action;

    if (!routeRunsCode(route)) {
        return 0;
    }
    return !smatch(route->handler->name, "action") || routeActionName(route, &action) == 0;
}


/* Continue, upload and redirect handlers do not serve protected content themselves. */
static int routeServesContent(WebsRoute *route)
{
    cchar *name;

    name = route->handler ? route->handler->name : "";
    return !smatch(name, "continue") && !smatch(name, "upload") && !smatch(name, "redirect");
}



static int routeIsGate(WebsRoute *route)
{
    return route->handler && smatch(route->handler->name, "continue");
}


/*
    Return the selector through which requests can reach a later nested route without authentication, or null if
    covered.
 */
static cchar *gateUncovered(WebsRoute *gate, WebsRoute *route)
{
    WebsKey *key;
    cchar   *safe[] = { "GET", "HEAD", "POST", 0 };
    int     i;

    if (gate->methods >= 0) {
        if (route->methods >= 0) {
            for (key = hashFirst(route->methods); key; key = hashNext(route->methods, key)) {
                if (!routeAllowsMethod(gate, key->name.value.string)) {
                    return key->name.value.string;
                }
            }
        } else {
            for (i = 0; safe[i]; i++) {
                if (!routeAllowsMethod(gate, safe[i])) {
                    return safe[i];
                }
            }
        }
    }
    if (gate->protocol && (!route->protocol || !smatch(gate->protocol, route->protocol))) {
        return route->protocol ? route->protocol : "another protocol";
    }
    if (gate->extensions >= 0) {
        if (route->extensions < 0) {
            return "another extension";
        }
        for (key = hashFirst(route->extensions); key; key = hashNext(route->extensions, key)) {
            if (!hashLookup(gate->extensions, key->name.value.string)) {
                return key->name.value.string;
            }
        }
    }
    return 0;
}



static int prefixesNest(WebsRoute *r1, WebsRoute *r2)
{
    WebsRoute *shorter, *longer;

    shorter = r1->prefixLen <= r2->prefixLen ? r1 : r2;
    longer = shorter == r1 ? r2 : r1;
    return matchRoutePrefix(longer->prefix, shorter->prefix, shorter->prefixLen) == 0;
}


static int dirContains(cchar *dir, cchar *child);

/*
    Compare configured document roots for overlap, by the same rule as the temporary directory audit. Symlink
    aliases are not detected. Continue routes protect subsequent routes and overlap all roots.
 */
static int regionsOverlap(WebsRoute *r1, WebsRoute *r2)
{
    cchar *d1, *d2;

    if (routeIsGate(r1) || routeIsGate(r2)) {
        return 1;
    }
    d1 = r1->dir ? r1->dir : websGetDocuments();
    d2 = r2->dir ? r2->dir : websGetDocuments();
    if (d1 == 0 || d2 == 0) {
        return 1;
    }
    return dirContains(d1, d2) || dirContains(d2, d1);
}


/*
    Report route ordering, selector and action overlaps that bypass authentication. Return the problem count; a
    table without authentication warns without counting. Use error() so warnings appear at the default log level.
 */
static int checkRouteAuth(void)
{
    WebsRoute *strict, *loose, *candidate;
    cchar     *uncovered;
    int       i, j, authRoutes, problems;

    problems = 0;
    authRoutes = 0;
    for (i = 0; i < routeCount; i++) {
        if (routeRequiresAuth(routes[i])) {
            authRoutes++;
        }
    }
    if (authRoutes == 0) {
        error("Security warning: no authenticated routes configured. All content is served to "
              "unauthenticated clients. See route.txt for how to protect content.");
        return 0;
    }
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!routeRequiresAuth(strict)) {
            continue;
        }
        /*
            Report one warning per protected route, naming the worst shadowing route rather than every
            one. A route carrying extensions=, methods= or protocol= only claims a subset of requests, so
            an unqualified route is the more serious exposure and is the one worth naming.
         */
        loose = 0;
        for (j = i + 1; j < routeCount; j++) {
            candidate = routes[j];
            if (routeRequiresAuth(candidate)) {
                continue;
            }
            if (!regionsOverlap(strict, candidate)) {
                continue;
            }
            if (matchRoutePrefix(strict->prefix, candidate->prefix, candidate->prefixLen) != 0) {
                continue;
            }
            /*
                Equal-prefix routes receive requests declined by selectors. Protocol-only selection cannot be
                bypassed by path spelling.
             */
            if (candidate->prefixLen == strict->prefixLen && strict->extensions < 0 && strict->methods < 0) {
                continue;
            }
            if (loose == 0 || (routeIsQualified(loose) && !routeIsQualified(candidate))) {
                loose = candidate;
            }
        }
        if (loose && routeIsGate(strict)) {
            /*
                A gate's dir= serves nothing, so it is not a remedy: the directory must be served by an
                authenticated route of its own.
             */
            error("Security warning: gate route \"%s\" requires authentication, but later route \"%s\" "
                  "(handler=%s) does not and can serve the same requests. A case or Unicode folded request will "
                  "miss \"%s\" and be served without credentials. Serve the directory from \"%s\" itself, with "
                  "the file handler and its own dir=, or reorder the routes.",
                  strict->prefix, loose->prefix, loose->handler ? loose->handler->name : "?",
                  strict->prefix, strict->prefix);
            problems++;

        } else if (loose) {
            /*
                Several routes may share one prefix - shipped route.txt has three "/" routes - so name
                the shadowing route's handler too, or the operator cannot tell which one is meant.
             */
            error("Security warning: route \"%s\" requires authentication, but later route \"%s\" "
                  "(handler=%s) does not and can serve the same directory. A case or Unicode folded "
                  "request will miss \"%s\" and be served without credentials. Reorder the routes, "
                  "narrow \"%s\", or give \"%s\" its own dir=.",
                  strict->prefix, loose->prefix, loose->handler ? loose->handler->name : "?",
                  strict->prefix, loose->prefix, strict->prefix);
            problems++;
        }
    }
    /* Report nested public routes reachable through an authenticated route's unmatched selectors. */
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!strict->authType && strict->abilities < 0) {
            continue;
        }
        for (j = i + 1; j < routeCount; j++) {
            candidate = routes[j];
            if (routeRequiresAuth(candidate) || !routeServesContent(candidate) ||
                candidate->prefixLen <= strict->prefixLen ||
                matchRoutePrefix(candidate->prefix, strict->prefix, strict->prefixLen) != 0 ||
                !regionsOverlap(strict, candidate)) {
                continue;
            }
            if ((uncovered = gateUncovered(strict, candidate)) != 0) {
                error("Security warning: route \"%s\" requires authentication only for some requests, but later "
                      "route \"%s\" (handler=%s) under it also accepts %s, which reaches it without credentials. "
                      "%s", strict->prefix, candidate->prefix, candidate->handler ? candidate->handler->name : "?",
                      uncovered, strict->protocol ? "Drop protocol= from the authenticated route and redirect http "
                      "to https with a protocol=http redirect=https route instead." :
                      "Drop the selector from the authenticated route or give the later route the same one.");
                problems++;
            }
        }
    }
    /*
        Report only the first earlier conflicting route; code routes are checked separately below.
     */
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!routeRequiresAuth(strict)) {
            continue;
        }
        for (j = 0; j < i; j++) {
            candidate = routes[j];
            if (routeRequiresAuth(candidate) || !routeServesContent(candidate) || routeRunsAllCode(candidate)) {
                continue;
            }
            if (!regionsOverlap(strict, candidate) ||
                matchRoutePrefix(strict->prefix, candidate->prefix, candidate->prefixLen) != 0) {
                continue;
            }
            error("Security warning: route \"%s\" (handler=%s) comes before authenticated route \"%s\" and "
                  "serves its requests without credentials. Put \"%s\" first.",
                  candidate->prefix, candidate->handler ? candidate->handler->name : "?", strict->prefix,
                  strict->prefix);
            problems++;
            break;
        }
    }
    /* Unnamed action routes share otherwise unclaimed actions. Protect individual actions with named routes. */
    for (i = 0; i < routeCount; i++) {
        strict = routes[i];
        if (!routeRequiresAuth(strict) || !routeRunsAllCode(strict) || !smatch(strict->handler->name, "action")) {
            continue;
        }
        for (j = 0; j < routeCount; j++) {
            candidate = routes[j];
            if (j != i && !routeRequiresAuth(candidate) && routeRunsAllCode(candidate) &&
                smatch(candidate->handler->name, "action")) {
                error("Security warning: authenticated action route \"%s\" and unauthenticated action route \"%s\" "
                      "both run every action, so \"%s\" protects none of them. Name each protected action in its "
                      "own route, such as \"/action/upload\".", strict->prefix, candidate->prefix, strict->prefix);
                problems++;
                break;
            }
        }
    }

    for (j = 0; j < routeCount; j++) {
        candidate = routes[j];
        if (routeRequiresAuth(candidate) || !routeRunsAllCode(candidate)) {
            continue;
        }
        for (i = j + 1; i < routeCount; i++) {
            strict = routes[i];
            if (routeRequiresAuth(strict) && prefixesNest(strict, candidate) && regionsOverlap(strict, candidate)) {
                error("Security warning: route \"%s\" (handler=%s) comes before authenticated route \"%s\", so it "
                      "runs its %s without credentials. Put authenticated routes before the action, template and "
                      "CGI routes.", candidate->prefix, candidate->handler->name, strict->prefix,
                      smatch(candidate->handler->name, "action") ? "actions" :
                      smatch(candidate->handler->name, "jst") ? "templates" : "CGI programs");
                problems++;
                break;
            }
        }
    }
    return problems;
}

#else

/*
    With no authentication, identify the first code-executing or writable route in the warning.
 */
static void warnUnauthenticated(void)
{
    WebsRoute *route;
    int       i;

    for (i = 0; i < routeCount; i++) {
        route = routes[i];
        if (routeRunsCode(route) || (route->flags & WEBS_ROUTE_UPLOAD) || routeAllowsMethod(route, "PUT") ||
            routeAllowsMethod(route, "DELETE")) {
            error("Security warning: this build has ME_GOAHEAD_AUTH=0 and cannot authenticate, so every route is "
                  "served to unauthenticated clients, including route \"%s\" (handler=%s), which %s. Make every "
                  "route safe for anonymous use, or build with ME_GOAHEAD_AUTH=1 and protect it.", route->prefix,
                  route->handler ? route->handler->name : "?",
                  routeRunsCode(route) ? "runs code" : "accepts uploads or writes");
            return;
        }
    }
    error("Security warning: this build has ME_GOAHEAD_AUTH=0 and cannot authenticate, so every route is served to "
          "unauthenticated clients.");
}
#endif /* ME_GOAHEAD_AUTH */


#if CHECK_CASE_FOLDING

static bool flipCase(char *name)
{
    bool flipped;

    for (flipped = 0; *name; name++) {
        if (isupper((uchar) *name)) {
            *name = (char) tolower((uchar) *name);
            flipped = 1;
        } else if (islower((uchar) *name)) {
            *name = (char) toupper((uchar) *name);
            flipped = 1;
        }
    }
    return flipped;
}


/*
    Probe filesystem case rules using an entry or the directory's own name. Do not write to the filesystem.
 */
static bool isCaseInsensitiveDir(cchar *dir)
{
    struct stat   s1, s2;
    struct dirent *dp;
    DIR           *dirp;
    char          *path, *name, *original, *variant;
    bool          result;

    path = websAbsolutePath(dir);
    original = variant = 0;
    if ((dirp = opendir(path)) != 0) {
        while (!variant && (dp = readdir(dirp)) != 0) {
            name = sclone(dp->d_name);
            if (flipCase(name)) {
                original = sfmt("%s/%s", path, dp->d_name);
                variant = sfmt("%s/%s", path, name);
            }
            wfree(name);
        }
        closedir(dirp);
    }
    if (!variant && (name = strrchr(path, '/')) != 0 && name[1]) {
        original = sclone(path);
        variant = sclone(path);
        if (!flipCase(&variant[name - path + 1])) {
            wfreeset(variant);
        }
    }
    result = variant && stat(original, &s1) == 0 && stat(variant, &s2) == 0 && s1.st_dev == s2.st_dev &&
             s1.st_ino == s2.st_ino;
    wfree(original);
    wfree(variant);
    wfree(path);
    return result;
}


/*
    Warn when case-sensitive routes serve case-insensitive filesystems, permitting alternate spellings to bypass routes.
 */
static int checkCaseFolding(void)
{
    cchar *docs, *dir;
    int   i, j, problems;

    problems = 0;
    docs = websGetDocuments();
    for (i = -1; i < routeCount; i++) {
        dir = (i < 0) ? docs : routes[i]->dir;
        if (dir == 0 || *dir == '\0' || (i >= 0 && smatch(dir, docs))) {
            continue;
        }
        for (j = 0; j < i && !smatch(routes[j]->dir, dir); j++) { }
        if (j < i || !isCaseInsensitiveDir(dir)) {
            continue;
        }
        error("Security warning: the served directory \"%s\" is on a file system that ignores letter case, but "
              "this build matches routes with case, so a request spelled in another case misses its route and is "
              "served by a broader one. Build with ME_GOAHEAD_CASE_INSENSITIVE_FS=1.", dir);
        problems++;
    }
    return problems;
}
#endif /* CHECK_CASE_FOLDING */


/*
    Return the number of route-policy bypasses.
 */
PUBLIC int websCheckRoutes(void)
{
    int problems;

    if (routeCount == 0) {
        return 0;
    }
    problems = 0;
#if CHECK_CASE_FOLDING
    problems += checkCaseFolding();
#endif
#if ME_GOAHEAD_AUTH
    problems += checkRouteAuth();
#else
    warnUnauthenticated();
#endif
    return problems;
}


/*
    Legacy entry point. Retained so existing callers keep working; websCheckRoutes performs this check
    and the route shadowing check.
 */
PUBLIC void websCheckRouteAuth(void)
{
    (void) websCheckRoutes();
}


PUBLIC void websSetStrict(bool on)
{
    strictMode = on;
}


PUBLIC bool websGetStrict(void)
{
    return strictMode;
}


PUBLIC char *websAbsolutePath(cchar *path)
{
    char cwd[ME_GOAHEAD_LIMIT_FILENAME], *result, *cp, *tail, *normalized;
    int  absolute;

    absolute = (*path == '/' || *path == '\\');
#if ME_WIN_LIKE
    if (isalpha((uchar) path[0]) && path[1] == ':') {
        absolute = 1;
    }
#endif
    if (absolute) {
        result = sclone(path);
    } else if (getcwd(cwd, sizeof(cwd)) == NULL) {
        result = sclone(path);
    } else {
        result = sfmt("%s/%s", cwd, path);
    }
    for (cp = result; *cp; cp++) {
        if (*cp == '\\') {
            *cp = '/';
        }
    }
    /*
        Normalize after the drive or UNC prefix; clamp .. at the root.
     */
    cp = result;
#if ME_WIN_LIKE
    if (isalpha((uchar) cp[0]) && cp[1] == ':') {
        cp += 2;
    } else if (cp[0] == '/' && cp[1] == '/') {
        cp++;
    }
#endif
    if (*cp == '/') {
        tail = sclone(cp);
        *cp = '\0';
        normalized = websNormalizeUriPath(tail);
        cp = sfmt("%s%s%s", result, *normalized == '/' ? "" : "/", normalized);
        wfree(result);
        wfree(normalized);
        wfree(tail);
        result = cp;
    }
    /*
        "/" is its own trailing separator and must keep it, or every path compares as inside it.
     */
    for (cp = &result[slen(result) - 1]; cp > result && *cp == '/'; cp--) {
        *cp = '\0';
    }
    return result;
}


/*
    Compare configured directories using filesystem case rules. Symlink aliases are not detected.
 */
static int dirContains(cchar *dir, cchar *child)
{
    char  *parent, *sub;
    ssize len;
    int   rc;

    parent = websAbsolutePath(dir);
    sub = websAbsolutePath(child);
    len = slen(parent);
    rc = 0;
    if (len > 0 && matchRoutePrefix(sub, parent, len) == 0) {
        /*
            "/tmp" contains "/tmp" and "/tmp/x", but not "/tmpdir". A parent of "/" ends in the
            separator itself, so the child's next character is already part of its own name.
         */
        rc = sub[len] == '\0' || sub[len] == '/' || parent[len - 1] == '/';
    }
    wfree(parent);
    wfree(sub);
    return rc;
}


/*
    Audit the directories used to stage request data - CGI stdin and stdout, upload and PUT staging -
    against the served directories. A temporary file inside a served directory is fetchable over HTTP,
    so one client's in-flight request body or CGI response is readable by another. Returns the number of
    problems found; websOpen refuses to start the server when this is non-zero, because the exposure is
    unauthenticated and there is no request-time check that could contain it.
 */
PUBLIC int websCheckTempDirs(void)
{
    struct { cchar *dir, *setting; } staging[3];
    struct stat sbuf;
    cchar       *docs;
    int         i, j, count, problems;

    problems = 0;
    count = 0;
    staging[count].dir = websGetTempDir();
    staging[count++].setting = "ME_GOAHEAD_TEMP_DIR";
    if (stat(staging[0].dir, &sbuf) < 0 || !S_ISDIR(sbuf.st_mode)) {
        error("Security error: the temporary directory \"%s\" does not exist or is not a directory. Set "
              "ME_GOAHEAD_TEMP_DIR to an existing directory outside the documents directory.", staging[0].dir);
        problems++;
    }
#if ME_GOAHEAD_UPLOAD
    staging[count].dir = ME_GOAHEAD_UPLOAD_DIR;
    staging[count++].setting = "ME_GOAHEAD_UPLOAD_DIR";
#endif
#if !ME_ROM
    staging[count].dir = ME_GOAHEAD_PUT_DIR;
    staging[count++].setting = "ME_GOAHEAD_PUT_DIR";
#endif

    for (i = 0; i < count; i++) {
        if (staging[i].dir == 0 || *staging[i].dir == '\0') {
            continue;
        }
        for (j = -1; j < routeCount; j++) {
            /*
                Routes without an explicit dir= serve the document root, so it is checked once here
                rather than once per route.
             */
            docs = (j < 0) ? websGetDocuments() : routes[j]->dir;
            if (docs == 0 || *docs == '\0') {
                continue;
            }
            if (dirContains(docs, staging[i].dir)) {
                error("Security error: %s \"%s\" is inside the served directory \"%s\". Request data "
                      "staged there - CGI output, uploads, PUT bodies - is served to any client that "
                      "names the file. Set %s to a directory outside the documents directory.",
                      staging[i].setting, staging[i].dir, docs, staging[i].setting);
                problems++;
                break;
            }
        }
    }
    return problems;
}


/*
    Recognize methods declared by routes instead of rejecting them as unimplemented.
 */
static void addKnownMethods(WebsHash methods)
{
    WebsKey *key;

    if (methods < 0) {
        return;
    }
    if (routeMethods < 0) {
        routeMethods = hashCreate(-1);
    }
    for (key = hashFirst(methods); key; key = hashNext(methods, key)) {
        hashEnter(routeMethods, key->name.value.string, valueInteger(0), 0);
    }
}


/*
    Methods are case-sensitive. A spelling the server does not implement, such as "head", is unknown.
 */
PUBLIC bool websKnownMethod(cchar *method)
{
    static cchar *standard[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS", "TRACE", 0 };
    int          i;

    if (method == 0) {
        return 0;
    }
    for (i = 0; standard[i]; i++) {
        if (smatch(method, standard[i])) {
            return 1;
        }
    }
    return routeMethods >= 0 && hashLookup(routeMethods, method) != 0;
}


PUBLIC int websOpenRoute(void)
{
    if ((handlers = hashCreate(-1)) < 0) {
        return -1;
    }
    websDefineHandler("continue", continueHandler, 0, 0, 0);
    websDefineHandler("redirect", redirectHandler, 0, 0, 0);
    return 0;
}


PUBLIC void websCloseRoute(void)
{
    WebsHandler *handler;
    WebsKey     *key;
    int         i;

    if (handlers >= 0) {
        for (key = hashFirst(handlers); key; key = hashNext(handlers, key)) {
            handler = key->content.value.symbol;
            if (handler->close) {
                (*handler->close)();
            }
            wfreeset(handler->name);
            wfree(handler);
        }
        hashFree(handlers);
        handlers = -1;
    }
    if (routes) {
        for (i = 0; i < routeCount; i++) {
            freeRoute(routes[i]);
        }
        wfree(routes);
        routes = 0;
    }
    for (i = 0; i < retiredCount; i++) {
        freeRoute(retiredRoutes[i]);
    }
    retiredCount = 0;
    wfreeset(retiredRoutes);
    routeCount = routeMax = 0;
    if (routeMethods >= 0) {
        hashFree(routeMethods);
        routeMethods = -1;
    }
}


PUBLIC int websDefineHandler(cchar *name, WebsHandlerProc match, WebsHandlerProc service, WebsHandlerClose close,
                             int flags)
{
    WebsHandler *handler;

    assert(name && *name);

    if ((handler = walloc(sizeof(WebsHandler))) == 0) {
        return -1;
    }
    memset(handler, 0, sizeof(WebsHandler));
    handler->name = sclone(name);
    handler->match = match;
    handler->service = service;
    handler->close = close;
    handler->flags = flags;
#if ME_GOAHEAD_LEGACY
    if (flags & WEBS_LEGACY_HANDLER) {
        /* The caller cast a legacy service to WebsHandlerProc. Restore its own type. */
        handler->legacy = (WebsLegacyHandlerProc) (void (*)(void)) service;
    }
#endif

    hashEnter(handlers, name, valueSymbol(handler), 0);
    return 0;
}


/*
    Split required keys on spaces, tabs, and commas; accepted alternatives also split on |.
 */
static void addOption(WebsHash *hash, char *keys, cchar *sep, char *value)
{
    char *key, *tok;

    if (*hash < 0) {
        *hash = hashCreate(-1);
    }
    for (key = stok(keys, sep, &tok); key; key = stok(0, sep, &tok)) {
        if (strcmp(key, "none") == 0) {
            continue;
        }
        if (value == 0) {
            hashEnter(*hash, key, valueInteger(0), 0);
        } else {
            hashEnter(*hash, key, valueString(value, VALUE_ALLOCATE), 0);
        }
    }
}


#if !ME_GOAHEAD_AUTH
/*
    Refuse a configuration directive that describes an access control policy this build cannot
    enforce. Loading it and serving the resource unprotected leaves the operator with a configuration
    that reads as if authentication were in force while every protected route is public; declining to
    start is strictly the better failure. Returns -1 for the caller's result code.
 */
static int rejectAuthDirective(cchar *directive)
{
    error("Configuration directive \"%s\" requires ME_GOAHEAD_AUTH, but this build cannot enforce "
          "authentication. Refusing the configuration rather than serving it without the access "
          "control it describes. Rebuild with ME_GOAHEAD_AUTH=1 or remove the directive.", directive);
    return -1;
}
#endif


/*
    Load route and authentication configuration files
 */
PUBLIC int websLoad(cchar *path)
{
    WebsRoute *route;
    WebsHash  abilities, extensions, methods, redirects;
    char      *buf, *line, *kind, *next, *auth, *dir, *handler, *protocol, *uri, *option, *key, *value, *status;
    char      *redirectUri, *token;
    int       rc, badKeyword, flags;

    assert(path && *path);

    rc = 0;
    badKeyword = 0;
    if ((buf = websReadWholeFile(path)) == 0) {
        error("Cannot open config file %s", path);
        return -1;
    }
    for (line = stok(buf, "\r\n", &token); line; line = stok(NULL, "\r\n", &token)) {
        kind = stok(line, " \t", &next);
        if (kind == 0 || *kind == '\0' || *kind == '#') {
            continue;
        }
        if (smatch(kind, "route")) {
            auth = dir = handler = protocol = uri = 0;
            abilities = extensions = methods = redirects = -1;
            flags = 0;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "abilities")) {
                    addOption(&abilities, value, " \t,", 0);
                } else if (smatch(key, "auth")) {
                    auth = value;
                } else if (smatch(key, "dir")) {
                    /*
                        Reject empty directories, which would expose the filesystem root.
                     */
                    if (value == 0 || *value == '\0') {
                        error("Route dir= must name a directory");
                        badKeyword = 1;
                    }
                    dir = value;
                } else if (smatch(key, "extensions")) {
                    addOption(&extensions, value, " \t,|", 0);
                } else if (smatch(key, "handler")) {
                    handler = value;
                } else if (smatch(key, "methods")) {
                    addOption(&methods, value, " \t,|", 0);
                } else if (smatch(key, "redirect")) {
                    if (strchr(value, '@')) {
                        status = stok(value, "@", &redirectUri);
                        if (smatch(status, "*")) {
                            status = "0";
                        }
                    } else {
                        status = "0";
                        redirectUri = value;
                    }
                    if (smatch(redirectUri, "https")) redirectUri = "https://";
                    if (smatch(redirectUri, "http")) redirectUri = "http://";
                    addOption(&redirects, status, " \t,|", redirectUri);
                } else if (smatch(key, "cross-site")) {
                    if (smatch(value, "allow")) {
                        flags |= WEBS_ROUTE_CROSS_SITE;
                    } else {
                        error("Route cross-site= must be allow");
                        badKeyword = 1;
                    }
                } else if (smatch(key, "protocol")) {
                    protocol = value;
                } else if (smatch(key, "upload")) {
                    if (smatch(value, "true")) {
                        flags |= WEBS_ROUTE_UPLOAD;
                    } else if (!smatch(value, "false")) {
                        error("Route upload= must be true or false");
                        badKeyword = 1;
                    }
                } else if (smatch(key, "uri")) {
                    uri = value;
                } else {
                    error("Bad route keyword %s", key);
                    badKeyword = 1;
                }
            }
            /*
                Reject unknown qualifiers so typos cannot silently remove route protection.
             */
            if (badKeyword) {
                rc = -1;
                break;
            }
            if ((route = websAddRoute(uri, handler, -1)) == 0) {
                rc = -1;
                break;
            }
            route->flags |= flags;
            if (websSetRouteMatch(route, dir, protocol, methods, extensions, abilities, redirects) < 0) {
                rc = -1;
                break;
            }
#if ME_GOAHEAD_AUTH
            if (auth && websSetRouteAuth(route, auth) < 0) {
                rc = -1;
                break;
            }
#else
            if (auth) {
                rc = rejectAuthDirective("auth");
                break;
            }
#endif

        } else if (smatch(kind, "user")) {
#if ME_GOAHEAD_AUTH
            char *name, *password, *roles;
            name = password = roles = 0;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "name")) {
                    name = value;
                } else if (smatch(key, "password")) {
                    password = value;
                } else if (smatch(key, "roles")) {
                    roles = value;
                } else {
                    error("Bad user keyword %s", key);
                    badKeyword = 1;
                }
            }
            if (badKeyword) {
                rc = -1;
                break;
            }
            if (websAddUser(name, password, roles) == 0) {
                rc = -1;
                break;
            }
#else
            rc = rejectAuthDirective(kind);
            break;
#endif

        } else if (smatch(kind, "role")) {
#if ME_GOAHEAD_AUTH
            char *name;
            name = 0;
            abilities = -1;
            while ((option = stok(NULL, " \t\r\n", &next)) != 0) {
                key = stok(option, "=", &value);
                if (smatch(key, "name")) {
                    name = value;
                } else if (smatch(key, "abilities")) {
                    addOption(&abilities, value, " \t,|", 0);
                } else {
                    error("Bad role keyword %s", key);
                    badKeyword = 1;
                }
            }
            if (badKeyword) {
                rc = -1;
                break;
            }
            if (websAddRole(name, abilities) == 0) {
                rc = -1;
                break;
            }
#else
            rc = rejectAuthDirective(kind);
            break;
#endif

        } else {
            error("Unknown route keyword %s", kind);
            rc = -1;
            break;
        }
    }
    wfree(buf);
#if ME_GOAHEAD_AUTH
    websComputeAllUserAbilities();
#endif
    return rc;
}


/*
    Handler to just continue matching other routes
 */
static bool continueHandler(Webs *wp)
{
    return 0;
}


/*
    Handler to redirect to the default (code zero) URI
 */
static bool redirectHandler(Webs *wp)
{
    return websRedirectByStatus(wp, 0) == 0;
}


#if ME_GOAHEAD_LEGACY
PUBLIC int websUrlHandlerDefine(cchar *prefix, cchar *dir, int arg, WebsLegacyHandlerProc handler, int flags)
{
    WebsRoute   *route;
    WebsHandler *def;
    static int  legacyCount = 0;
    char        name[ME_GOAHEAD_LIMIT_STRING];

    assert(prefix && *prefix);
    assert(handler);

    fmt(name, sizeof(name), "%s-%d", prefix, legacyCount);
    if (websDefineHandler(name, 0, 0, 0, WEBS_LEGACY_HANDLER) < 0) {
        return -1;
    }
    def = hashLookupSymbol(handlers, name);
    def->legacy = handler;
    if ((route = websAddRoute(prefix, name, 0)) == 0) {
        return -1;
    }
    if (dir) {
        route->dir = sclone(dir);
    }
    return 0;
}


PUBLIC int websPublish(cchar *prefix, cchar *dir)
{
    WebsRoute *route;

    if ((route = websAddRoute(prefix, 0, 0)) == 0) {
        return -1;
    }
    route->dir = sclone(dir);
    return 0;
}
#endif


/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/runtime.c ************/

/*
    runtime.c -- Runtime support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/
/*
    This structure stores scheduled events.
 */
typedef struct Callback {
    void (*routine)(void *arg, int id);
    void *arg;
    WebsTime at;
    int id;
} Callback;

/*********************************** Defines **********************************/
/*
    Class definitions
 */
#define CLASS_NORMAL    0               /* [All other]      Normal characters */
#define CLASS_PERCENT   1               /* [%]              Begin format */
#define CLASS_MODIFIER  2               /* [-+ #,]          Modifiers */
#define CLASS_ZERO      3               /* [0]              Special modifier - zero pad */
#define CLASS_STAR      4               /* [*]              Width supplied by arg */
#define CLASS_DIGIT     5               /* [1-9]            Field widths */
#define CLASS_DOT       6               /* [.]              Introduce precision */
#define CLASS_BITS      7               /* [hlLz]           Length bits */
#define CLASS_TYPE      8               /* [cdefgiopsSuxX]  Type specifiers ('n' excluded) */

#define STATE_NORMAL    0               /* Normal chars in format string */
#define STATE_PERCENT   1               /* "%" */
#define STATE_MODIFIER  2               /* -+ #,*/
#define STATE_WIDTH     3               /* Width spec */
#define STATE_DOT       4               /* "." */
#define STATE_PRECISION 5               /* Precision spec */
#define STATE_BITS      6               /* Size spec */
#define STATE_TYPE      7               /* Data type */
#define STATE_COUNT     8

PUBLIC char stateMap[] = {
    /*     STATES:  Normal Percent Modifier Width  Dot  Prec Bits Type */
    /* CLASS           0      1       2       3     4     5    6    7  */
    /* Normal   0 */ 0,     0,      0,      0,    0,    0,   0,   0,
    /* Percent  1 */ 1,     0,      1,      1,    1,    1,   1,   1,
    /* Modifier 2 */ 0,     2,      2,      0,    0,    0,   0,   0,
    /* Zero     3 */ 0,     2,      2,      3,    5,    5,   0,   0,
    /* Star     4 */ 0,     3,      3,      0,    5,    0,   0,   0,
    /* Digit    5 */ 0,     3,      3,      3,    5,    5,   0,   0,
    /* Dot      6 */ 0,     4,      4,      4,    0,    0,   0,   0,
    /* Bits     7 */ 0,     6,      6,      6,    6,    6,   6,   0,
    /* Types    8 */ 0,     7,      7,      7,    7,    7,   7,   0,
};

/*
    Format:         %[modifier][width][precision][bits][type]

    The Class map will map from a specifier letter to a state.
 */
PUBLIC char classMap[] = {
    /*   0  ' '    !     "     #     $     %     &     ' */
    2,    0,    0,    2,    0,    1,    0,    0,
    /*  07   (     )     *     +     ,     -     .     / */
    0,    0,    4,    2,    2,    2,    6,    0,
    /*  10   0     1     2     3     4     5     6     7 */
    3,    5,    5,    5,    5,    5,    5,    5,
    /*  17   8     9     :     ;     <     =     >     ? */
    5,    5,    0,    0,    0,    0,    0,    0,
    /*  20   @     A     B     C     D     E     F     G */
    8,    0,    0,    0,    0,    0,    0,    0,
    /*  27   H     I     J     K     L     M     N     O */
    0,    0,    0,    0,    7,    0,    8,    0,
    /*  30   P     Q     R     S     T     U     V     W */
    0,    0,    0,    8,    0,    0,    0,    0,
    /*  37   X     Y     Z     [     \     ]     ^     _ */
    8,    0,    0,    0,    0,    0,    0,    0,
    /*  40   '     a     b     c     d     e     f     g */
    0,    0,    0,    8,    8,    8,    8,    8,
    /*  47   h     i     j     k     l     m     n     o */
    7,    8,    0,    0,    7,    0,    0,    8,
    /*  50   p     q     r     s     t     u     v     w */
    8,    0,    0,    8,    0,    8,    0,    8,
    /*  57   x     y     z  */
    8,    0,    7,
};

/*
    Flags
 */
#define SPRINTF_LEFT       0x1          /* Left align */
#define SPRINTF_SIGN       0x2          /* Always sign the result */
#define SPRINTF_LEAD_SPACE 0x4          /* put leading space for +ve numbers */
#define SPRINTF_ALTERNATE  0x8          /* Alternate format */
#define SPRINTF_LEAD_ZERO  0x10         /* Zero pad */
#define SPRINTF_SHORT      0x20         /* 16-bit */
#define SPRINTF_LONG       0x40         /* 32-bit */
#define SPRINTF_INT64      0x80         /* 64-bit */
#define SPRINTF_COMMA      0x100        /* Thousand comma separators */
#define SPRINTF_UPPER_CASE 0x200        /* As the name says for numbers */

typedef struct Format {
    uchar *buf;
    uchar *endbuf;
    uchar *start;
    uchar *end;
    ssize growBy;
    ssize maxsize;
    int precision;
    int radix;
    int width;
    int flags;
    int len;
} Format;

/*
    Evaluate the character once even when the buffer is full, preserving pointer and vararg advancement.
 */
#define BPUT(fmt, c) \
    do { \
        char bputChar = (char) (c); \
        /* Less one to allow room for the null */ \
        if ((fmt)->end >= ((fmt)->endbuf - sizeof(char))) { \
            if (growBuf(fmt) > 0) { \
                *(fmt)->end++ = bputChar; \
            } \
        } else { \
            *(fmt)->end++ = bputChar; \
        } \
    } while (0)

#define BPUTNULL(fmt) \
    do { \
        if ((fmt)->end > (fmt)->endbuf) { \
            if (growBuf(fmt) > 0) { \
                *(fmt)->end = '\0'; \
            } \
        } else { \
            *(fmt)->end = '\0'; \
        } \
    } while (0)

/*
    The handle list stores the length of the list and the number of used handles in the first two words.  These are
    hidden from the caller by returning a pointer to the third word to the caller.
 */
#define H_LEN    0          /* First entry holds length of list */
#define H_USED   1          /* Second entry holds number of used */
#define H_OFFSET 2          /* Offset to real start of list */
#define H_INCR   16         /* Grow handle list in chunks this size */

#define RINGQ_LEN(bp) ((bp->servp > bp->endp) ? (bp->buflen + (bp->endp - bp->servp)) : (bp->endp - bp->servp))

typedef struct HashTable {  /* Symbol table descriptor */
    WebsKey **hash_table;   /* Allocated at run time */
    int inuse;              /* Is this entry in use */
    int size;               /* Size of the table below */
} HashTable;

#ifndef LOG_ERR
    #define LOG_ERR 0
#endif
#if ME_WIN_LIKE
PUBLIC void syslog(int priority, char *format, ...);
#endif

PUBLIC int logLevel;                /* Log verbosity level */

/************************************* Locals *********************************/

static Callback **callbacks;
static int      callbackMax;

static HashTable **sym;             /* List of symbol tables */
static int       symMax;            /* One past the max symbol table */

char *embedthisGoAheadCopyright = EMBEDTHIS_GOAHEAD_COPYRIGHT;

#if ME_GOAHEAD_LOGGING
static char *logPath;               /* Log file name */
/*
    Must start at -1, the same "not open" sentinel logClose() restores. Zero is descriptor 0, so any
    message emitted before logOpen() would be written into standard input rather than the log.
 */
static int logFd = -1;              /* Standard output or error log descriptor */
#if !ME_ROM
static WebsLogFile logFile = { NULL, -1, 0 };   /* Log file target */
#endif
#endif

/********************************** Forwards **********************************/

static int calcPrime(int size);
static ssize getBinBlockSize(ssize size);
static int hashIndex(HashTable *tp, cchar *name);
static WebsKey *hash(HashTable *tp, cchar *name);

#if ME_GOAHEAD_LOGGING
static void defaultLogHandler(int level, cchar *buf);
static WebsLogHandler logHandler = defaultLogHandler;
#endif

static int  getState(char c, int state);
static int  growBuf(Format *fmt);
static int  parseField(cchar **spec, char c);
static char *sprintfCore(char *buf, ssize maxsize, cchar *fmt, va_list arg);
static void outNum(Format *fmt, char *prefix, uint64 val);
static void outString(Format *fmt, char *str, ssize len);
#if ME_FLOAT
static void outFloat(Format *fmt, char specChar, double value);
#endif

/************************************* Code ***********************************/

PUBLIC int websRuntimeOpen(void)
{
    symMax = 0;
    sym = 0;
    srand((uint) time(NULL));
    return 0;
}


PUBLIC void websRuntimeClose(void)
{
}


/*
    This function is called when a scheduled process time has come.
 */
static void callEvent(int id)
{
    Callback *cp;

    assert(0 <= id && id < callbackMax);
    cp = callbacks[id];
    assert(cp);

    (cp->routine)(cp->arg, cp->id);
}


/*
    Schedule relative to epoch on clock failure so the event runs once the clock recovers.
 */
static WebsTime eventDue(int delay)
{
    WebsTime now;

    now = websTime();
    return ((delay + 500) / 1000) + (now < 0 ? 0 : now);
}


/*
    Schedule an event in delay milliseconds time. We will use 1 second granularity for webServer.
 */
PUBLIC int websStartEvent(int delay, WebsEventProc proc, void *arg)
{
    Callback *s;
    int      id;

    if ((id = wallocObject(&callbacks, &callbackMax, sizeof(Callback))) < 0) {
        return -1;
    }
    s = callbacks[id];
    s->routine = proc;
    s->arg = arg;
    s->id = id;

    /*
        Round the delay up to seconds.
     */
    s->at = eventDue(delay);
    return id;
}


PUBLIC void websRestartEvent(int id, int delay)
{
    Callback *s;

    if (callbacks == NULL || id == -1 || id >= callbackMax || (s = callbacks[id]) == NULL) {
        return;
    }
    s->at = eventDue(delay);
}


PUBLIC void websStopEvent(int id)
{
    Callback *s;

    if (callbacks == NULL || id == -1 || id >= callbackMax || (s = callbacks[id]) == NULL) {
        return;
    }
    wfree(s);
    callbackMax = wfreeHandle(&callbacks, id);
}


int websRunEvents(void)
{
    Callback *s;
    WebsTime now;
    int      i, delay, nextEvent;

    nextEvent = (MAXINT / 1000);
    if ((now = websTime()) < 0) {
        /* No event can be timed without a clock. Poll until it returns */
        return 1000;
    }

    for (i = 0; i < callbackMax; i++) {
        if ((s = callbacks[i]) != NULL) {
            if (s->at <= now) {
                callEvent(i);
                delay = MAXINT / 1000;
                /* Rescan in case event scheduled or modified an event */
                i = -1;
            } else {
                delay = (int) min(s->at - now, MAXINT / 1000);
            }
            nextEvent = min(delay, nextEvent);
        }
    }
    return nextEvent * 1000;
}


/*
    Allocating secure replacement for sprintf and vsprintf.
 */
PUBLIC char *sfmt(cchar *format, ...)
{
    va_list ap;
    char    *result;

    assert(format);

    va_start(ap, format);
    result = sprintfCore(NULL, -1, format, ap);
    va_end(ap);
    return result;
}


/*
    Replacement for sprintf
 */
PUBLIC char *fmt(char *buf, ssize bufsize, cchar *format, ...)
{
    va_list ap;
    char    *result;

    assert(buf);
    assert(format);

    if (bufsize <= 0) {
        return 0;
    }
    va_start(ap, format);
    result = sprintfCore(buf, bufsize, format, ap);
    va_end(ap);
    return result;
}


/*
    Secure vsprintf replacement
 */
PUBLIC char *sfmtv(cchar *fmt, va_list arg)
{
    assert(fmt);
    return sprintfCore(NULL, -1, fmt, arg);
}


/*
    Parse width or precision with saturation at INT_MAX; leave spec after the digits.
 */
static int parseField(cchar **spec, char c)
{
    cchar *cp;
    int   value;

    value = 0;
    for (cp = *spec; isdigit((uchar) c); c = *cp++) {
        if (value > (INT_MAX - (c - '0')) / 10) {
            value = INT_MAX;
        } else {
            value = value * 10 + (c - '0');
        }
    }
    *spec = cp - 1;
    return value;
}


static int getState(char c, int state)
{
    int chrClass;

    if (c < ' ' || c > 'z') {
        chrClass = CLASS_NORMAL;
    } else {
        assert((c - ' ') < (int) sizeof(classMap));
        chrClass = classMap[(c - ' ')];
    }
    assert((chrClass * STATE_COUNT + state) < (int) sizeof(stateMap));
    state = stateMap[chrClass * STATE_COUNT + state];
    return state;
}


static char *sprintfCore(char *buf, ssize maxsize, cchar *spec, va_list args)
{
    Format fmt;
    ssize  len;
    int64  iValue;
    uint64 uValue;
    int    state;
    char   c, *safe;

    memset(&fmt, 0, sizeof(fmt));
    if (spec == 0) {
        spec = "";
    }
    if (buf != 0) {
        assert(maxsize > 0);
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[maxsize];
        fmt.growBy = -1;
    } else {
        if (maxsize <= 0) {
            maxsize = MAXINT;
        }
        len = min(256, maxsize);
        if ((buf = walloc(len)) == 0) {
            return 0;
        }
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[len];
        fmt.growBy = min(512, maxsize - len);
    }
    fmt.maxsize = maxsize;
    fmt.start = fmt.buf;
    fmt.end = fmt.buf;
    fmt.len = 0;
    *fmt.start = '\0';

    state = STATE_NORMAL;

    while ((c = *spec++) != '\0') {
        state = getState(c, state);

        switch (state) {
        case STATE_NORMAL:
            BPUT(&fmt, c);
            break;

        case STATE_PERCENT:
            fmt.precision = -1;
            fmt.width = 0;
            fmt.flags = 0;
            break;

        case STATE_MODIFIER:
            switch (c) {
            case '+':
                fmt.flags |= SPRINTF_SIGN;
                break;
            case '-':
                fmt.flags |= SPRINTF_LEFT;
                break;
            case '#':
                fmt.flags |= SPRINTF_ALTERNATE;
                break;
            case '0':
                fmt.flags |= SPRINTF_LEAD_ZERO;
                break;
            case ' ':
                fmt.flags |= SPRINTF_LEAD_SPACE;
                break;
            case ',':
                fmt.flags |= SPRINTF_COMMA;
                break;
            }
            break;

        case STATE_WIDTH:
            if (c == '*') {
                fmt.width = va_arg(args, int);
                if (fmt.width < 0) {
                    fmt.width = (fmt.width == INT_MIN) ? INT_MAX : -fmt.width;
                    fmt.flags |= SPRINTF_LEFT;
                }
            } else {
                fmt.width = parseField(&spec, c);
            }
            fmt.width = min(fmt.width, ME_GOAHEAD_LIMIT_FORMAT_FIELD);
            break;

        case STATE_DOT:
            fmt.precision = 0;
            break;

        case STATE_PRECISION:
            if (c == '*') {
                fmt.precision = va_arg(args, int);
            } else {
                fmt.precision = parseField(&spec, c);
            }
            break;

        case STATE_BITS:
            switch (c) {
            case 'L':
                fmt.flags |= SPRINTF_INT64;
                break;

            case 'l':
                if (fmt.flags & SPRINTF_LONG) {
                    //  "ll" is long long: 64 bits on every platform
                    fmt.flags &= ~SPRINTF_LONG;
                    fmt.flags |= SPRINTF_INT64;
                } else {
                    fmt.flags |= SPRINTF_LONG;
                }
                break;

            case 'h':
                fmt.flags |= SPRINTF_SHORT;
                break;

            case 'z':
                //  ssize and size_t: 64 bits on 64-bit systems, else the size of an int
#if ME_64
                fmt.flags |= SPRINTF_INT64;
#endif
                break;
            }
            break;

        case STATE_TYPE:
            switch (c) {
            case 'e':
#if ME_FLOAT
            case 'g':
            case 'f':
                fmt.radix = 10;
                outFloat(&fmt, c, (double) va_arg(args, double));
                break;
#endif /* ME_FLOAT */

            case 'c':
                BPUT(&fmt, (char) va_arg(args, int));
                break;

            case 'S':
                /* Safe string */
                safe = websEscapeHtml(va_arg(args, char*));
                outString(&fmt, safe, -1);
                wfree(safe);
                break;

            case 'w':
            /* Wide string of wchar characters (Same as %ls"). Null terminated. */
            /* Fall through */

            case 's':
                /* Standard string */
                outString(&fmt, va_arg(args, char*), -1);
                break;

            case 'i':
            case 'd':
                fmt.radix = 10;
                if (fmt.flags & SPRINTF_SHORT) {
                    iValue = (short) va_arg(args, int);
                } else if (fmt.flags & SPRINTF_LONG) {
                    iValue = (long) va_arg(args, long);
                } else if (fmt.flags & SPRINTF_INT64) {
                    iValue = (int64) va_arg(args, int64);
                } else {
                    iValue = (int) va_arg(args, int);
                }
                if (iValue >= 0) {
                    if (fmt.flags & SPRINTF_LEAD_SPACE) {
                        outNum(&fmt, " ", iValue);
                    } else if (fmt.flags & SPRINTF_SIGN) {
                        outNum(&fmt, "+", iValue);
                    } else {
                        outNum(&fmt, 0, iValue);
                    }
                } else {
                    outNum(&fmt, "-", -iValue);
                }
                break;

            case 'X':
                fmt.flags |= SPRINTF_UPPER_CASE;
#if ME_64
                fmt.flags &= ~(SPRINTF_SHORT | SPRINTF_LONG);
                fmt.flags |= SPRINTF_INT64;
#else
                fmt.flags &= ~(SPRINTF_INT64);
#endif
            /*  Fall through  */
            case 'o':
            case 'x':
            case 'u':
                if (fmt.flags & SPRINTF_SHORT) {
                    uValue = (ushort) va_arg(args, uint);
                } else if (fmt.flags & SPRINTF_LONG) {
                    uValue = (ulong) va_arg(args, ulong);
                } else if (fmt.flags & SPRINTF_INT64) {
                    uValue = (uint64) va_arg(args, uint64);
                } else {
                    uValue = va_arg(args, uint);
                }
                if (c == 'u') {
                    fmt.radix = 10;
                    outNum(&fmt, 0, uValue);
                } else if (c == 'o') {
                    fmt.radix = 8;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        outNum(&fmt, "0", uValue);
                    } else {
                        outNum(&fmt, 0, uValue);
                    }
                } else {
                    fmt.radix = 16;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        if (c == 'X') {
                            outNum(&fmt, "0X", uValue);
                        } else {
                            outNum(&fmt, "0x", uValue);
                        }
                    } else {
                        outNum(&fmt, 0, uValue);
                    }
                }
                break;

            /*
                The %n conversion is deliberately not implemented. It is the only printf
                conversion that writes through a caller-supplied pointer, so any format string
                that an attacker can influence becomes a write-what-where primitive. GoAhead has
                its own formatter, so neither platform defence applies: not the glibc
                _FORTIFY_SOURCE %n check, nor the MSVC CRT, which has refused %n by default
                since VS2005 unless _set_printf_count_output() re-enables it.
                'n' is mapped to CLASS_NORMAL in classMap, so "%n" emits a literal "n" - the same
                treatment any other unknown conversion gets - and consumes no vararg.
             */

            case 'p':       /* Pointer */
#if ME_64
                uValue = (uint64) va_arg(args, void*);
#else
                uValue = (uint) PTOI(va_arg(args, void*));
#endif
                fmt.radix = 16;
                outNum(&fmt, "0x", uValue);
                break;

            default:
                BPUT(&fmt, c);
            }
        }
    }
    BPUTNULL(&fmt);
    return (char*) fmt.buf;
}


static void outString(Format *fmt, char *str, ssize len)
{
    char  *cp;
    ssize i;

    /*
        Ignore # for strings.
     */
    if (str == NULL) {
        str = "null";
        len = 4;
    } else if (fmt->precision >= 0) {
        for (cp = str, len = 0; len < fmt->precision; len++) {
            if (*cp++ == '\0') {
                break;
            }
        }
    } else if (len < 0) {
        len = slen(str);
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
    for (i = 0; i < len && *str; i++) {
        BPUT(fmt, *str++);
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
}


static void outNum(Format *fmt, char *prefix, uint64 value)
{
    char numBuf[64];
    char *cp;
    char *endp;
    char c;
    int  letter, len, leadingZeros, i, fill;

    endp = &numBuf[sizeof(numBuf) - 1];
    *endp = '\0';
    cp = endp;

    /*
     *  Convert to ascii
     */
    if (fmt->radix == 16) {
        do {
            letter = (int) (value % fmt->radix);
            if (letter > 9) {
                if (fmt->flags & SPRINTF_UPPER_CASE) {
                    letter = 'A' + letter - 10;
                } else {
                    letter = 'a' + letter - 10;
                }
            } else {
                letter += '0';
            }
            *--cp = (char) letter;
            value /= fmt->radix;
        } while (value > 0);

    } else if (fmt->flags & SPRINTF_COMMA) {
        i = 1;
        do {
            *--cp = (char) ('0' + (int) (value % fmt->radix));
            value /= fmt->radix;
            if ((i++ % 3) == 0 && value > 0) {
                *--cp = ',';
            }
        } while (value > 0);
    } else {
        do {
            *--cp = (char) ('0' + (int) (value % fmt->radix));
            value /= fmt->radix;
        } while (value > 0);
    }

    len = (int) (endp - cp);
    fill = fmt->width - len;

    if (prefix != 0) {
        fill -= (int) slen(prefix);
    }
    leadingZeros = (fmt->precision > len) ? min(fmt->precision, ME_GOAHEAD_LIMIT_FORMAT_FIELD) - len : 0;
    fill -= leadingZeros;

    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(fmt, c);
        }
    }
    if (prefix != 0) {
        while (*prefix) {
            BPUT(fmt, *prefix++);
        }
    }
    for (i = 0; i < leadingZeros; i++) {
        BPUT(fmt, '0');
    }
    while (*cp) {
        BPUT(fmt, *cp);
        cp++;
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(fmt, ' ');
        }
    }
}


#if ME_FLOAT
static void outFloat(Format *fmt, char specChar, double value)
{
    char result[ME_GOAHEAD_LIMIT_STRING], *cp, c;
    int  fill, i, len, index, precision;

    // Apply width later so snprintf padding cannot crowd out the number.
    result[0] = '\0';
    len = 0;
    precision = min(fmt->precision, ME_GOAHEAD_LIMIT_FORMAT_FIELD);
    if (specChar == 'f') {
        len = snprintf(result, sizeof(result), "%.*f", precision, value);

    } else if (specChar == 'g') {
        len = snprintf(result, sizeof(result), "%.*g", precision, value);

    } else if (specChar == 'e') {
        len = snprintf(result, sizeof(result), "%.*e", precision, value);
    }
    len = (len < 0) ? 0 : min(len, (int) sizeof(result) - 1);
    fill = fmt->width - len;
    if (fmt->flags & SPRINTF_COMMA) {
        if (((len - 1) / 3) > 0) {
            fill -= (len - 1) / 3;
        }
    }

    if (fmt->flags & SPRINTF_SIGN && value > 0) {
        BPUT(fmt, '+');
        fill--;
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(fmt, c);
        }
    }
    index = len;
    for (cp = result; cp < &result[len]; cp++) {
        BPUT(fmt, *cp);
        if (fmt->flags & SPRINTF_COMMA) {
            if ((--index % 3) == 0 && index > 0) {
                BPUT(fmt, ',');
            }
        }
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(fmt, ' ');
        }
    }
    BPUTNULL(fmt);
}
#endif /* ME_FLOAT */


/*
    Grow the buffer to fit new data. Return 1 if the buffer can grow.
    Grow using the growBy size specified when creating the buffer.
 */
static int growBuf(Format *fmt)
{
    uchar *newbuf;
    ssize buflen, endOffset, startOffset;

    buflen = (int) (fmt->endbuf - fmt->buf);
    if (fmt->maxsize >= 0 && buflen >= fmt->maxsize) {
        return 0;
    }
    if (fmt->growBy <= 0) {
        /*
            User supplied buffer
         */
        return 0;
    }
    if ((newbuf = walloc(buflen + fmt->growBy)) == 0) {
        return -1;
    }
    /*
        Capture the offsets before releasing the old buffer so they are not derived from a freed pointer.
     */
    endOffset = fmt->end - fmt->buf;
    startOffset = fmt->start - fmt->buf;
    if (fmt->buf) {
        memcpy(newbuf, fmt->buf, buflen);
        wfreeset(fmt->buf);
    }
    buflen += fmt->growBy;
    fmt->end = newbuf + endOffset;
    fmt->start = newbuf + startOffset;
    fmt->buf = newbuf;
    fmt->endbuf = &fmt->buf[buflen];

    /*
        Increase growBy to reduce overhead
     */
    if ((buflen + (fmt->growBy * 2)) < fmt->maxsize) {
        fmt->growBy *= 2;
    }
    return 1;
}


WebsValue valueInteger(long value)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = integer;
    v.value.integer = value;
    return v;
}


WebsValue valueSymbol(void *value)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = symbol;
    v.value.symbol = value;
    return v;
}


WebsValue valueString(cchar *value, int flags)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = string;
    if (flags & VALUE_ALLOCATE) {
        v.allocated = 1;
        v.value.string = sclone(value);
    } else {
        v.allocated = 0;
        v.value.string = (char*) value;
    }
    return v;
}


PUBLIC void valueFree(WebsValue *v)
{
    if (v->valid && v->allocated && v->type == string && v->value.string != NULL) {
        wfreeset(v->value.string);
    }
    v->type = undefined;
    v->valid = 0;
    v->allocated = 0;
}

#if !ME_ROM
/*
    Append binary log data without following symlinks or inheriting the descriptor into CGI.
 */
PUBLIC int websOpenLogFile(cchar *path)
{
    int fd;

    if ((fd = open(path, O_CREAT | O_APPEND | O_WRONLY | O_BINARY | O_NOFOLLOW | O_CLOEXEC, 0640)) >= 0) {
        /* Some platforms don't implement O_APPEND (VXWORKS) */
        lseek(fd, 0, SEEK_END);
    }
    return fd;
}


PUBLIC int websStartLogFile(WebsLogFile *log, cchar *path)
{
    websCloseLogFile(log);
    if ((log->fd = websOpenLogFile(path)) < 0) {
        return -1;
    }
    log->path = sclone(path);
    log->size = (ssize) lseek(log->fd, 0, SEEK_END);
    return 0;
}



static void rotateLogFile(WebsLogFile *log)
{
    char *old;

    close(log->fd);
    log->fd = -1;
    old = sfmt("%s.old", log->path);
#if ME_WIN_LIKE
    unlink(old);
#endif
    if (rename(log->path, old) != 0) {
        unlink(log->path);
    }
    wfree(old);
    log->fd = websOpenLogFile(log->path);
    log->size = 0;
}


PUBLIC void websWriteLogFile(WebsLogFile *log, cchar *buf, ssize len)
{
    ssize written;

    if (log->fd < 0) {
        return;
    }
    if (ME_GOAHEAD_LIMIT_LOG_SIZE > 0 && log->size > 0 && log->size + len > ME_GOAHEAD_LIMIT_LOG_SIZE) {
        rotateLogFile(log);
        if (log->fd < 0) {
            return;
        }
    }
    /* A write that fails has nowhere to be reported, so it is dropped. */
    while (len > 0 && (written = write(log->fd, buf, (int) len)) > 0) {
        buf += written;
        len -= written;
        log->size += written;
    }
}


PUBLIC void websCloseLogFile(WebsLogFile *log)
{
    if (log->fd >= 0) {
        close(log->fd);
    }
    log->fd = -1;
    log->size = 0;
    wfreeset(log->path);
}
#endif


#if ME_GOAHEAD_LOGGING
/*
    Write a whole buffer to the log. A write that fails has nowhere to be reported, so it is dropped.
 */
static void logWrite(int fd, cchar *buf, ssize len)
{
    ssize written;

    while (len > 0 && (written = write(fd, buf, (int) len)) > 0) {
        buf += written;
        len -= written;
    }
}


static void defaultLogHandler(int flags, cchar *buf)
{
    char *line;

    if (flags & WEBS_RAW_MSG) {
        line = sclone(buf);
    } else {
        line = sfmt("%s: %d: %s\n", ME_NAME, flags & WEBS_LEVEL_MASK, buf);
    }
#if !ME_ROM
    if (logFile.fd >= 0) {
        websWriteLogFile(&logFile, line, slen(line));
    } else
#endif
    {
        /*
            Before logOpen() and after logClose() there is no log to write to, but the messages still
            matter: goahead.c and test.c report fatal command line errors and exit before the log is
            ever configured. Fall back to stderr so those reach the operator instead of vanishing.
         */
        logWrite((logFd >= 0) ? logFd : 2, line, slen(line));
    }
    wfree(line);
#if ME_WIN_LIKE || ME_UNIX_LIKE
    if ((flags & WEBS_ERROR_MSG) && !(flags & WEBS_RAW_MSG) && websGetBackground()) {
        syslog(LOG_ERR, "%s", buf);
    }
#endif
}


/* Escape request-controlled log bytes in every handler. Raw messages retain newlines, CRs and tabs. */
static void logOutput(int flags, cchar *message)
{
    char *escaped;

    escaped = websEscapeControl(message, (flags & WEBS_RAW_MSG) ? "\n\r\t" : NULL, NULL);
    logHandler(flags, escaped);
    wfree(escaped);
}


PUBLIC void error(cchar *fmt, ...)
{
    va_list args;
    char    *message;

    if (!logHandler) {
        return;
    }
    va_start(args, fmt);
    message = sfmtv(fmt, args);
    va_end(args);
    logOutput(WEBS_ERROR_MSG, message);
    wfree(message);
}


PUBLIC void assertError(WEBS_ARGS_DEC, cchar *fmt, ...)
{
    va_list args;
    char    *fmtBuf, *message;

    va_start(args, fmt);
    fmtBuf = sfmtv(fmt, args);

    message = sfmt("Assertion %s, failed at %s %d", fmtBuf, WEBS_ARGS);
    va_end(args);
    wfree(fmtBuf);
    if (logHandler) {
        logOutput(WEBS_ASSERT_MSG, message);
    }
    wfree(message);
}


PUBLIC void logmsgProc(int level, cchar *fmt, ...)
{
    va_list args;
    char    *message;

    if ((level & WEBS_LEVEL_MASK) <= logLevel && logHandler) {
        va_start(args, fmt);
        message = sfmtv(fmt, args);
        logOutput(level | WEBS_LOG_MSG, message);
        wfree(message);
        va_end(args);
    }
}


PUBLIC void traceProc(int level, cchar *fmt, ...)
{
    va_list args;
    char    *message;

    if ((level & WEBS_LEVEL_MASK) <= logLevel && logHandler) {
        va_start(args, fmt);
        message = sfmtv(fmt, args);
        logOutput(level | WEBS_TRACE_MSG, message);
        wfree(message);
        va_end(args);
    }
}


PUBLIC int websGetLogLevel(void)
{
    return logLevel;
}


void websSetLogLevel(int level)
{
    logLevel = level;
}


WebsLogHandler logGetHandler(void)
{
    return logHandler;
}


/*
    Replace the default trace handler. Return a pointer to the old handler.
 */
WebsLogHandler logSetHandler(WebsLogHandler handler)
{
    WebsLogHandler oldHandler;

    oldHandler = logHandler;
    if (handler) {
        logHandler = handler;
    }
    return oldHandler;
}


PUBLIC int logOpen(void)
{
    if (!logPath) {
        /* Default from the ME_GOAHEAD_LOGFILE define in goahead.h */
        logSetPath(ME_GOAHEAD_LOGFILE);
    }
#if !ME_ROM
    websCloseLogFile(&logFile);
#endif
    if (smatch(logPath, "stdout")) {
        logFd = 1;
    } else if (smatch(logPath, "stderr")) {
        logFd = 2;
#if !ME_ROM
    } else {
        if (websStartLogFile(&logFile, logPath) < 0) {
            return -1;
        }
#endif
    }
    logSetHandler(logHandler);
    return 0;
}


PUBLIC void logClose(void)
{
#if !ME_ROM
    websCloseLogFile(&logFile);
#endif
}


PUBLIC cchar *logGetPath(void)
{
    return logPath;
}


PUBLIC void logSetPath(cchar *path)
{
    char *lp;

    wfree(logPath);
    logPath = sclone(path);
    if ((lp = strchr(logPath, ':')) != 0) {
        *lp++ = '\0';
        logLevel = atoi(lp);
    }
}
#endif


/*
    Convert a string to lower case
 */
PUBLIC char *slower(char *str)
{
    char *s;

    if (str == NULL) {
        return NULL;
    }
    s = str;
    while (*s) {
        if (isupper((uchar) *s)) {
            *s = (char) tolower((uchar) *s);
        }
        s++;
    }
    *s = '\0';
    return str;
}


/*
    Convert a string to upper case
 */
PUBLIC char *supper(char *str)
{
    char *s;

    if (str == NULL) {
        return NULL;
    }
    s = str;
    while (*s) {
        if (islower((uchar) *s)) {
            *s = (char) toupper((uchar) *s);
        }
        s++;
    }
    *s = '\0';
    return str;
}


PUBLIC char *itosbuf(char *buf, ssize size, int64 value, int radix)
{
    char   *cp, *end;
    char   digits[] = "0123456789ABCDEF";
    uint64 uvalue;
    int    negative;

    if ((radix != 10 && radix != 16) || size < 2) {
        return 0;
    }
    end = cp = &buf[size];
    *--cp = '\0';

    if (value < 0) {
        /*
            Use an unsigned magnitude to handle INT64_MIN without overflow.
         */
        uvalue = (uint64) (-(value + 1)) + 1;
        negative = 1;
        size--;
    } else {
        uvalue = (uint64) value;
        negative = 0;
    }
    do {
        *--cp = digits[uvalue % (uint64) radix];
        uvalue /= (uint64) radix;
    } while (uvalue > 0 && cp > buf);

    if (negative) {
        if (cp <= buf) {
            return 0;
        }
        *--cp = '-';
    }
    if (buf < cp) {
        /* Move the null too */
        memmove(buf, cp, end - cp);
    }
    return buf;
}


/*
    Allocate a new file handle. On the first call, the caller must set the handle map to be a pointer to a null
    pointer.  map points to the second element in the handle array.
 */
PUBLIC int wallocHandle(void *mapArg)
{
    void  ***map;
    ssize *mp;
    int   handle, len, memsize, incr;

    map = (void***) mapArg;
    assert(map);

    if (*map == NULL) {
        incr = H_INCR;
        memsize = (incr + H_OFFSET) * sizeof(void*);
        if ((mp = walloc(memsize)) == NULL) {
            return -1;
        }
        memset(mp, 0, memsize);
        mp[H_LEN] = incr;
        mp[H_USED] = 0;
        *map = (void*) &mp[H_OFFSET];
    } else {
        mp = &((*(ssize**) map)[-H_OFFSET]);
    }
    len = (int) mp[H_LEN];

    /*
        Find the first null handle
     */
    if (mp[H_USED] < mp[H_LEN]) {
        for (handle = 0; handle < len; handle++) {
            if (mp[handle + H_OFFSET] == 0) {
                mp[H_USED]++;
                return handle;
            }
        }
    } else {
        handle = len;
    }

    /*
        No free handle so grow the handle list. Grow list in chunks of H_INCR.
     */
    len += H_INCR;
    memsize = (len + H_OFFSET) * sizeof(void*);
    if ((mp = wrealloc(mp, memsize)) == NULL) {
        return -1;
    }
    *map = (void*) &mp[H_OFFSET];
    mp[H_LEN] = len;
    memset(&mp[H_OFFSET + len - H_INCR], 0, sizeof(ssize*) * H_INCR);
    mp[H_USED]++;
    return handle;
}


/*
    Free a handle. This function returns the value of the largest handle in use plus 1, to be saved as a max value.
 */
PUBLIC int wfreeHandle(void *mapArg, int handle)
{
    void  ***map;
    ssize *mp;
    int   len;

    map = (void***) mapArg;
    assert(map);
    mp = &((*(ssize**) map)[-H_OFFSET]);
    assert(mp[H_LEN] >= H_INCR);

    assert(mp[handle + H_OFFSET]);
    assert(mp[H_USED]);
    mp[handle + H_OFFSET] = 0;
    if (--(mp[H_USED]) == 0) {
        wfree((void*) mp);
        *map = NULL;
    }
    /*
        Find the greatest handle number in use.
     */
    if (*map == NULL) {
        handle = -1;
    } else {
        len = (int) mp[H_LEN];
        if (mp[H_USED] < mp[H_LEN]) {
            for (handle = len - 1; handle >= 0; handle--) {
                if (mp[handle + H_OFFSET])
                    break;
            }
        } else {
            handle = len;
        }
    }
    return handle + 1;
}


/*
    Allocate an entry in the halloc array
 */
PUBLIC int wallocObject(void *listArg, int *max, int size)
{
    void ***list;
    char *cp;
    int  id;

    list = (void***) listArg;
    assert(list);
    assert(max);

    if ((id = wallocHandle(list)) < 0) {
        return -1;
    }
    if (size > 0) {
        if ((cp = walloc(size)) == NULL) {
            wfreeHandle(list, id);
            return -1;
        }
        assert(cp);
        memset(cp, 0, size);
        (*list)[id] = (void*) cp;
    }
    if (id >= *max) {
        *max = id + 1;
    }
    return id;
}


/*
    Create a new buf. "increment" is the amount to increase the size of the buf should it need to grow to accommodate
    data being added. "maxsize" is an upper limit (sanity level) beyond which the buffer must not grow. Set maxsize to
       -1 to
    imply no upper limit. The buffer for the buf is always *  dynamically allocated. Set maxsize
 */
PUBLIC int bufCreate(WebsBuf *bp, ssize initSize, ssize maxsize)
{
    ssize increment;

    assert(bp);

    /*
        Only -1 means unlimited; reject other negative limits.
     */
    if (maxsize < -1) {
        return -1;
    }
    if (initSize <= 0) {
        initSize = ME_GOAHEAD_LIMIT_BUFFER;
    }
    if (maxsize == 0) {
        maxsize = initSize;
    }
    assert(initSize >= 0);
    memset(bp, 0, sizeof(WebsBuf));

    increment = getBinBlockSize(initSize);
    if ((bp->buf = walloc((increment))) == NULL) {
        return -1;
    }
    bp->maxsize = maxsize;
    bp->buflen = increment;
    bp->increment = increment;
    bp->endbuf = &bp->buf[bp->buflen];
    bp->servp = bp->buf;
    bp->endp = bp->buf;
    *bp->servp = '\0';
    return 0;
}


/*
    Delete a buf and free the buf buffer.
 */
PUBLIC void bufFree(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp == NULL) {
        return;
    }
    bufFlush(bp);
    wfree((char*) bp->buf);
    bp->buf = NULL;
    bp->servp = NULL;
    bp->endp = NULL;
    bp->buflen = 0;
}


/*
    Return the length of the data in the buf. Users must fill the queue to a high water mark of at most one less than
    the queue size.
 */
PUBLIC ssize bufLen(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp > bp->endp) {
        return bp->buflen + bp->endp - bp->servp;
    } else {
        return bp->endp - bp->servp;
    }
}


/*
    Move wrapped ring data to the front before NUL-terminating. One byte is reserved for the terminator.
 */
PUBLIC char *bufStart(WebsBuf *bp)
{
    char  *data;
    ssize len, head;

    assert(bp);
    if (bp->buf && bp->servp > bp->endp) {
        len = bufLen(bp);
        head = bp->endbuf - bp->servp;
        data = walloc(len);
        memcpy(data, bp->servp, head);
        memcpy(&data[head], bp->buf, len - head);
        memcpy(bp->buf, data, len);
        wfree(data);
        bp->servp = bp->buf;
        bp->endp = bp->buf + len;
    }
    bufAddNull(bp);
    return bp->servp;
}


/*
    Get a byte from the queue
 */
PUBLIC int bufGetc(WebsBuf *bp)
{
    char c, *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp == bp->endp) {
        return -1;
    }

    cp = (char*) bp->servp;
    c = *cp++;
    bp->servp = cp;
    if (bp->servp >= bp->endbuf) {
        bp->servp = bp->buf;
    }
    return (int) ((uchar) c);
}


/*
    Add a char to the queue. Note if being used to store wide strings this does not add a trailing '\0'. Grow the buffer
       as
    required.
 */
PUBLIC int bufPutc(WebsBuf *bp, char c)
{
    char *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if ((bufRoom(bp) < (int) sizeof(char)) && !bufGrow(bp, 0)) {
        return -1;
    }
    cp = (char*) bp->endp;
    *cp++ = (char) c;
    bp->endp = cp;
    if (bp->endp >= bp->endbuf) {
        bp->endp = bp->buf;
    }
    return 0;
}


/*
    Insert a wide character at the front of the queue
 */
PUBLIC int bufInsertc(WebsBuf *bp, char c)
{
    char *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) < (int) sizeof(char) && !bufGrow(bp, 0)) {
        return -1;
    }
    if (bp->servp <= bp->buf) {
        bp->servp = bp->endbuf;
    }
    cp = (char*) bp->servp;
    *--cp = (char) c;
    bp->servp = cp;
    return 0;
}


PUBLIC ssize bufPut(WebsBuf *bp, cchar *fmt, ...)
{
    va_list ap;
    char    *str;
    ssize   rc;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (fmt == 0) {
        return 0;
    }
    va_start(ap, fmt);
    str = sfmtv(fmt, ap);
    va_end(ap);

    rc = bufPutBlk(bp, str, strlen(str) * sizeof(char));
    wfree(str);
    if (rc) {
        *((char*) bp->endp) = (char) '\0';
    }
    return rc;
}


/*
    Add a string to the queue. Add a trailing null (maybe two nulls)
 */
PUBLIC ssize bufPutStr(WebsBuf *bp, cchar *str)
{
    ssize rc;

    assert(bp);
    assert(str);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    rc = bufPutBlk(bp, str, strlen(str) * sizeof(char));
    if (rc) {
        *((char*) bp->endp) = (char) '\0';
    }
    return rc;
}


/*
    Add a null terminator. This does NOT increase the size of the queue
 */
PUBLIC void bufAddNull(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    /*
        The ring keeps one byte free, so endp is always inside the buffer and the terminator fits even if the
        buffer cannot grow.
     */
    if (bufRoom(bp) < (int) sizeof(char)) {
        bufGrow(bp, 0);
    }
    *((char*) bp->endp) = (char) '\0';
}


#if UNICODE
/*
    Get a byte from the queue
 */
PUBLIC int bufGetcA(WebsBuf *bp)
{
    uchar c;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp == bp->endp) {
        return -1;
    }

    c = *bp->servp++;
    if (bp->servp >= bp->endbuf) {
        bp->servp = bp->buf;
    }
    return c;
}


/*
    Add a byte to the queue. Note if being used to store strings this does not add a trailing '\0'.
    Grow the buffer as required.
 */
PUBLIC int bufPutcA(WebsBuf *bp, char c)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) == 0 && !bufGrow(bp)) {
        return -1;
    }
    *bp->endp++ = (uchar) c;
    if (bp->endp >= bp->endbuf) {
        bp->endp = bp->buf;
    }
    return 0;
}


/*
    Insert a byte at the front of the queue
 */
PUBLIC int bufInsertcA(WebsBuf *bp, char c)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) == 0 && !bufGrow(bp)) {
        return -1;
    }
    if (bp->servp <= bp->buf) {
        bp->servp = bp->endbuf;
    }
    *--bp->servp = (uchar) c;
    return 0;
}


/*
    Add a string to the queue. Add a trailing null (not really in the q). ie. beyond the last valid byte.
 */
PUBLIC int bufPutStrA(WebsBuf *bp, char *str)
{
    int rc;

    assert(bp);
    assert(str);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    rc = (int) bufPutBlk(bp, str, strlen(str));
    bp->endp[0] = '\0';
    return rc;
}
#endif


/*
    Add a block of data to the buf. Return the number of bytes added. Grow the buffer as required.
 */
PUBLIC ssize bufPutBlk(WebsBuf *bp, cchar *buf, ssize size)
{
    ssize this, added;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(buf);
    assert(0 <= size);

    /*
        Loop adding the maximum bytes we can add in a single straight line copy
     */
    added = 0;
    while (size > 0) {
        this = min(bufRoom(bp), size);
        if (this <= 0) {
            if (!bufGrow(bp, 0)) {
                break;
            }
            this = min(bufRoom(bp), size);
        }
        memcpy(bp->endp, buf, this);
        buf += this;
        bp->endp += this;
        size -= this;
        added += this;
        if (bp->endp >= bp->endbuf) {
            bp->endp = bp->buf;
        }
    }
    return added;
}


/*
    Get a block of data from the buf. Return the number of bytes returned.
 */
PUBLIC ssize bufGetBlk(WebsBuf *bp, char *buf, ssize size)
{
    ssize this, bytes_read;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(buf);
    assert(0 <= size && size < bp->buflen);

    /*
        Loop getting the maximum bytes we can get in a single straight line copy
     */
    bytes_read = 0;
    while (size > 0) {
        this = bufGetBlkMax(bp);
        this = min(this, size);
        if (this <= 0) {
            break;
        }
        memcpy(buf, bp->servp, this);
        buf += this;
        bp->servp += this;
        size -= this;
        bytes_read += this;

        if (bp->servp >= bp->endbuf) {
            bp->servp = bp->buf;
        }
    }
    return bytes_read;
}


/*
    Return the maximum number of bytes the buffer can accept via a single block copy. Useful if the user is doing their
    own data insertion.
 */
PUBLIC ssize bufRoom(WebsBuf *bp)
{
    ssize space, in_a_line;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    space = bp->buflen - RINGQ_LEN(bp) - 1;
    in_a_line = bp->endbuf - bp->endp;

    return min(in_a_line, space);
}


/*
    Return the maximum number of bytes the buffer can provide via a single block copy. Useful if the user is doing their
    own data retrieval.
 */
PUBLIC ssize bufGetBlkMax(WebsBuf *bp)
{
    ssize len, in_a_line;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    len = RINGQ_LEN(bp);
    in_a_line = bp->endbuf - bp->servp;

    return min(in_a_line, len);
}


/*
    Adjust the endp pointer after the user has copied data into the queue.
 */
PUBLIC void bufAdjustEnd(WebsBuf *bp, ssize size)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(0 <= size && size < bp->buflen);

    bp->endp += size;
    if (bp->endp >= bp->endbuf) {
        bp->endp -= bp->buflen;
    }
    /*
        Flush the queue if the endp pointer is corrupted via a bad size
     */
    if (bp->endp >= bp->endbuf) {
        error("Bad end pointer");
        bufFlush(bp);
    }
}


/*
    Adjust the servp pointer after the user has copied data from the queue.
 */
PUBLIC void bufAdjustStart(WebsBuf *bp, ssize size)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(0 <= size && size < bp->buflen);

    bp->servp += size;
    if (bp->servp >= bp->endbuf) {
        bp->servp -= bp->buflen;
    }
    /*
        Flush the queue if the servp pointer is corrupted via a bad size
     */
    if (bp->servp >= bp->endbuf) {
        error("Bad serv pointer");
        bufFlush(bp);
    }
}


/*
    Flush all data in a buffer. Reset the pointers.
 */
PUBLIC void bufFlush(WebsBuf *bp)
{
    assert(bp);
    assert(bp->servp);

    if (bp->buf) {
        bp->servp = bp->buf;
        bp->endp = bp->buf;
        if (bp->servp) {
            *bp->servp = '\0';
        }
    }
}


PUBLIC void bufCompact(WebsBuf *bp)
{
    ssize len;

    if (bp->buf) {
        if ((len = bufLen(bp)) > 0) {
            if (bp->servp < bp->endp && bp->servp > bp->buf) {
                bufAddNull(bp);
                memmove(bp->buf, bp->servp, len + 1);
                bp->endp -= bp->servp - bp->buf;
                bp->servp = bp->buf;
            }
        } else {
            bp->servp = bp->endp = bp->buf;
            *bp->servp = '\0';
        }
    }
}


/*
    Reset pointers if empty
 */
PUBLIC void bufReset(WebsBuf *bp)
{
    if (bp->buf && bp->servp == bp->endp && bp->servp > bp->buf) {
        bp->servp = bp->endp = bp->buf;
        *bp->servp = '\0';
    }
}


/*
    Grow the buffer. Return true if the buffer can be grown. Grow using the increment size specified when opening the
    buf. Don't grow beyond the maximum possible size.
 */
PUBLIC bool bufGrow(WebsBuf *bp, ssize room)
{
    char  *newbuf;
    ssize len;

    assert(bp);

    if (room == 0) {
        if (bp->maxsize >= 0 && bp->buflen >= bp->maxsize) {
#if BLD_DEBUG
            error("Cannot grow buf. Needed %d, max %d", room, bp->maxsize);
#endif
            return 0;
        }
        room = bp->increment;
        /*
            Double growth increments only while below the limit.
         */
        if (bp->maxsize < 0 || bp->increment < bp->maxsize) {
            bp->increment = getBinBlockSize(2 * bp->increment);
        }
    }
    len = bufLen(bp);
    if ((newbuf = walloc(bp->buflen + room)) == NULL) {
        return 0;
    }
    bufGetBlk(bp, newbuf, bufLen(bp));
    wfree((char*) bp->buf);

    bp->buflen += room;
    bp->buf = newbuf;
    bp->endbuf = &bp->buf[bp->buflen];
    bp->servp = newbuf;
    bp->endp = &newbuf[len];
    bufAddNull(bp);
    return 1;
}


/*
    Find the smallest binary memory size that "size" will fit into.  This makes the buf and bufGrow routines much
    more efficient. The walloc routine likes powers of 2 minus 1.
 */
static ssize getBinBlockSize(ssize size)
{
    int q;

    size = size >> WEBS_SHIFT;
    for (q = 0; size; size >>= 1) {
        q++;
    }
    return ((ssize) 1) << (WEBS_SHIFT + q);
}


WebsHash hashCreate(int size)
{
    WebsHash  sd;
    HashTable *tp;

    if (size < 0) {
        size = WEBS_SMALL_HASH;
    }
    assert(size > 2);

    /*
        Create a new handle for this symbol table
     */
    if ((sd = wallocHandle(&sym)) < 0) {
        return -1;
    }

    /*
        Create a new symbol table structure and zero
     */
    if ((tp = (HashTable*) walloc(sizeof(HashTable))) == NULL) {
        symMax = wfreeHandle(&sym, sd);
        return -1;
    }
    memset(tp, 0, sizeof(HashTable));
    if (sd >= symMax) {
        symMax = sd + 1;
    }
    assert(0 <= sd && sd < symMax);
    sym[sd] = tp;

    /*
        Now create the hash table for fast indexing.
     */
    tp->size = calcPrime(size);
    if ((tp->hash_table = (WebsKey**) walloc(tp->size * sizeof(WebsKey*))) == 0) {
        wfreeHandle(&sym, sd);
        wfree(tp);
        return -1;
    }
    assert(tp->hash_table);
    memset(tp->hash_table, 0, tp->size * sizeof(WebsKey*));
    return sd;
}


/*
    Close this symbol table. Call a cleanup function to allow the caller to free resources associated with each symbol
    table entry.
 */
PUBLIC void hashFree(WebsHash sd)
{
    HashTable *tp;
    WebsKey   *sp, *forw;
    int       i;

    if (sd < 0) {
        return;
    }
    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Free all symbols in the hash table, then the hash table itself.
     */
    for (i = 0; i < tp->size; i++) {
        for (sp = tp->hash_table[i]; sp; sp = forw) {
            forw = sp->forw;
            valueFree(&sp->name);
            valueFree(&sp->content);
            wfree((void*) sp);
            sp = forw;
        }
    }
    wfree((void*) tp->hash_table);
    symMax = wfreeHandle(&sym, sd);
    wfree((void*) tp);
}


/*
    Return the first symbol in the hashtable if there is one. This call is used as the first step in traversing the
    table. A call to hashFirst should be followed by calls to hashNext to get all the rest of the entries.
 */
WebsKey *hashFirst(WebsHash sd)
{
    HashTable *tp;
    WebsKey   *sp;
    int       i;

    assert(0 <= sd && sd < symMax);
    if (sd < 0 || sd >= symMax) {
        return 0;
    }
    tp = sym[sd];
    assert(tp);

    /*
        Find the first symbol in the hashtable and return a pointer to it.
     */
    for (i = 0; i < tp->size; i++) {
        if ((sp = tp->hash_table[i]) != 0) {
            return sp;
        }
    }
    return 0;
}


/*
    Return the next symbol in the hashtable if there is one. See hashFirst.
 */
WebsKey *hashNext(WebsHash sd, WebsKey *last)
{
    HashTable *tp;
    WebsKey   *sp;
    int       i;

    assert(0 <= sd && sd < symMax);
    if (sd < 0) {
        return 0;
    }
    tp = sym[sd];
    assert(tp);
    if (last == 0) {
        return hashFirst(sd);
    }
    if (last->forw) {
        return last->forw;
    }
    for (i = last->bucket + 1; i < tp->size; i++) {
        if ((sp = tp->hash_table[i]) != 0) {
            return sp;
        }
    }
    return NULL;
}


/*
    Lookup a symbol and return a pointer to the symbol entry. If not present then return a NULL.
 */
WebsKey *hashLookup(WebsHash sd, cchar *name)
{
    HashTable *tp;
    WebsKey   *sp;
    char      *cp;

    assert(0 <= sd && sd < symMax);
    if (sd < 0 || (tp = sym[sd]) == NULL) {
        return NULL;
    }
    if (name == NULL || *name == '\0') {
        return NULL;
    }
    /*
        Do an initial hash and then follow the link chain to find the right entry
     */
    for (sp = hash(tp, name); sp; sp = sp->forw) {
        cp = sp->name.value.string;
        if (cp[0] == name[0] && strcmp(cp, name) == 0) {
            break;
        }
    }
    return sp;
}


void *hashLookupSymbol(WebsHash sd, cchar *name)
{
    WebsKey *kp;

    if ((kp = hashLookup(sd, name)) == 0) {
        return 0;
    }
    return kp->content.value.symbol;
}


/*
    Enter a symbol into the table. If already there, update its value.  Always succeeds if memory available. We allocate
    a copy of "name" here so it can be a volatile variable. The value "v" is just a copy of the passed in value, so it
    MUST be persistent.
 */
WebsKey *hashEnter(WebsHash sd, cchar *name, WebsValue v, int arg)
{
    HashTable *tp;
    WebsKey   *sp, *last;
    char      *cp;
    int       hindex;

    assert(name);
    assert(0 <= sd && sd < symMax);
    /*
        Reject null or empty names before hashIndex dereferences them.
     */
    if (name == NULL || *name == '\0') {
        return NULL;
    }
    tp = sym[sd];
    assert(tp);

    /*
        Calculate the first daisy-chain from the hash table. If non-zero, then we have daisy-chain, so scan it and look
        for the symbol.
     */
    last = NULL;
    hindex = hashIndex(tp, name);
    if ((sp = tp->hash_table[hindex]) != NULL) {
        for (; sp; sp = sp->forw) {
            cp = sp->name.value.string;
            if (cp[0] == name[0] && strcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
        if (sp) {
            /*
                Found, so update the value If the caller stores handles which require freeing, they will be lost here.
                It is the callers responsibility to free resources before overwriting existing contents. We will here
                free allocated strings which occur due to value_instring().  We should consider providing the cleanup
                function on the open rather than the close and then we could call it here and solve the problem.
             */
            if (sp->content.valid) {
                valueFree(&sp->content);
            }
            sp->content = v;
            sp->arg = arg;
            return sp;
        }
        /*
            Not found so allocate and append to the daisy-chain
         */
        if ((sp = (WebsKey*) walloc(sizeof(WebsKey))) == 0) {
            return NULL;
        }
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->content = v;
        sp->forw = (WebsKey*) NULL;
        sp->arg = arg;
        sp->bucket = hindex;
        last->forw = sp;

    } else {
        /*
            Daisy chain is empty so we need to start the chain
         */
        if ((sp = (WebsKey*) walloc(sizeof(WebsKey))) == 0) {
            return NULL;
        }
        tp->hash_table[hindex] = sp;
        tp->hash_table[hashIndex(tp, name)] = sp;

        sp->forw = (WebsKey*) NULL;
        sp->content = v;
        sp->arg = arg;
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->bucket = hindex;
    }
    return sp;
}


/*
    Delete a symbol from a table
 */
PUBLIC int hashDelete(WebsHash sd, cchar *name)
{
    HashTable *tp;
    WebsKey   *sp, *last;
    char      *cp;
    int       hindex;

    assert(name && *name);
    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Calculate the first daisy-chain from the hash table. If non-zero, then we have daisy-chain, so scan it and look
        for the symbol.
     */
    last = NULL;
    hindex = hashIndex(tp, name);
    if ((sp = tp->hash_table[hindex]) != NULL) {
        for ( ; sp; sp = sp->forw) {
            cp = sp->name.value.string;
            if (cp[0] == name[0] && strcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
    }
    if (sp == (WebsKey*) NULL) {              /* Not Found */
        return -1;
    }
    /*
         Unlink and free the symbol. Last will be set if the element to be deleted is not first in the chain.
     */
    if (last) {
        last->forw = sp->forw;
    } else {
        tp->hash_table[hindex] = sp->forw;
    }
    valueFree(&sp->name);
    valueFree(&sp->content);
    wfree((void*) sp);
    return 0;
}


/*
    Hash a symbol and return a pointer to the hash daisy-chain list. All symbols reside on the chain (ie. none stored in
    the hash table itself)
 */
static WebsKey *hash(HashTable *tp, cchar *name)
{
    assert(tp);

    return tp->hash_table[hashIndex(tp, name)];
}


/*
    Compute the hash function and return an index into the hash table We use a basic additive function that is then made
    modulo the size of the table.
 */
static ME_NO_SANITIZE_WRAP int hashIndex(HashTable *tp, cchar *name)
{
    uint sum;
    int  i;

    assert(tp);
    /*
        Add in each character shifted up progressively by 7 bits. The shift amount is rounded so as to not shift too
        far. It thus cycles with each new cycle placing character shifted up by one bit.

        Promote via uchar: a signed char with the high bit set sign-extends to a negative int, and shifting a
        negative value left is undefined. The cast is a no-op below 0x80, so ASCII keys keep their bucket.
     */
    i = 0;
    sum = 0;
    while (*name) {
        sum += (((int) (uchar) *name++) << i);
        i = (i + 7) % (BITS(int) - BITSPERBYTE);
    }
    return sum % tp->size;
}


/*
    Check if this number is a prime
 */
static int isPrime(int n)
{
    int i, max;

    assert(n > 0);

    max = n / 2;
    for (i = 2; i <= max; i++) {
        if (n % i == 0) {
            return 0;
        }
    }
    return 1;
}


/*
    Calculate the largest prime smaller than size.
 */
static int calcPrime(int size)
{
    int count;

    assert(size > 0);

    for (count = size; count > 0; count--) {
        if (isPrime(count)) {
            return count;
        }
    }
    return 1;
}


/*
    Convert a hex string to an integer
 */
uint hextoi(cchar *hexstring)
{
    cchar *h;
    uint  c, v;

    if (!hexstring) {
        return 0;
    }
    v = 0;
    h = hexstring;
    if (*h == '0' && (*(h + 1) == 'x' || *(h + 1) == 'X')) {
        h += 2;
    }
    while ((c = (uint) *h++) != 0) {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'f') {
            c = (c - 'a') + 10;
        } else if (c >=  'A' && c <= 'F') {
            c = (c - 'A') + 10;
        } else {
            break;
        }
        if (v > (UINT_MAX - c) / 16) {
            /*
                Saturate on overflow so an over-large hex string can never wrap to a small value
                (or to zero). A wrapped chunk-size of 0 would otherwise be mistaken for the
                terminating chunk and enable request smuggling (CWE-190 feeding CWE-444).
             */
            return UINT_MAX;
        }
        v = (v * 0x10) + c;
    }
    return v;
}


PUBLIC char *sclone(cchar *s)
{
    char  *buf;
    ssize len;

    if (s == NULL) {
        s = "";
    }
    len = strlen(s) + 1;
    if ((buf = walloc(len)) != 0) {
        scopy(buf, len, s);
    }
    return buf;
}



/*
    Clone a sub-string of a specified length. The null is added after the length. The given len can be longer than the
    source string.
 */
PUBLIC char *snclone(cchar *str, ssize len)
{
    char  *ptr;
    ssize size, l;

    if (str == 0) {
        str = "";
    }
    l = slen(str);
    len = min(l, len);
    size = len + 1;
    if ((ptr = walloc(size)) != 0) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
    }
    return ptr;
}


PUBLIC bool snumber(cchar *s)
{
    if (!s) {
        return 0;
    }
    if (*s == '-' || *s == '+') {
        s++;
    }
    return s && *s && strspn(s, "1234567890") == strlen(s);
}


/*
    Convert a string to an integer. If the string starts with "0x" or "0X" a hexadecimal conversion is done.
 */
uint strtoi(char *s)
{
    if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')) {
        s += 2;
        return hextoi(s);
    }
    return atoi(s);
}


PUBLIC int scaselesscmp(cchar *s1, cchar *s2)
{
    if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncaselesscmp(s1, s2, max(slen(s1), slen(s2)));
}


PUBLIC bool scaselessmatch(cchar *s1, cchar *s2)
{
    return scaselesscmp(s1, s2) == 0;
}


PUBLIC bool smatch(cchar *s1, cchar *s2)
{
    return scmp(s1, s2) == 0;
}


PUBLIC bool sstarts(cchar *str, cchar *prefix)
{
    if (str == 0 || prefix == 0) {
        return 0;
    }
    if (strncmp(str, prefix, slen(prefix)) == 0) {
        return 1;
    }
    return 0;
}


PUBLIC int scmp(cchar *s1, cchar *s2)
{
    if (s1 == s2) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncmp(s1, s2, max(slen(s1), slen(s2)));
}


PUBLIC ssize slen(cchar *s)
{
    return s ? strlen(s) : 0;
}


PUBLIC ssize scopy(char *dest, ssize destMax, cchar *src)
{
    ssize len;

    assert(src);
    assert(dest);
    assert(destMax > 0 && destMax < MAXINT);

    len = slen(src);
    if (destMax <= len) {
        if (destMax > 0) {
            *dest = '\0';
        }
        return -1;
    }
    strcpy(dest, src);
    return len;
}


/*
    This routine copies at most "count" characters from a string. It ensures the result is always null terminated and
    the buffer does not overflow. Returns -1 if the buffer is too small.
 */
PUBLIC ssize sncopy(char *dest, ssize destMax, cchar *src, ssize count)
{
    ssize len;

    assert(dest);
    assert(src);
    assert(src != dest);
    assert(0 <= count && count < MAXINT);
    assert(0 < destMax && destMax < MAXINT);

    //  OPT need snlen(src, count);
    len = slen(src);
    len = min(len, count);
    if (destMax <= len) {
        if (destMax > 0) {
            *dest = '\0';
        }
        return -1;
    }
    if (len > 0) {
        memcpy(dest, src, len);
        dest[len] = '\0';
    } else {
        *dest = '\0';
        len = 0;
    }
    return len;
}


/*
    Case sensitive string comparison. Limited by length
 */
PUBLIC int sncmp(cchar *s1, cchar *s2, ssize n)
{
    int rc;

    assert(0 <= n && n < MAXINT);

    if (s1 == 0 && s2 == 0) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = *s1 - *s2;
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}


PUBLIC int sncaselesscmp(cchar *s1, cchar *s2, ssize n)
{
    int rc;

    assert(0 <= n && n < MAXINT);

    if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = tolower((uchar) *s1) - tolower((uchar) *s2);
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}


/*
    Split a string at a delimiter and return the parts.
    This differs from stok in that it never returns null. Also, stok eats leading deliminators, whereas
    ssplit will return an empty string if there are leading deliminators.
    Note: Modifies the original string and returns the string for chaining.
 */
PUBLIC char *ssplit(char *str, cchar *delim, char **last)
{
    char *end;

    if (last) {
        *last = "";
    }
    if (str == 0) {
        return "";
    }
    if (delim == 0 || *delim == '\0') {
        return str;
    }
    if ((end = strpbrk(str, delim)) != 0) {
        *end++ = '\0';
        end += strspn(end, delim);
    } else {
        end = "";
    }
    if (last) {
        *last = end;
    }
    return str;
}


/*
    Note "str" is modified as per strtok()
    WARNING:  this does not allocate
 */
PUBLIC char *stok(char *str, cchar *delim, char **last)
{
    char  *start, *end;
    ssize i;

    start = str ? str : *last;
    if (start == 0) {
        *last = 0;
        return 0;
    }
    i = strspn(start, delim);
    start += i;
    if (*start == '\0') {
        *last = 0;
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        *end++ = '\0';
        i = strspn(end, delim);
        end += i;
    }
    *last = end;
    return start;
}


PUBLIC char *strim(char *str, cchar *set, int where)
{
    char  *s;
    ssize len, i;

    if (str == 0 || set == 0) {
        return 0;
    }
    if (where & WEBS_TRIM_START) {
        i = strspn(str, set);
    } else {
        i = 0;
    }
    s = (char*) &str[i];
    if (where & WEBS_TRIM_END) {
        len = strlen(s);
        while (len > 0 && strspn(&s[len - 1], set) > 0) {
            s[len - 1] = '\0';
            len--;
        }
    }
    return s;
}


/*
    Parse the args and return the count of args. If argv is NULL, the args are parsed read-only. If argv is set,
    then the args will be extracted, back-quotes removed and argv will be set to point to all the args.
    NOTE: this routine does not allocate.
 */
PUBLIC int websParseArgs(char *args, char **argv, int maxArgc)
{
    char *dest, *src, *start;
    int  quote, argc;

    /*
        Example     "showColors" red 'light blue' "yellow white" 'Cannot \"render\"'
        Becomes:    ["showColors", "red", "light blue", "yellow white", "Cannot \"render\""]
     */
    for (argc = 0, src = args; src && *src != '\0' && argc < maxArgc; argc++) {
        while (isspace((uchar) *src)) {
            src++;
        }
        if (*src == '\0') {
            break;
        }
        start = dest = src;
        if (*src == '"' || *src == '\'') {
            quote = *src;
            src++;
            dest++;
        } else {
            quote = 0;
        }
        if (argv) {
            argv[argc] = src;
        }
        while (*src) {
            if (*src == '\\' && src[1] && (src[1] == '\\' || src[1] == '"' || src[1] == '\'')) {
                src++;
            } else {
                if (quote) {
                    if (*src == quote && !(src > start && src[-1] == '\\')) {
                        break;
                    }
                } else if (*src == ' ') {
                    break;
                }
            }
            if (argv) {
                *dest++ = *src;
            }
            src++;
        }
        if (*src != '\0') {
            src++;
        }
        if (argv) {
            *dest++ = '\0';
        }
    }
    return argc;
}


#if ME_GOAHEAD_LEGACY
PUBLIC int fmtValloc(char **sp, int n, cchar *format, va_list args)
{
    *sp = sfmtv(format, args);
    return (int) slen(*sp);
}


PUBLIC int fmtAlloc(char **sp, int n, cchar *format, ...)
{
    va_list args;

    va_start(args, format);
    *sp = sfmtv(format, args);
    va_end(args);
    return (int) slen(*sp);
}
#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/socket.c ************/

/*
    socket.c -- Sockets layer

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/

#define SOCKET_DRAIN_LIMIT (ME_GOAHEAD_LIMIT_BUFFER * 64)  /* Unread input discarded per drain */
#define SOCKET_DRAIN_CHUNK 256                              /* Drain read size, small as it sits on the request stack */
#define SOCKET_IP_MAX      128                              /* Numeric IPv4 or IPv6 address with scope */

/************************************ Locals **********************************/

PUBLIC WebsSocket **socketList;                   /* List of open sockets */
PUBLIC int        socketMax;                      /* Maximum size of socket */
PUBLIC Socket     socketHighestFd = (Socket) -1;  /* Highest socket fd opened */
PUBLIC int        socketOpenCount = 0;            /* Number of task using sockets */

static int  hasIPv6;                              /* System supports IPv6 */
static int  lingerCount;                          /* Sockets in a lingering close */
static bool warnedNoSendTimeout;                  /* Warned once that send timeouts are unavailable */
#if ME_UNIX_LIKE
static int spareFd = -1;                          /* Held back to refuse a connection when no descriptor is free */
#endif

/***************************** Forward Declarations ***************************/

static int ipv6(cchar *ip);
static void lingerExpired(void *data, int id);
static void lingerRead(int sid, int mask, void *data);
static bool socketDrain(int sid);
static void socketAccept(WebsSocket *sp);
static void socketDoEvent(WebsSocket *sp);

/*********************************** Code *************************************/

PUBLIC int socketOpen(void)
{
    Socket fd;

    if (++socketOpenCount > 1) {
        return 0;
    }

#if ME_WIN_LIKE
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
            return -1;
        }
        if (wsaData.wVersion != MAKEWORD(1, 1)) {
            WSACleanup();
            return -1;
        }
    }
#endif
    socketList = NULL;
    socketMax = 0;
    socketHighestFd = (Socket) -1;
#if ME_UNIX_LIKE
    spareFd = open("/dev/null", O_RDONLY | O_CLOEXEC);
#endif
    if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) != -1) {
        hasIPv6 = 1;
        closesocket(fd);
    } else {
        trace(1, "This system does not have IPv6 support");
    }
    return 0;
}


PUBLIC void socketClose(void)
{
    int i;

    if (--socketOpenCount <= 0) {
        for (i = socketMax - 1; i >= 0; i--) {
            if (socketList && socketList[i]) {
                socketCloseConnection(i);
            }
        }
        socketOpenCount = 0;
#if ME_UNIX_LIKE
        if (spareFd >= 0) {
            close(spareFd);
            spareFd = -1;
        }
#endif
    }
}


PUBLIC bool socketHasDualNetworkStack(void)
{
    bool dual;

#if defined(ME_COMPILER_HAS_SINGLE_STACK) || VXWORKS
    dual = 0;
#else
    dual = hasIPv6;
#endif
    return dual;
}


PUBLIC bool socketHasIPv6(void)
{
    return hasIPv6;
}


PUBLIC int socketListen(cchar *ip, int port, SocketAccept accept, int flags)
{
    WebsSocket              *sp;
    struct sockaddr_storage addr;
    Socklen                 addrlen;
    cchar                   *sip;
    int                     family, protocol, sid, enable;

    if (port > SOCKET_PORT_MAX) {
        return -1;
    }
    if ((sid = socketAlloc(ip, port, accept, flags)) < 0) {
        return -1;
    }
    sp = socketList[sid];
    assert(sp);

    /*
        Change null IP address to be an IPv6 endpoint if the system is dual-stack. That way we can listen on
        both IPv4 and IPv6
     */
    sip = ((ip == 0 || *ip == '\0') && socketHasDualNetworkStack()) ? "::" : ip;

    /*
        Bind to the socket endpoint and the call listen() to start listening
     */
    if (socketInfo(sip, port, &family, &protocol, &addr, &addrlen) < 0) {
        socketFree(sid);
        return -1;
    }
    if ((sp->sock = socket(family, SOCK_STREAM, protocol)) == SOCKET_ERROR) {
        socketFree(sid);
        return -1;
    }
    socketHighestFd = max(socketHighestFd, sp->sock);

#if ME_COMPILER_HAS_FCNTL
    fcntl(sp->sock, F_SETFD, FD_CLOEXEC);
#endif
    enable = 1;
#if ME_UNIX_LIKE || VXWORKS
    if (setsockopt(sp->sock, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(enable)) != 0) {
        error("Cannot set reuseaddr, errno %d", errno);
    }
#elif ME_WIN_LIKE && defined(SO_EXCLUSIVEADDRUSE)
    if (setsockopt(sp->sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*) &enable, sizeof(enable)) != 0) {
        error("Cannot set exclusiveaddruse, errno %d", WSAGetLastError());
    }
#endif

#if defined(IPV6_V6ONLY)
    /*
        By default, most stacks listen on both IPv6 and IPv4 if ip == 0, except windows which inverts this.
        So we explicitly control.
     */
    if (hasIPv6) {
        if (ip == 0) {
            enable = 0;
            setsockopt(sp->sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*) &enable, sizeof(enable));
        } else if (ipv6(ip)) {
            enable = 1;
            setsockopt(sp->sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*) &enable, sizeof(enable));
        }
    }
#endif
    if (bind(sp->sock, (struct sockaddr*) &addr, addrlen) == SOCKET_ERROR) {
        error("Cannot bind to address %s:%d, errno %d", ip ? ip : "*", port, errno);
        socketFree(sid);
        return -1;
    }
    if (listen(sp->sock, SOMAXCONN) < 0) {
        socketFree(sid);
        return -1;
    }
    sp->flags |= SOCKET_LISTENING | SOCKET_NODELAY;
    sp->handlerMask |= SOCKET_READABLE;
    socketSetBlock(sid, (flags & SOCKET_BLOCK));
    if (sp->flags & SOCKET_NODELAY) {
        socketSetNoDelay(sid, 1);
    }
    return sid;
}


PUBLIC void socketCloseConnection(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    socketFree(sid);
}


#ifndef SHUT_WR
    #define SHUT_WR 1
#endif

PUBLIC void socketLinger(int sid, int timeout)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    if (timeout <= 0 || sp->sock == (Socket) -1 || (sp->flags & (SOCKET_EOF | SOCKET_LISTENING)) ||
        lingerCount >= ME_GOAHEAD_LIMIT_LINGER_CONNECTIONS || shutdown(sp->sock, SHUT_WR) < 0) {
        socketFree(sid);
        return;
    }
    socketSetBlock(sid, 0);
    sp->flags |= SOCKET_LINGERING;
    lingerCount++;
    socketCreateHandler(sid, SOCKET_READABLE, lingerRead, NULL);
    sp->lingerEvent = websStartEvent(timeout * 1000, lingerExpired, (void*) (ssize) sid);
}


/* Drain at most SOCKET_DRAIN_LIMIT bytes to bound event-loop work. Return false on EOF or hard error. */
static bool socketDrain(int sid)
{
    WebsSocket *sp;
    char       buf[SOCKET_DRAIN_CHUNK];
    ssize      nbytes, total;
    int        error;

    sp = socketList[sid];
    for (total = 0; (nbytes = recv(sp->sock, buf, sizeof(buf), 0)) > 0; ) {
        total += nbytes;
        if (total >= SOCKET_DRAIN_LIMIT) {
            return 1;
        }
    }
    if (nbytes < 0) {
        error = socketGetError(sid);
        if (error == EAGAIN || error == EWOULDBLOCK || error == EINTR) {
            return 1;
        }
    }
    return 0;
}



static void lingerRead(int sid, int mask, void *data)
{
    if (socketPtr(sid) == NULL || socketDrain(sid)) {
        return;
    }
    socketFree(sid);
}


static void lingerExpired(void *data, int id)
{
    WebsSocket *sp;
    int        sid;

    websStopEvent(id);
    sid = (int) (ssize) data;
    if (sid < socketMax && (sp = socketList[sid]) != NULL && (sp->flags & SOCKET_LINGERING) && sp->lingerEvent == id) {
        sp->lingerEvent = -1;
        socketFree(sid);
    }
}


/*
    Accept a connection. Called as a callback on incoming connection.
 */
static void socketAccept(WebsSocket *sp)
{
    struct sockaddr_storage addrStorage;
    struct sockaddr         *addr;
    WebsSocket              *nsp;
    Socket                  newSock;
    Socklen                 len;
    char                    ipbuf[SOCKET_IP_MAX];
    int                     port, nid;

    assert(sp);

    /*
        Accept the connection and prevent inheriting by children (F_SETFD)
     */
    memset(&addrStorage, 0, sizeof(addrStorage));
    len = sizeof(addrStorage);
    addr = (struct sockaddr*) &addrStorage;
    if ((newSock = accept(sp->sock, addr, &len)) == SOCKET_ERROR) {
#if ME_UNIX_LIKE
        /*
            Use the spare descriptor to accept and close a queued connection, avoiding a readable-listener spin on
            descriptor exhaustion.
         */
        if ((errno == EMFILE || errno == ENFILE) && spareFd >= 0) {
            close(spareFd);
            if ((newSock = accept(sp->sock, addr, &len)) != SOCKET_ERROR) {
                close(newSock);
                trace(2, "Refused a connection: no file descriptors are free");
            }
            spareFd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
#endif
        return;
    }
#if ME_COMPILER_HAS_FCNTL
    fcntl(newSock, F_SETFD, FD_CLOEXEC);
#endif
    socketHighestFd = max(socketHighestFd, newSock);

    /*
        Create a socket structure and insert into the socket list
     */
    /*
        newSock is not owned by a WebsSocket until the assignment below, so nothing else will close it.
        Returning without closing abandons the descriptor, and the listening socket stays readable, so
        at the allocation ceiling every wakeup accepts and leaks one more.
     */
    if ((nid = socketAlloc(sp->ip, sp->port, sp->accept, sp->flags)) < 0) {
        closesocket(newSock);
        return;
    }
    if ((nsp = socketList[nid]) == 0) {
        closesocket(newSock);
        socketFree(nid);
        return;
    }
    assert(nsp);
    nsp->sock = newSock;
    nsp->flags &= ~SOCKET_LISTENING;
    socketSetBlock(nid, (nsp->flags & SOCKET_BLOCK));
    if (nsp->flags & SOCKET_NODELAY) {
        socketSetNoDelay(nid, 1);
    }

    /*
        Call the user accept callback. The user must call socketCreateHandler to register for further events of
           interest.
     */
    if (sp->accept != NULL) {
        /* Get the remote client address */
        if (socketAddress(addr, (int) len, ipbuf, sizeof(ipbuf), &port) < 0) {
            socketFree(nid);
            return;
        }
        if ((sp->accept)(nid, ipbuf, port, sp->sid) < 0) {
            socketFree(nid);
        }
    }
}


PUBLIC void socketRegisterInterest(int sid, int handlerMask)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->handlerMask = handlerMask;
    if (sp->flags & SOCKET_BUFFERED_READ) {
        sp->handlerMask |= SOCKET_READABLE;
    }
    if (sp->flags & SOCKET_BUFFERED_WRITE) {
        sp->handlerMask |= SOCKET_WRITABLE;
    }
}


/*
    Wait until an event occurs on a socket. Return zero on success, -1 on failure.
 */
PUBLIC int socketWaitForEvent(WebsSocket *sp, int handlerMask)
{
    int mask;

    assert(sp);

    mask = sp->handlerMask;
    sp->handlerMask |= handlerMask;
    while (socketSelect(sp->sid, 1000)) {
        if (sp->currentEvents & (handlerMask | SOCKET_EXCEPTION)) {
            break;
        }
    }
    sp->handlerMask = mask;
    if (sp->currentEvents & SOCKET_EXCEPTION) {
        return -1;
    } else if (sp->currentEvents & handlerMask) {
        return 0;
    }
    return 0;
}


PUBLIC void socketHiddenData(WebsSocket *sp, ssize len, int dir)
{
    if (len > 0) {
        sp->flags |= (dir == SOCKET_READABLE) ? SOCKET_BUFFERED_READ : SOCKET_BUFFERED_WRITE;
        if (sp->handler) {
            socketReservice(sp->sid);
        }
    } else {
        sp->flags &= ~((dir == SOCKET_READABLE) ? SOCKET_BUFFERED_READ : SOCKET_BUFFERED_WRITE);
    }
}


/*
    Wait for a handle to become readable or writable and return a number of noticed events. Timeout is in milliseconds.
 */
#if ME_WIN_LIKE
PUBLIC int socketSelect(int sid, int timeout)
{
    struct timeval tv;
    WebsSocket     *sp;
    fd_set         readFds, writeFds, exceptFds;
    int            nEvents;
    int            all;

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);

    tv.tv_sec = (long) (timeout / 1000);
    tv.tv_usec = (DWORD) (timeout % 1000) * 1000;

    /*
        Set the select event masks for events to watch
     */
    all = nEvents = 0;

    if (sid < 0) {
        all++;
        sid = 0;
    }

    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            continue;
        }
        assert(sp);
        /*
            Set the appropriate bit in the ready masks for the sp->sock.
         */
        if (sp->handlerMask & SOCKET_READABLE) {
            FD_SET(sp->sock, &readFds);
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_WRITABLE) {
            FD_SET(sp->sock, &writeFds);
            nEvents++;
        }
        if (sp->handlerMask & SOCKET_EXCEPTION) {
            FD_SET(sp->sock, &exceptFds);
            nEvents++;
        }
        if (sp->flags & SOCKET_RESERVICE) {
            tv.tv_sec = 0;
            tv.tv_usec = 0;
        }
        if (!all) {
            break;
        }
    }
    /*
        Windows select() fails if no descriptors are set, instead of just sleeping like other, nice select() calls.
        So, if WINDOWS, sleep.
     */
    if (nEvents == 0) {
        Sleep((DWORD) timeout);
        return 0;
    }
    /*
        Wait for the event or a timeout. Winsock ignores the descriptor count
     */
    nEvents = select(0, &readFds, &writeFds, &exceptFds, &tv);
    if (nEvents < 0) {
        //  A failed select leaves the sets undefined, so report only sockets due a reservice
        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);
        FD_ZERO(&exceptFds);
        nEvents = 0;
    }

    if (all) {
        sid = 0;
    }
    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            continue;
        }
        if (sp->flags & SOCKET_RESERVICE) {
            if (sp->handlerMask & SOCKET_READABLE) {
                sp->currentEvents |= SOCKET_READABLE;
            }
            if (sp->handlerMask & SOCKET_WRITABLE) {
                sp->currentEvents |= SOCKET_WRITABLE;
            }
            sp->flags &= ~SOCKET_RESERVICE;
            nEvents++;
        }
        if (FD_ISSET(sp->sock, &readFds)) {
            sp->currentEvents |= SOCKET_READABLE;
        }
        if (FD_ISSET(sp->sock, &writeFds)) {
            sp->currentEvents |= SOCKET_WRITABLE;
        }
        if (FD_ISSET(sp->sock, &exceptFds)) {
            sp->currentEvents |= SOCKET_EXCEPTION;
        }
        if (!all) {
            break;
        }
    }
    return nEvents;
}

#else /* !ME_WIN_LIKE */


PUBLIC int socketSelect(int sid, int timeout)
{
    WebsSocket     *sp;
    struct timeval tv;
    fd_mask        *readFds, *writeFds, *exceptFds, bit;
    ssize          index;
    int            all, len, nwords, nEvents;

    /*
        Allocate and zero the select masks
     */
    nwords = (socketHighestFd + NFDBITS) / NFDBITS;
    len = nwords * sizeof(fd_mask);

    readFds = walloc(len);
    memset(readFds, 0, len);
    writeFds = walloc(len);
    memset(writeFds, 0, len);
    exceptFds = walloc(len);
    memset(exceptFds, 0, len);

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    /*
        Set the select event masks for events to watch
     */
    all = nEvents = 0;

    if (sid < 0) {
        all++;
        sid = 0;
    }

    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            if (all == 0) {
                break;
            } else {
                continue;
            }
        }
        assert(sp);
        /*
            Initialize the ready masks and compute the mask offsets.
         */
        index = sp->sock / (NBBY * sizeof(fd_mask));
        bit = (fd_mask) (1UL << (sp->sock % (NBBY * sizeof(fd_mask))));
        /*
            Set the appropriate bit in the ready masks for the sp->sock.
         */
        if (sp->handlerMask & SOCKET_READABLE) {
            readFds[index] |= bit;
        }
        if (sp->handlerMask & SOCKET_WRITABLE) {
            writeFds[index] |= bit;
        }
        if (sp->handlerMask & SOCKET_EXCEPTION) {
            exceptFds[index] |= bit;
        }
        if (sp->flags & SOCKET_RESERVICE) {
            tv.tv_sec = 0;
            tv.tv_usec = 0;
        }
        if (!all) {
            break;
        }
    }
    /*
        Wait for the event or a timeout
     */
    nEvents = select(socketHighestFd + 1, (fd_set*) readFds, (fd_set*) writeFds, (fd_set*) exceptFds, &tv);
    if (nEvents < 0) {
        //  A failed select, including EINTR, leaves the requested interests in the masks
        memset(readFds, 0, len);
        memset(writeFds, 0, len);
        memset(exceptFds, 0, len);
        nEvents = 0;
    }
    if (all) {
        sid = 0;
    }
    for (; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) == NULL) {
            if (all == 0) {
                break;
            } else {
                continue;
            }
        }
        index = sp->sock / (NBBY * sizeof(fd_mask));
        bit = (fd_mask) (1UL << (sp->sock % (NBBY * sizeof(fd_mask))));

        if (sp->flags & SOCKET_RESERVICE) {
            if (sp->handlerMask & SOCKET_READABLE) {
                sp->currentEvents |= SOCKET_READABLE;
            }
            if (sp->handlerMask & SOCKET_WRITABLE) {
                sp->currentEvents |= SOCKET_WRITABLE;
            }
            sp->flags &= ~SOCKET_RESERVICE;
            nEvents++;
        }
        if (readFds[index] & bit) {
            sp->currentEvents |= SOCKET_READABLE;
        }
        if (writeFds[index] & bit) {
            sp->currentEvents |= SOCKET_WRITABLE;
        }
        if (exceptFds[index] & bit) {
            sp->currentEvents |= SOCKET_EXCEPTION;
        }
        if (!all) {
            break;
        }
    }
    wfree(readFds);
    wfree(writeFds);
    wfree(exceptFds);
    return nEvents;
}
#endif /* !(WINDOWS || CE) */


PUBLIC void socketProcess(void)
{
    WebsSocket *sp;
    int        sid;

    for (sid = 0; sid < socketMax; sid++) {
        if ((sp = socketList[sid]) != NULL) {
            if (sp->currentEvents & sp->handlerMask) {
                socketDoEvent(sp);
            }
        }
    }
}


static void socketDoEvent(WebsSocket *sp)
{
    int sid;

    assert(sp);

    sid = sp->sid;
    if (sp->currentEvents & SOCKET_READABLE) {
        if (sp->flags & SOCKET_LISTENING) {
            socketAccept(sp);
            sp->currentEvents = 0;
            return;
        }
    }
    /*
        Now invoke the users socket handler. NOTE: the handler may delete the
        socket, so we must be very careful after calling the handler.
     */
    if (sp->handler && (sp->handlerMask & sp->currentEvents)) {
        (sp->handler)(sid, sp->handlerMask & sp->currentEvents, sp->handler_data);
        /*
            Make sure socket pointer is still valid, then reset the currentEvents.
         */
        if (socketList && sid < socketMax && socketList[sid] == sp) {
            sp->currentEvents = 0;
        }
    }
}


/*
    Set the socket blocking mode. Return the previous mode.
 */
PUBLIC int socketSetBlock(int sid, int on)
{
    WebsSocket *sp;
    int        oldBlock;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    oldBlock = (sp->flags & SOCKET_BLOCK);
    sp->flags &= ~(SOCKET_BLOCK);
    if (on) {
        sp->flags |= SOCKET_BLOCK;
    }
    /*
        Put the socket into block / non-blocking mode
     */
    if (sp->flags & SOCKET_BLOCK) {
#if ME_WIN_LIKE
        ulong flag = !on;
        ioctlsocket(sp->sock, FIONBIO, &flag);
#elif ECOS
        int off;
        off = 0;
        ioctl(sp->sock, FIONBIO, &off);
#elif VXWORKS
        int iflag = !on;
        ioctl(sp->sock, FIONBIO, &iflag);
#elif TIDSP
        setsockopt((SOCKET) sp->sock, SOL_SOCKET, SO_BLOCKING, &on, sizeof(on));
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) & ~O_NONBLOCK);
#endif

    } else {
#if ME_WIN_LIKE
        ulong flag = !on;
        int   rc = ioctlsocket(sp->sock, FIONBIO, &flag);
        rc = rc;
#elif ECOS
        int on = 1;
        ioctl(sp->sock, FIONBIO, &on);
#elif VXWORKS
        int iflag = !on;
        ioctl(sp->sock, FIONBIO, &iflag);
#elif TIDSP
        setsockopt((SOCKET) sp->sock, SOL_SOCKET, SO_BLOCKING, &on, sizeof(on));
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) | O_NONBLOCK);
#endif
    }
#if MACOSX
    /* Prevent SIGPIPE when writing to closed socket on OS X */
    int iflag = 1;
    setsockopt(sp->sock, SOL_SOCKET, SO_NOSIGPIPE, (void*) &iflag, sizeof(iflag));
#endif
    return oldBlock;
}


/*
    Set the socket send timeout in milliseconds. Zero means wait indefinitely.
    Returns zero if the platform supports a send timeout, otherwise -1.
 */
PUBLIC int socketSetSendTimeout(int sid, int timeout)
{
    WebsSocket *sp;

#if ME_WIN_LIKE
    DWORD tv;
#else
    struct timeval tv;
#endif

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return -1;
    }
#if ME_WIN_LIKE
    tv = (DWORD) timeout;
#else
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
#endif
    /*
        Detect support at runtime. Stacks without SO_SNDTIMEO fail with ENOPROTOOPT, so no platform
        conditionals are required and callers can fall back to an unbounded wait.
     */
    if (setsockopt(sp->sock, SOL_SOCKET, SO_SNDTIMEO, (char*) &tv, sizeof(tv)) < 0) {
        if (!warnedNoSendTimeout) {
            warnedNoSendTimeout = 1;
            error("Warning: this network stack has no send timeout (SO_SNDTIMEO). A client that stops reading "
                  "a response can stall the server. Put it behind a proxy that buffers responses.");
        }
        return -1;
    }
    return 0;
}


/*
    Set the TCP delay behavior (nagle algorithm)
 */
PUBLIC int socketSetNoDelay(int sid, bool on)
{
    WebsSocket *sp;
    int        oldDelay;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    oldDelay = sp->flags & SOCKET_NODELAY;
    if (on) {
        sp->flags |= SOCKET_NODELAY;
    } else {
        sp->flags &= ~(SOCKET_NODELAY);
    }
#if ME_WIN_LIKE
    {
        BOOL noDelay;
        noDelay = on ? 1 : 0;
        setsockopt(sp->sock, IPPROTO_TCP, TCP_NODELAY, (FAR char*) &noDelay, sizeof(BOOL));
    }
#else
    {
        int noDelay;
        noDelay = on ? 1 : 0;
        setsockopt(sp->sock, IPPROTO_TCP, TCP_NODELAY, (char*) &noDelay, sizeof(int));
    }
#endif /* ME_WIN_LIKE */
    return oldDelay;
}


/*
    Write to a socket. Absorb as much data as the socket can buffer. Block if the socket is in blocking mode. Returns -1
    on error, otherwise the number of bytes written.
 */
PUBLIC ssize socketWrite(int sid, void *buf, ssize bufsize)
{
    WebsSocket *sp;
    ssize      len, written, sofar;
    int        errCode;

    if (buf == 0 || (sp = socketPtr(sid)) == NULL) {
        socketSetError(EBADF);
        return -1;
    }
    if (sp->flags & SOCKET_EOF) {
        socketSetError(EBADF);
        return -1;
    }
    len = bufsize;
    sofar = 0;
    while (len > 0) {
        if ((written = send(sp->sock, (char*) buf + sofar, (int) len, 0)) < 0) {
            errCode = socketGetError(sid);
            if (errCode == EINTR) {
                continue;
            } else if (errCode == EWOULDBLOCK || errCode == EAGAIN) {
                if (sofar) {
                    /*
                        If some data was written, we mask the EAGAIN for this time. Caller should recall and then
                        will get a negative return code with EAGAIN.
                     */
                    return sofar;
                }
            }
            return -errCode;
        }
        len -= written;
        sofar += written;
    }
    return sofar;
}


/*
    Read from a socket. Return the number of bytes read if successful. This may be less than the requested "bufsize" and
    may be zero. This routine may block if the socket is in blocking mode.
    Return -1 for errors or EOF. Distinguish between error and EOF via socketEof().
 */
PUBLIC ssize socketRead(int sid, void *buf, ssize bufsize)
{
    WebsSocket *sp;
    ssize      nbytes;
    int        errCode;

    assert(buf);
    assert(bufsize > 0);

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    if (sp->flags & SOCKET_EOF) {
        return -1;
    }
    if ((nbytes = recv(sp->sock, buf, (int) bufsize, 0)) < 0) {
        errCode = socketGetError(sid);
        if (errCode == EINTR || errCode == EAGAIN || errCode == EWOULDBLOCK) {
            nbytes = 0;
        } else {
            /* Conn reset or Some other error */
            sp->flags |= SOCKET_EOF;
            nbytes = -errCode;
        }

    } else if (nbytes == 0) {
        sp->flags |= SOCKET_EOF;
        nbytes = -1;
    }
    return nbytes;
}


/*
    Return true if EOF
 */
PUBLIC bool socketEof(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return 1;
    }
    return sp->flags & SOCKET_EOF;
}


PUBLIC void socketReservice(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->flags |= SOCKET_RESERVICE;
}


/*
    Create a user handler for this socket. The handler called whenever there
    is an event of interest as defined by handlerMask (SOCKET_READABLE, ...)
 */
PUBLIC void socketCreateHandler(int sid, int handlerMask, SocketHandler handler, void *data)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->handler = handler;
    sp->handler_data = data;
    socketRegisterInterest(sid, handlerMask);
}


PUBLIC void socketDeleteHandler(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->handler = NULL;
    socketRegisterInterest(sid, 0);
}


/*
    Allocate a new socket structure
 */
PUBLIC int socketAlloc(cchar *ip, int port, SocketAccept accept, int flags)
{
    WebsSocket *sp;
    int        sid;

    if (socketMax >= FD_SETSIZE) {
        return -1;
    }
    if ((sid = wallocObject(&socketList, &socketMax, sizeof(WebsSocket))) < 0) {
        return -1;
    }
    sp = socketList[sid];
    sp->sid = sid;
    //  No descriptor yet: zero is standard input, which freeing the socket would close
    sp->sock = (Socket) -1;
    sp->accept = accept;
    sp->port = port;
    sp->fileHandle = -1;
    sp->saveMask = -1;
    if (ip) {
        sp->ip = sclone(ip);
    }
    sp->flags = flags & (SOCKET_BLOCK | SOCKET_LISTENING | SOCKET_NODELAY);
    return sid;
}


/*
    Free a socket structure
 */
PUBLIC void socketFree(int sid)
{
    WebsSocket *sp;
    int        i;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    /*
        To close a socket, remove any registered interests, set it to non-blocking so that the recv which follows won't
        block, do a shutdown on it so peers on the other end will receive a FIN, then read any data not yet retrieved
        from the receive buffer, and finally close it.  If these steps are not all performed RESETs may be sent to the
        other end causing problems.
     */
    if (sp->flags & SOCKET_LINGERING) {
        websStopEvent(sp->lingerEvent);
        lingerCount--;
    }
    socketRegisterInterest(sid, 0);
    if (sp->sock >= 0) {
        socketSetBlock(sid, 0);
        socketDrain(sid);
        if (shutdown(sp->sock, SHUT_RDWR) >= 0) {
            socketDrain(sid);
        }
        closesocket(sp->sock);
    }
    wfreeset(sp->ip);
    wfree(sp);
    socketMax = wfreeHandle(&socketList, sid);
    /*
        Calculate the new highest socket number
     */
    socketHighestFd = (Socket) -1;
    for (i = 0; i < socketMax; i++) {
        if ((sp = socketList[i]) == NULL) {
            continue;
        }
        socketHighestFd = max(socketHighestFd, sp->sock);
    }
}


/*
    Return the socket object reference
 */
WebsSocket *socketPtr(int sid)
{
    if (sid < 0 || sid >= socketMax || socketList[sid] == NULL) {
        assert(NULL);
        errno = EBADF;
        return NULL;
    }
    assert(socketList[sid]);
    return socketList[sid];
}


/*
    Get the operating system error code
 */
PUBLIC int socketGetError(int sid)
{
#if ME_WIN_LIKE
    switch (WSAGetLastError()) {
    case WSAEWOULDBLOCK:
        return EWOULDBLOCK;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAEPROCLIM:
        return EAGAIN;
    case WSAETIMEDOUT:
        return ETIMEDOUT;
    case WSAEINTR:
        return EINTR;
    default:
        return EINVAL;
    }
#else
    return errno;
#endif
}


PUBLIC void socketSetError(int error)
{
#if ME_WIN_LIKE
    SetLastError(error);
#elif ME_UNIX_LIKE || VXWORKS
    errno = error;
#endif
}


/*
    Return the underlying socket handle
 */
PUBLIC Socket socketGetHandle(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return (Socket) -1;
    }
    return sp->sock;
}


/*
    Get blocking mode
 */
PUBLIC int socketGetBlock(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    return sp->flags & SOCKET_BLOCK;
}


PUBLIC int socketGetMode(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    return sp->flags;
}


PUBLIC void socketSetMode(int sid, int mode)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return;
    }
    sp->flags = mode;
}


PUBLIC int socketGetPort(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    return sp->port;
}


#if ME_UNIX_LIKE || WINDOWS
/*
    Get a socket address from a host/port combination. If a host provides both IPv4 and IPv6 addresses,
    prefer the IPv4 address. This routine uses getaddrinfo.
    Caller must free addr.
 */
PUBLIC int socketInfo(cchar *ip, int port, int *family, int *protocol, struct sockaddr_storage *addr, Socklen *addrlen)
{
    struct addrinfo hints, *res, *r;
    char            portBuf[16];
    int             v6;

    assert(addr);
    memset((char*) &hints, '\0', sizeof(hints));

    /*
        Note that IPv6 does not support broadcast, there is no 255.255.255.255 equivalent.
        Multicast can be used over a specific link, but the user must provide that address plus %scope_id.
     */
    if (ip == 0 || ip[0] == '\0') {
        ip = 0;
        hints.ai_flags |= AI_PASSIVE;           /* Bind to 0.0.0.0 and :: */
    }
    v6 = ipv6(ip);
    hints.ai_socktype = SOCK_STREAM;
    if (ip) {
        hints.ai_family = v6 ? AF_INET6 : AF_INET;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    itosbuf(portBuf, sizeof(portBuf), port, 10);

    /*
        Try to sleuth the address to avoid duplicate address lookups. Then try IPv4 first then IPv6.
     */
    res = 0;
    if (getaddrinfo(ip, portBuf, &hints, &res) != 0 || res == NULL) {
        return -1;
    }
    /*
        Prefer IPv4 if IPv6 not requested
     */
    for (r = res; r; r = r->ai_next) {
        if (v6) {
            if (r->ai_family == AF_INET6) {
                break;
            }
        } else {
            if (r->ai_family == AF_INET) {
                break;
            }
        }
    }
    if (r == NULL) {
        r = res;
    }
    memset(addr, 0, sizeof(*addr));
    memcpy((char*) addr, (char*) r->ai_addr, (int) r->ai_addrlen);

    *addrlen = (int) r->ai_addrlen;
    *family = r->ai_family;
    *protocol = r->ai_protocol;
    freeaddrinfo(res);
    return 0;
}
#else

PUBLIC int socketInfo(char *ip, int port, int *family, int *protocol, struct sockaddr_storage *addr, Socklen *addrlen)
{
    struct sockaddr_in sa;

    memset((char*) &sa, '\0', sizeof(struct sockaddr_in));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((short) (port & 0xFFFF));

    if (ip && *ip) {
        sa.sin_addr.s_addr = inet_addr((char*) ip);
    } else {
        sa.sin_addr.s_addr = INADDR_ANY;
    }
    if (sa.sin_addr.s_addr == INADDR_NONE) {
#if VXWORKS
        /*
            VxWorks supports IPv4 here; reject unresolved names instead of treating ERROR as an address.
         */
        int hostAddr;

        if ((hostAddr = hostGetByName((char*) ip)) == ERROR) {
            return -1;
        }
        sa.sin_addr.s_addr = (ulong) hostAddr;
#else
        struct hostent *hostent;
        /*
            Only IPv4 fits sockaddr_in.
         */
        hostent = gethostbyname2(ip, AF_INET);
        if (hostent == 0 || hostent->h_addrtype != AF_INET || hostent->h_length != (int) sizeof(sa.sin_addr)) {
            return -1;
        }
        memcpy((char*) &sa.sin_addr, (char*) hostent->h_addr_list[0], sizeof(sa.sin_addr));
#endif
    }
    memcpy((char*) addr, (char*) &sa, sizeof(sa));
    *addrlen = sizeof(struct sockaddr_in);
    *family = sa.sin_family;
    *protocol = 0;
    return 0;
}
#endif


/*
    Return a numerical IP address and port for the given socket info
 */
PUBLIC int socketAddress(struct sockaddr *addr, int addrlen, char *ip, int ipLen, int *port)
{
#if (ME_UNIX_LIKE || ME_WIN_LIKE)
    char service[NI_MAXSERV];

#if ME_WIN_LIKE || defined(IN6_IS_ADDR_V4MAPPED)
    if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6*) addr;
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
            struct sockaddr_in addr4;
            memset(&addr4, 0, sizeof(addr4));
            addr4.sin_family = AF_INET;
            addr4.sin_port = addr6->sin6_port;
            memcpy(&addr4.sin_addr.s_addr, addr6->sin6_addr.s6_addr + 12, sizeof(addr4.sin_addr.s_addr));
            memcpy(addr, &addr4, sizeof(addr4));
            addrlen = sizeof(addr4);
        }
    }
#endif
    if (getnameinfo(addr, addrlen, ip, ipLen, service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV | NI_NOFQDN)) {
        if (ip && ipLen > 0) {
            *ip = '\0';
        }
        if (port) {
            *port = 0;
        }
        return -1;
    }
    if (port) {
        *port = atoi(service);
    }

#else
    struct sockaddr_in *sa;

#if HAVE_NTOA_R
    sa = (struct sockaddr_in*) addr;
    inet_ntoa_r(sa->sin_addr, ip, ipLen);
#else
    uchar *cp;
    sa = (struct sockaddr_in*) addr;
    cp = (uchar*) &sa->sin_addr;
    fmt(ip, ipLen, "%d.%d.%d.%d", cp[0], cp[1], cp[2], cp[3]);
#endif
    if (port) {
        *port = ntohs(sa->sin_port);
    }
#endif
    return 0;
}


/*
    Looks like an IPv6 address if it has 2 or more colons
 */
static int ipv6(cchar *ip)
{
    char *cp;
    int  colons;

    if (ip == 0 || *ip == 0) {
        /*
            Listening on just a bare port means IPv4 only.
         */
        return 0;
    }
    if (*ip == '[') {
        //  A bracketed literal is IPv6 however few colons it holds, including the empty "[]"
        return 1;
    }
    colons = 0;
    for (cp = (char*) ip; ((*cp != '\0') && (colons < 2)) ; cp++) {
        if (*cp == ':') {
            colons++;
        }
    }
    return colons >= 2;
}


/*
    Parse address and return the IP address and port components. Handles ipv4 and ipv6 addresses.
    If the IP portion is absent, *pip is set to null. If the port portion is absent, port is set to the defaultPort.
    If a ":*" port specifier is used, *pport is set to -1;
    When an address contains an ipv6 port it should be written as:

        aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii
    or
        [aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii]:port

    If supplied an IPv6 address, the backets are stripped in the returned IP address.

    This routine parses any "https://" prefix.
    Caller must free *pip
 */
PUBLIC int socketParseAddress(cchar *address, char **pip, int *pport, int *secure, int defaultPort)
{
    char *ip, *cp;

    ip = 0;
    if (defaultPort < 0) {
        defaultPort = 80;
    }
    *secure = strncmp(address, "https", 5) == 0;
    if ((cp = strstr(address, "://")) != 0) {
        address = &cp[3];
    }
    if (ipv6(address)) {
        /*
            IPv6. If port is present, it will follow a closing bracket ']'
         */
        if ((cp = strchr(address, ']')) != 0) {
            cp++;
            if ((*cp) && (*cp == ':')) {
                *pport = (*++cp == '*') ? -1 : atoi(cp);

                /* Set ipAddr to ipv6 address without brackets */
                ip = sclone(address + 1);
                if ((cp = strchr(ip, ']')) != 0) {
                    *cp = '\0';
                }

            } else {
                /* Handles [a:b:c:d:e:f:g:h:i] case (no port)- should not occur */
                ip = sclone(address + 1);
                if ((cp = strchr(ip, ']')) != 0) {
                    *cp = '\0';
                }
                /* No port present, use callers default */
                *pport = defaultPort;
            }
        } else {
            /* Handles a:b:c:d:e:f:g:h:i case (no port) */
            ip = sclone(address);

            /* No port present, use callers default */
            *pport = defaultPort;
        }

    } else {
        /*
            ipv4
         */
        ip = sclone(address);
        if ((cp = strchr(ip, ':')) != 0) {
            *cp++ = '\0';
            if (*cp == '*') {
                *pport = -1;
            } else {
                *pport = atoi(cp);
            }
            if (*ip == '*') {
                wfree(ip);
                ip = 0;
            }

        } else if (strchr(ip, '.')) {
            *pport = defaultPort;

        } else {
            if (isdigit((uchar) *ip)) {
                *pport = atoi(ip);
                wfree(ip);
                ip = 0;
            } else {
                /* No port present, use callers default */
                *pport = defaultPort;
            }
        }
    }
    /*
        Empty address forms select all interfaces.
     */
    if (ip && *ip == '\0') {
        wfree(ip);
        ip = 0;
    }
    if (pip) {
        *pip = ip;
    } else if (ip) {
        wfree(ip);
    }
    return 0;
}


PUBLIC bool socketIsV6(int sid)
{
    WebsSocket *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return 0;
    }
    return sp->ip && ipv6(sp->ip);
}


PUBLIC bool socketAddressIsV6(cchar *ip)
{
    return ip && ipv6(ip);
}


PUBLIC WebsSocket **socketGetList(void)
{
    return socketList;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/time.c ************/

/**
    time.c - Date and Time handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************** Defines ***********************************/

#define SEC_PER_MIN  (60)
#define SEC_PER_HOUR (60 * 60)
#define SEC_PER_DAY  (86400)
#define SEC_PER_YEAR (INT64(31556952))

/*
    Token types
 */
#define TOKEN_DAY    0x01000000
#define TOKEN_MONTH  0x02000000
#define TOKEN_ZONE   0x04000000
#define TOKEN_OFFSET 0x08000000

typedef struct TimeToken {
    char *name;
    int value;
    int type;
} TimeToken;

static WebsHash timeTokens = -1;

static TimeToken days[] = {
    { "sun",  0, TOKEN_DAY },
    { "mon",  1, TOKEN_DAY },
    { "tue",  2, TOKEN_DAY },
    { "wed",  3, TOKEN_DAY },
    { "thu",  4, TOKEN_DAY },
    { "fri",  5, TOKEN_DAY },
    { "sat",  6, TOKEN_DAY },
    { 0, 0, 0 },
};

static TimeToken fullDays[] = {
    { "sunday",     0, TOKEN_DAY },
    { "monday",     1, TOKEN_DAY },
    { "tuesday",    2, TOKEN_DAY },
    { "wednesday",  3, TOKEN_DAY },
    { "thursday",   4, TOKEN_DAY },
    { "friday",     5, TOKEN_DAY },
    { "saturday",   6, TOKEN_DAY },
    { 0, 0, 0 },
};

/*
    Make origin 1 to correspond to user date entries 10/28/2014
 */
static TimeToken months[] = {
    { "jan",  1, TOKEN_MONTH },
    { "feb",  2, TOKEN_MONTH },
    { "mar",  3, TOKEN_MONTH },
    { "apr",  4, TOKEN_MONTH },
    { "may",  5, TOKEN_MONTH },
    { "jun",  6, TOKEN_MONTH },
    { "jul",  7, TOKEN_MONTH },
    { "aug",  8, TOKEN_MONTH },
    { "sep",  9, TOKEN_MONTH },
    { "oct", 10, TOKEN_MONTH },
    { "nov", 11, TOKEN_MONTH },
    { "dec", 12, TOKEN_MONTH },
    { 0, 0, 0 },
};

static TimeToken fullMonths[] = {
    { "january",    1, TOKEN_MONTH },
    { "february",   2, TOKEN_MONTH },
    { "march",      3, TOKEN_MONTH },
    { "april",      4, TOKEN_MONTH },
    { "may",        5, TOKEN_MONTH },
    { "june",       6, TOKEN_MONTH },
    { "july",       7, TOKEN_MONTH },
    { "august",     8, TOKEN_MONTH },
    { "september",  9, TOKEN_MONTH },
    { "october",   10, TOKEN_MONTH },
    { "november",  11, TOKEN_MONTH },
    { "december",  12, TOKEN_MONTH },
    { 0, 0, 0 }
};

static TimeToken ampm[] = {
    { "am", 0, TOKEN_OFFSET },
    { "pm", (12 * 3600), TOKEN_OFFSET },
    { 0, 0, 0 },
};


static TimeToken zones[] = {
    { "ut",      0, TOKEN_ZONE },
    { "utc",     0, TOKEN_ZONE },
    { "gmt",     0, TOKEN_ZONE },
    { "edt",  -240, TOKEN_ZONE },
    { "est",  -300, TOKEN_ZONE },
    { "cdt",  -300, TOKEN_ZONE },
    { "cst",  -360, TOKEN_ZONE },
    { "mdt",  -360, TOKEN_ZONE },
    { "mst",  -420, TOKEN_ZONE },
    { "pdt",  -420, TOKEN_ZONE },
    { "pst",  -480, TOKEN_ZONE },
    { 0, 0, 0 },
};


static TimeToken offsets[] = {
    { "tomorrow",    86400, TOKEN_OFFSET },
    { "yesterday",  -86400, TOKEN_OFFSET },
    { "next week",  (86400 * 7), TOKEN_OFFSET },
    { "last week", -(86400 * 7), TOKEN_OFFSET },
    { 0, 0, 0 },
};

static int timeSep = ':';

static int normalMonthStart[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 0,
};
static int leapMonthStart[] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 0
};

static int leapYear(int year);
static void validateTime(struct tm *tm, struct tm *defaults);

/************************************ Code ************************************/

PUBLIC int websTimeOpen(void)
{
    TimeToken *tt;

    timeTokens = hashCreate(59);
    for (tt = days; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = fullDays; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = months; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = fullMonths; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = ampm; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = zones; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = offsets; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    return 0;
}


PUBLIC void websTimeClose(void)
{
    if (timeTokens >= 0) {
        hashFree(timeTokens);
        timeTokens = -1;
    }
}


static int leapYear(int year)
{
    if (year % 4) {
        return 0;
    } else if (year % 400 == 0) {
        return 1;
    } else if (year % 100 == 0) {
        return 0;
    }
    return 1;
}


static int64 daysSinceEpoch(int year)
{
    int64 count;

    count = ((int64) 365) * (year - 1970);
    count += ((year - 1) / 4) - (1970 / 4);
    count -= ((year - 1) / 100) - (1970 / 100);
    count += ((year - 1) / 400) - (1970 / 400);
    return count;
}


/*
    Convert validated fields to 64-bit WebsTime; validation bounds intermediate arithmetic.
 */
static WebsTime makeTime(struct tm *tp)
{
    int64 dayCount, seconds;
    int   year, month;

    year = tp->tm_year + 1900 + tp->tm_mon / 12;
    month = tp->tm_mon % 12;
    if (month < 0) {
        month += 12;
        --year;
    }
    dayCount = daysSinceEpoch(year);
    dayCount += leapYear(year) ? leapMonthStart[month] : normalMonthStart[month];
    dayCount += tp->tm_mday - 1;
    seconds = dayCount * SEC_PER_DAY + (((int64) tp->tm_hour * 60) + tp->tm_min) * 60 + tp->tm_sec;
    return (WebsTime) seconds;
}


static int lookupSym(char *token, int kind)
{
    TimeToken *tt;

    if ((tt = (TimeToken*) hashLookupSymbol(timeTokens, token)) == 0) {
        return -1;
    }
    if (kind != tt->type) {
        return -1;
    }
    return tt->value;
}


static int getNum(char **token, int sep)
{
    int num;

    if (*token == 0) {
        return 0;
    }

    num = atoi(*token);
    *token = strchr(*token, sep);
    if (*token) {
        *token += 1;
    }
    return num;
}


static int getNumOrSym(char **token, int sep, int kind, int *isAlpah)
{
    char *cp;
    int  num;

    assert(token && *token);

    /*
        Define the out-parameter on every path. The caller branches on it, and an early return here
        would otherwise leave it indeterminate for a date string that runs out of tokens.
     */
    *isAlpah = 0;

    if (token == NULL || *token == NULL || **token == '\0') {
        return 0;
    }
    if (isalpha((uchar) **token)) {
        *isAlpah = 1;
        cp = strchr(*token, sep);
        if (cp) {
            *cp++ = '\0';
        }
        num = lookupSym(*token, kind);
        *token = cp;
        return num;
    }
    num = atoi(*token);
    *token = strchr(*token, sep);
    if (*token) {
        *token += 1;
    }
    return num;
}


static int getDigits(cchar **cpp, int count, int *value)
{
    cchar *cp;
    int   i, v;

    cp = *cpp;
    v = 0;
    for (i = 0; i < count; i++) {
        if (!isdigit((uchar) cp[i])) {
            return -1;
        }
        v = (v * 10) + (cp[i] - '0');
    }
    *value = v;
    *cpp = &cp[count];
    return 0;
}


static int getMonthIndex(cchar **cpp)
{
    int i;

    for (i = 0; months[i].name; i++) {
        if (sncaselesscmp(*cpp, months[i].name, 3) == 0) {
            *cpp += 3;
            return months[i].value - 1;
        }
    }
    return -1;
}


static int getTimeOfDay(cchar **cpp, int *hour, int *min, int *sec)
{
    if (getDigits(cpp, 2, hour) < 0 || *(*cpp)++ != ':') {
        return -1;
    }
    if (getDigits(cpp, 2, min) < 0 || *(*cpp)++ != ':') {
        return -1;
    }
    return getDigits(cpp, 2, sec);
}


PUBLIC WebsTime websTime(void)
{
    time_t now;

    now = time(0);
    if (now == (time_t) -1 || (WebsTime) now < 0) {
        return -1;
    }
    return (WebsTime) now;
}


PUBLIC int websParseHttpDate(WebsTime *time, cchar *dateString)
{
    struct tm tm;
    cchar     *cp;
    int       mon, mday, year, hour, min, sec, sep, alpha;

    if (time == NULL || dateString == NULL) {
        return -1;
    }
    cp = dateString;
    for (alpha = 0; isalpha((uchar) cp[alpha]); alpha++) { }
    if (alpha < 3) {
        return -1;
    }
    cp += alpha;

    if (*cp == ',') {
        /*
            Parse IMF-fixdate or RFC 850, distinguished by the day separator.
         */
        if (*++cp != ' ') {
            return -1;
        }
        cp++;
        if (getDigits(&cp, 2, &mday) < 0) {
            return -1;
        }
        sep = *cp;
        if (sep != ' ' && sep != '-') {
            return -1;
        }
        cp++;
        if ((mon = getMonthIndex(&cp)) < 0 || *cp++ != sep) {
            return -1;
        }
        if (sep == ' ') {
            if (getDigits(&cp, 4, &year) < 0) {
                return -1;
            }
        } else {
            if (getDigits(&cp, 2, &year) < 0) {
                return -1;
            }
            year += (year < 70) ? 2000 : 1900;
        }
        if (*cp++ != ' ' || getTimeOfDay(&cp, &hour, &min, &sec) < 0 || *cp++ != ' ') {
            return -1;
        }
        if (!smatch(cp, "GMT") && !smatch(cp, "UTC")) {
            return -1;
        }

    } else if (*cp == ' ') {
        /*
            Parse asctime with a space-padded day and implied GMT.
         */
        cp++;
        if ((mon = getMonthIndex(&cp)) < 0 || *cp++ != ' ') {
            return -1;
        }
        if (*cp == ' ') {
            cp++;
            if (getDigits(&cp, 1, &mday) < 0) {
                return -1;
            }
        } else if (getDigits(&cp, 2, &mday) < 0) {
            return -1;
        }
        if (*cp++ != ' ' || getTimeOfDay(&cp, &hour, &min, &sec) < 0 || *cp++ != ' ') {
            return -1;
        }
        if (getDigits(&cp, 4, &year) < 0 || *cp != '\0') {
            return -1;
        }

    } else {
        return -1;
    }
    if (year < 1970 || mday < 1 || mday > 31 || hour > 23 || min > 59 || sec > 60) {
        return -1;
    }
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = mon;
    tm.tm_mday = mday;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    *time = makeTime(&tm);
    return 0;
}


static void swapDayMonth(struct tm *tp)
{
    int tmp;

    tmp = tp->tm_mday;
    tp->tm_mday = tp->tm_mon;
    tp->tm_mon = tmp;
}


/*
    Parse the a date/time string and return the result in *time. Missing date items may be provided
    via the defaults argument. This is a tolerant parser. It is not validating and will do its best
    to parse any possible date string.
 */
PUBLIC int websParseDateTime(WebsTime *time, cchar *dateString, struct tm *defaults)
{
    TimeToken *tt;
    struct tm tm;
    char      *str, *next, *token, *cp, *sep;
    int64     value;
    int       kind, hour, min, negate, value1, value2, value3, alpha, alpha2, alpha3;
    int       dateSep, offset, zoneOffset, fullYear;

    if (!dateString) {
        dateString = "";
    }
    offset = 0;
    zoneOffset = 0;
    sep = ", \t";
    cp = 0;
    next = 0;
    fullYear = 0;

    /*
        Set these mandatory values to -1 so we can tell if they are set to valid values
        WARNING: all the calculations use tm_year with origin 0, not 1900. It is fixed up below.
     */
    tm.tm_year = -MAXINT;
    tm.tm_mon = tm.tm_mday = tm.tm_hour = tm.tm_sec = tm.tm_min = tm.tm_wday = -1;
    tm.tm_min = tm.tm_sec = tm.tm_yday = -1;

#if ME_UNIX_LIKE && !CYGWIN
    tm.tm_gmtoff = 0;
    tm.tm_zone = 0;
#endif

    /*
        Set to -1 to try to determine if DST is in effect
     */
    tm.tm_isdst = -1;
    str = slower(sclone(dateString));

    /*
        Handle ISO dates: 2009-05-21t16:06:05.000z
     */
    if (strchr(str, ' ') == 0 && strchr(str, '-') && str[slen(str) - 1] == 'z') {
        for (cp = str; *cp; cp++) {
            if (*cp == '-') {
                *cp = '/';
            } else if (*cp == 't' && cp > str && isdigit((uchar) cp[-1]) && isdigit((uchar) cp[1]) ) {
                *cp = ' ';
            }
        }
    }
    token = stok(str, sep, &next);

    while (token && *token) {
        if (snumber(token)) {
            /*
                Parse either day of month or year. Priority to day of month. Format: <29> Jan <15> <2014>
             */
            value = atoi(token);
            if (value > 3000) {
                *time = value;
                wfree(str);
                return 0;
            } else if (value > 32 || (tm.tm_mday >= 0 && tm.tm_year == -MAXINT)) {
                if (value >= 1000) {
                    fullYear = 1;
                }
                tm.tm_year = (int) value - 1900;
            } else if (tm.tm_mday < 0) {
                tm.tm_mday = (int) value;
            }

        } else if (strncmp(token, "gmt", 3) == 0 || strncmp(token, "utc", 3) == 0) {
            /*
                Timezone format: GMT|UTC[+-]NN[:]NN
                GMT-08:00, UTC-0800
             */
            token += 3;
            if (token[0] == '-' || token[0] == '+') {
                negate = *token == '-' ? -1 : 1;
                token++;
            } else {
                negate = 1;
            }
            if (strchr(token, ':')) {
                hour = getNum(&token, timeSep);
                min = getNum(&token, timeSep);
            } else {
                hour = atoi(token);
                min = hour % 100;
                hour /= 100;
            }
            /*
                Bound the offset before scaling. The digits are attacker supplied and reach this
                via an unbounded atoi, so an out-of-range zone is discarded rather than scaled.
             */
            if (hour < -24 || hour > 24 || min < -59 || min > 59) {
                hour = min = 0;
            }
            zoneOffset = negate * (hour * 60 + min);

        } else if (isalpha((uchar) *token)) {
            if ((tt = (TimeToken*) hashLookupSymbol(timeTokens, token)) != 0) {
                kind = tt->type;
                value = tt->value;
                switch (kind) {

                case TOKEN_DAY:
                    tm.tm_wday = (int) value;
                    break;

                case TOKEN_MONTH:
                    tm.tm_mon = (int) value;
                    break;

                case TOKEN_OFFSET:
                    /* Named timezones or symbolic names like: tomorrow, yesterday, next week ... */
                    /* Units are seconds */
                    offset += (int) value;
                    break;

                case TOKEN_ZONE:
                    zoneOffset = (int) value;
                    break;

                default:
                    /* Just ignore unknown values */
                    break;
                }
            }

        } else if ((cp = strchr(token, timeSep)) != 0 && isdigit((uchar) token[0])) {
            /*
                Time:  10:52[:23]
                Must not parse GMT-07:30
             */
            tm.tm_hour = getNum(&token, timeSep);
            tm.tm_min = getNum(&token, timeSep);
            tm.tm_sec = getNum(&token, timeSep);

        } else {
            dateSep = '/';
            if (strchr(token, dateSep) == 0) {
                dateSep = '-';
                if (strchr(token, dateSep) == 0) {
                    dateSep = '.';
                    if (strchr(token, dateSep) == 0) {
                        dateSep = 0;
                    }
                }
            }
            if (dateSep) {
                /*
                    Date:  07/28/2014, 07/28/08, Jan/28/2014, Jaunuary-28-2014, 28-jan-2014
                    Support order: dd/mm/yy, mm/dd/yy and yyyy/mm/dd
                    Support separators "/", ".", "-"
                 */
                value1 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha);
                value2 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha2);
                value3 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha3);

                if (value1 > 31) {
                    /* yy/mm/dd */
                    tm.tm_year = value1;
                    tm.tm_mon = value2;
                    tm.tm_mday = value3;

                } else if (value1 > 12 || alpha2) {
                    /*
                        dd/mm/yy
                        Cannot detect 01/02/03  This will be evaluated as Jan 2 2003 below.
                     */
                    tm.tm_mday = value1;
                    tm.tm_mon = value2;
                    tm.tm_year = value3;

                } else {
                    /*
                        The default to parse is mm/dd/yy unless the mm value is out of range
                     */
                    tm.tm_mon = value1;
                    tm.tm_mday = value2;
                    tm.tm_year = value3;
                }
            }
        }
        token = stok(NULL, sep, &next);
    }

    /*
        Y2K fix and rebias
     */
    if (0 <= tm.tm_year && tm.tm_year < 100 && !fullYear) {
        if (tm.tm_year < 50) {
            tm.tm_year += 2000;
        } else {
            tm.tm_year += 1900;
        }
    }
    if (tm.tm_year >= 1900) {
        tm.tm_year -= 1900;
    }

    /*
        Convert back to origin 0 for months
     */
    if (tm.tm_mon > 0) {
        tm.tm_mon--;
    }

    /*
        Validate and fill in missing items with defaults
     */
    validateTime(&tm, defaults);
    *time = makeTime(&tm);
    *time += -(zoneOffset * SEC_PER_MIN);
    *time += offset;
    wfree(str);
    return 0;
}


static void validateTime(struct tm *tp, struct tm *defaults)
{
    struct tm empty;

    /*
        Fix apparent day-mon-year ordering issues. Cannot fix everything!
     */
    if ((12 <= tp->tm_mon && tp->tm_mon <= 31) && 0 <= tp->tm_mday && tp->tm_mday <= 11) {
        /*
            Looks like day month are swapped
         */
        swapDayMonth(tp);
    }

    /*
        Check for overflow. Underflow validated below.
        tm_sec can be 61 for leap seconds.
     */
    if (tp->tm_sec > 61) {
        tp->tm_sec = -1;
    }
    if (tp->tm_min > 59) {
        tp->tm_min = -1;
    }
    if (tp->tm_hour > 23) {
        tp->tm_hour = -1;
    }
    if (tp->tm_mday > 31) {
        tp->tm_mday = -1;
    }
    if (tp->tm_mon > 11) {
        tp->tm_mon = -1;
    }
    if (tp->tm_wday > 6) {
        tp->tm_wday = -1;
    }
    if (tp->tm_yday > 366) {
        tp->tm_yday = -1;
    }

    /*
        Bound the year to 0-9999 so the calendar arithmetic in makeTime cannot overflow. A year
        outside that range is treated as unset and filled from the defaults below.
     */
    if (tp->tm_year != -MAXINT && (tp->tm_year < -1900 || tp->tm_year > 8099)) {
        tp->tm_year = -MAXINT;
    }

#if UNUSED
    if (tp->tm_year != -MAXINT && tp->tm_mon >= 0 && tp->tm_mday >= 0 && tp->tm_hour >= 0) {
        /*  Everything defined */
        return;
    }
#endif

    /*
        Use empty time if missing
     */
    if (defaults == NULL) {
        memset(&empty, 0, sizeof(empty));
        defaults = &empty;
        empty.tm_mday = 1;
        empty.tm_year = 70;
    }
    if (tp->tm_hour < 0 && tp->tm_min < 0 && tp->tm_sec < 0) {
        tp->tm_hour = defaults->tm_hour;
        tp->tm_min = defaults->tm_min;
        tp->tm_sec = defaults->tm_sec;
    }

    /*
        Get weekday, if before today then make next week
     */
    if (tp->tm_wday >= 0 && tp->tm_year == -MAXINT && tp->tm_mon < 0 && tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday + (tp->tm_wday - defaults->tm_wday + 7) % 7;
        tp->tm_mon = defaults->tm_mon;
        tp->tm_year = defaults->tm_year;
    }

    /*
        Get month, if before this month then make next year
     */
    if (tp->tm_mon >= 0 && tp->tm_mon <= 11 && tp->tm_mday < 0) {
        if (tp->tm_year == -MAXINT) {
            tp->tm_year = defaults->tm_year + (((tp->tm_mon - defaults->tm_mon) < 0) ? 1 : 0);
        }
        tp->tm_mday = defaults->tm_mday;
    }

    /*
        Get date, if before current time then make tomorrow
     */
    if (tp->tm_hour >= 0 && tp->tm_year == -MAXINT && tp->tm_mon < 0 && tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday + ((tp->tm_hour - defaults->tm_hour) < 0 ? 1 : 0);
        tp->tm_mon = defaults->tm_mon;
        tp->tm_year = defaults->tm_year;
    }
    if (tp->tm_year == -MAXINT) {
        tp->tm_year = defaults->tm_year;
    }
    if (tp->tm_mon < 0) {
        tp->tm_mon = defaults->tm_mon;
    }
    if (tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday;
    }
    if (tp->tm_yday < 0) {
        if (tp->tm_mon <= 11) {
            tp->tm_yday = (leapYear(tp->tm_year + 1900) ?
                           leapMonthStart[tp->tm_mon] : normalMonthStart[tp->tm_mon]) + tp->tm_mday - 1;
        } else {
            tp->tm_yday = defaults->tm_yday;
        }
    }
    if (tp->tm_hour < 0) {
        tp->tm_hour = defaults->tm_hour;
    }
    if (tp->tm_min < 0) {
        tp->tm_min = defaults->tm_min;
    }
    if (tp->tm_sec < 0) {
        tp->tm_sec = defaults->tm_sec;
    }
}


/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/upload.c ************/

/*
    upload.c -- File upload handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

#if ME_GOAHEAD_UPLOAD
/************************************ Locals **********************************/
/*
    Upload states
 */
#define UPLOAD_REQUEST_HEADER 1      /* Request header */
#define UPLOAD_BOUNDARY       2      /* Boundary divider */
#define UPLOAD_CONTENT_HEADER 3      /* Content part header */
#define UPLOAD_CONTENT_DATA   4      /* Content encoded data */
#define UPLOAD_CONTENT_END    5      /* End of multipart message */

#define MAX_BOUNDARY          70     /* RFC 2046 5.1.1 */
#define MAX_PADDING           256    /* Transport padding accepted after a delimiter */

#define BOUNDARY_MORE         -1     /* The data ends inside a possible delimiter */

static cchar *uploadDir;

/*
    The upload variable prefixes, indexed by UPLOAD_VAR_*. Both defineUploadVars and websIsUploadVar use them.
 */
#define UPLOAD_VAR_CLIENT_FILENAME 0
#define UPLOAD_VAR_CONTENT_TYPE    1
#define UPLOAD_VAR_FILENAME        2
#define UPLOAD_VAR_SIZE            3

static cchar *uploadVars[] = {
    "FILE_CLIENT_FILENAME_", "FILE_CONTENT_TYPE_", "FILE_FILENAME_", "FILE_SIZE_", 0
};

/*********************************** Forwards *********************************/

static void defineUploadVars(Webs *wp);
static int  delimiterSuffix(cchar *cp, cchar *end);
static char *getBoundary(Webs *wp, char *buf, ssize bufLen);
static void initUpload(Webs *wp);
static void processContentBoundary(Webs *wp, char *line);
static bool processContentData(Webs *wp);
static void processUploadHeader(Webs *wp, char *line);

/************************************* Code ***********************************/
/*
    The upload handler functions as a filter.
    Return false because the upload handler is not a terminal handler.
 */
static bool uploadHandler(Webs *wp)
{
    return 0;
}


/*
    Test a boundary against RFC 2046 5.1.1: 1 to 70 bchars, not ending in a space
 */
static bool validBoundary(cchar *boundary)
{
    ssize len;

    len = slen(boundary);
    if (len == 0 || len > MAX_BOUNDARY || boundary[len - 1] == ' ') {
        return 0;
    }
    for (; *boundary; boundary++) {
        if (!isalnum((uchar) *boundary) && !strchr("'()+_,-./:=? ", *boundary)) {
            return 0;
        }
    }
    return 1;
}


static void initUpload(Webs *wp)
{
    char *boundary;

    if (wp->uploadState == 0) {
        wp->uploadState = UPLOAD_BOUNDARY;
        boundary = websParseContentType(wp->contentType, "boundary");
        if (boundary && validBoundary(boundary)) {
            wfreeset(wp->boundary);
            /*
                A delimiter is CRLF "--" boundary (RFC 2046 5.1.1). The first delimiter has no preceding CRLF and is
                matched against wp->boundary + 2.
             */
            wp->boundary = sfmt("\r\n--%s", boundary);
            wp->boundaryLen = strlen(wp->boundary);
        }
        wfree(boundary);
        if (wp->boundaryLen == 0) {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Bad boundary");
        } else {
            websSetVar(wp, "UPLOAD_DIR", uploadDir);
            wp->files = hashCreate(11);
        }
    }
}


static void freeUploadFile(WebsUpload *up)
{
    if (up) {
        if (up->filename) {
            unlink(up->filename);
            wfreeset(up->filename);
        }
        wfreeset(up->clientFilename);
        wfreeset(up->contentType);
        wfree(up);
    }
}


PUBLIC void websFreeUpload(Webs *wp)
{
    WebsUpload *up;
    WebsKey    *s;

    if (wp->files >= 0) {
        for (s = hashFirst(wp->files); s; s = hashNext(wp->files, s)) {
            up = s->content.value.symbol;
            freeUploadFile(up);
            if (up == wp->currentFile) {
                wp->currentFile = 0;
            }
        }
        hashFree(wp->files);
        wp->files = -1;
    }
    if (wp->currentFile) {
        freeUploadFile(wp->currentFile);
        wp->currentFile = 0;
    }
    if (wp->upfd >= 0) {
        close(wp->upfd);
        wp->upfd = -1;
    }
}


PUBLIC bool websProcessUploadData(Webs *wp)
{
    char  *line, *nextTok;
    ssize nbytes;
    bool  canProceed;

    line = 0;
    nbytes = 0;
    canProceed = 1;
    while (canProceed && !wp->finalized && wp->uploadState != UPLOAD_CONTENT_END) {
        if (wp->uploadState == UPLOAD_BOUNDARY || wp->uploadState == UPLOAD_CONTENT_HEADER) {
            /*
                Parse the next input line
             */
            line = bufStart(&wp->input);
            if ((nextTok = memchr(line, '\n', bufLen(&wp->input))) != 0) {
                *nextTok++ = '\0';
                nbytes = nextTok - line;
            } else if (wp->eof && bufLen(&wp->input) > 0) {
                //  No more input: the rest is the last line, such as a close delimiter without a final CRLF
                nbytes = bufLen(&wp->input);
            } else {
                /* Incomplete line */
                canProceed = 0;
                break;
            }
            assert(nbytes > 0);
            strim(line, "\r", WEBS_TRIM_END);
        }
        switch (wp->uploadState) {
        case 0:
            initUpload(wp);
            break;

        case UPLOAD_BOUNDARY:
            processContentBoundary(wp, line);
            websConsumeInput(wp, nbytes);
            break;

        case UPLOAD_CONTENT_HEADER:
            processUploadHeader(wp, line);
            websConsumeInput(wp, nbytes);
            break;

        case UPLOAD_CONTENT_DATA:
            canProceed = processContentData(wp);
            if (bufLen(&wp->input) < wp->boundaryLen) {
                /*  Incomplete boundary - return to get more data */
                canProceed = 0;
            }
            break;

        case UPLOAD_CONTENT_END:
            break;
        }
    }
    if (wp->eof && !wp->finalized && wp->uploadState != UPLOAD_CONTENT_END) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing closing boundary");
        return 1;
    }
    bufCompact(&wp->input);
    return canProceed;
}


/*
    Expecting a delimiter line: "--" boundary, then transport padding, or "--" to close the body
 */
static void processContentBoundary(Webs *wp, char *line)
{
    cchar *suffix;
    ssize len;

    len = wp->boundaryLen - 2;
    if (strncmp(wp->boundary + 2, line, len) != 0) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Incomplete boundary");
        return;
    }
    suffix = &line[len];
    if (suffix[0] == '-' && suffix[1] == '-') {
        wp->uploadState = UPLOAD_CONTENT_END;

    } else if ((ssize) strspn(suffix, " \t") == slen(suffix)) {
        if (++wp->uploadParts > ME_GOAHEAD_LIMIT_PARAMS) {
            websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Too many upload parts");
            return;
        }
        wp->uploadState = UPLOAD_CONTENT_HEADER;

    } else {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Bad boundary");
    }
}


/*
    Accept only an empty or plain filename without prohibited dots, separators, characters, or device names.
    Do not normalize client filenames.
 */
static bool validClientFilename(cchar *name)
{
    ssize len;

    len = slen(name);
    if (len == 0) {
        return 1;
    }
    if (*name == '.' || name[len - 1] == '.' || name[len - 1] == ' ') {
        return 0;
    }
    if (!websValidUriChars(name) || strpbrk(name, "\\/:*?<>|~\"'%`^\n\r\t\f")) {
        return 0;
    }
    return !websIsReservedDevice(name);
}


static void processUploadHeader(Webs *wp, char *line)
{
    WebsUpload *file;
    char       *key, *headerTok, *rest, *nextPair, *value;

    if (line[0] == '\0') {
        wp->uploadState = UPLOAD_CONTENT_DATA;
        return;
    }
    trace(7, "Header line: %s", line);

    headerTok = line;
    stok(line, ": ", &rest);

    if (scaselesscmp(headerTok, "Content-Disposition") == 0) {
        /*
            The content disposition header describes either a form variable or an uploaded file.

            Content-Disposition: form-data; name="field1"
            >>blank line
            Field Data
            ---boundary

            Content-Disposition: form-data; name="field1" filename="user.file"
            >>blank line
            File data
            ---boundary
         */
        key = rest;
        wfreeset(wp->uploadVar);
        wfreeset(wp->clientFilename);

        while (key && stok(key, ";\r\n", &nextPair)) {

            key = strim(key, " ", WEBS_TRIM_BOTH);
            ssplit(key, "= ", &value);
            value = strim(value, "\"", WEBS_TRIM_BOTH);

            if (scaselesscmp(key, "form-data") == 0) {
                /* Nothing to do */

            } else if (scaselesscmp(key, "name") == 0) {
                if (value == 0 || *value == '\0') {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Empty name field");
                    return;
                }
                wfreeset(wp->uploadVar);
                wp->uploadVar = sclone(value);

            } else if (scaselesscmp(key, "filename") == 0) {
                if (wp->uploadVar == 0) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing name field");
                    return;
                }
                /*
                    A part may open only one file. wp->upfd is the per-part descriptor and is reset
                    when the part ends, so a descriptor open here means a repeated filename= pair or
                    a repeated Content-Disposition header. Reject rather than leak the descriptor.
                 */
                if (wp->upfd >= 0) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Duplicate filename field");
                    return;
                }
                if (++wp->uploadFiles > ME_GOAHEAD_LIMIT_UPLOAD_FILES) {
                    websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Too many upload files");
                    return;
                }
                if (!validClientFilename(value)) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload client filename");
                    return;
                }
                wfreeset(wp->clientFilename);
                wp->clientFilename = sclone(value);

                /*
                    Create the file to hold the uploaded data
                 */
                wfreeset(wp->uploadTmp);
                if ((wp->uploadTmp = websTempFile(uploadDir, "tmp")) == 0) {
                    /*
                        Do not name the temp file here: the assignment above is what failed, so
                        wp->uploadTmp is unconditionally NULL on this path. gcc proves it at -O2
                        and reports it as a null %s argument.
                     */
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR,
                              "Cannot create upload temp file. Check upload temp dir %s", uploadDir);
                    return;
                }
                trace(5, "File upload of: %s stored as %s", wp->clientFilename, wp->uploadTmp);

                /*
                    websTempFile created the file, private and empty, so this opens an existing file
                    rather than creating one. O_NOFOLLOW keeps that true if the path is replaced.
                 */
                if ((wp->upfd = open(wp->uploadTmp, O_WRONLY | O_TRUNC | O_BINARY | O_NOFOLLOW, 0600)) < 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot open upload temp file %s", wp->uploadTmp);
                    /*
                        Remove the temporary file on open failure; no request state owns it yet.
                     */
                    unlink(wp->uploadTmp);
                    wfreeset(wp->uploadTmp);
                    return;
                }
                /*
                    Create the files[id]
                 */
                freeUploadFile(wp->currentFile);
                file = wp->currentFile = walloc(sizeof(WebsUpload));
                memset(file, 0, sizeof(WebsUpload));
                file->clientFilename = sclone(wp->clientFilename);
                file->filename = sclone(wp->uploadTmp);
            }
            key = nextPair;
        }

    } else if (scaselesscmp(headerTok, "Content-Type") == 0) {
        if (wp->clientFilename) {
            trace(5, "Set files[%s][CONTENT_TYPE] = %s", wp->uploadVar, rest);
            //  A repeated line replaces the earlier value
            wfreeset(wp->currentFile->contentType);
            wp->currentFile->contentType = sclone(rest);
        }
    }
}


static void setUploadVar(Webs *wp, int index, cchar *value)
{
    char *key;

    key = sfmt("%s%s", uploadVars[index], wp->uploadVar);
    websSetVar(wp, key, value);
    wfree(key);
}


static void defineUploadVars(Webs *wp)
{
    WebsUpload *file;
    char       size[32];

    file = wp->currentFile;
    setUploadVar(wp, UPLOAD_VAR_CLIENT_FILENAME, file->clientFilename);
    setUploadVar(wp, UPLOAD_VAR_CONTENT_TYPE, file->contentType);
    setUploadVar(wp, UPLOAD_VAR_FILENAME, file->filename);
    fmt(size, sizeof(size), "%zd", file->size);
    setUploadVar(wp, UPLOAD_VAR_SIZE, size);
}


PUBLIC bool websIsUploadVar(cchar *name)
{
    cchar **prefix;

    if (smatch(name, "UPLOAD_DIR")) {
        return 1;
    }
    for (prefix = uploadVars; *prefix; prefix++) {
        if (sstarts(name, *prefix)) {
            return 1;
        }
    }
    return 0;
}


static int writeToFile(Webs *wp, char *data, ssize len)
{
    WebsUpload *file;
    ssize      rc;

    file = wp->currentFile;

    if ((file->size + len) > ME_GOAHEAD_LIMIT_UPLOAD) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Uploaded file exceeds maximum %zd",
                  (ssize) ME_GOAHEAD_LIMIT_UPLOAD);
        return -1;
    }
    if (len > 0) {
        /*
            File upload. Write the file data.
         */
        if ((rc = write(wp->upfd, data, (int) len)) != len) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot write to upload temp file %s, rc %zd", wp->uploadTmp,
                      rc);
            return -1;
        }
        file->size += len;
        trace(7, "uploadFilter: Wrote %zd bytes to %s", len, wp->uploadTmp);
    }
    return 0;
}


static bool processContentData(Webs *wp)
{
    WebsUpload *file;
    WebsBuf    *content;
    WebsKey    *sp;
    ssize      size, nbytes, len;
    char       *data, *bp;

    content = &wp->input;
    file = wp->currentFile;

    size = bufLen(content);
    if (size < wp->boundaryLen) {
        /*  Incomplete boundary. Return and get more data */
        return 0;
    }
    //  The boundary scan and the writes below read the content as one flat block
    bufStart(content);
    if ((bp = getBoundary(wp, content->servp, size)) == 0) {
        if (wp->clientFilename) {
            /* Keep a possible split delimiter; bytes before boundaryScan are safe to write. */
            nbytes = wp->boundaryScan;
            if (nbytes > 0) {
                if (writeToFile(wp, content->servp, nbytes) < 0) {
                    /* Proceed to handle error */
                    return 1;
                }
                websConsumeInput(wp, nbytes);
                wp->boundaryScan = 0;
            }
            //  Get more data
            return 0;
        }
        /*
            Retain form fields until their delimiter arrives. Resume at boundaryScan to keep scanning linear; the
            body limit bounds memory.
         */
        return 0;
    }
    data = content->servp;
    len = bp - data;
    //  Consume the CRLF that begins the delimiter too, leaving the delimiter line for processContentBoundary
    nbytes = len + 2;
    wp->boundaryScan = 0;
    if (wp->clientFilename) {
        /*
            Write the last bit of file data and add to the list of files and define environment variables
         */
        if (len > 0) {
            if (writeToFile(wp, data, len) < 0) {
                /* Proceed to handle error */
                websConsumeInput(wp, nbytes);
                return 1;
            }
        }
        /* Free a repeated field's previous upload and staging file; hashEnter frees only string values. */
        if ((sp = hashLookup(wp->files, wp->uploadVar)) != 0) {
            freeUploadFile(sp->content.value.symbol);
        }
        hashEnter(wp->files, wp->uploadVar, valueSymbol(file), 0);
        defineUploadVars(wp);
        wp->currentFile = 0;

    } else if (wp->uploadVar) {
        /*
            Normal string form data variables
         */
        data[len] = '\0';
        //  Multipart names and values are literal (RFC 7578), not URL encoded
#if ME_DEBUG
        trace(5, "uploadFilter: form[%s] = %s", wp->uploadVar, data);
#else
        trace(5, "uploadFilter: form[%s]", wp->uploadVar);
#endif
        websSetRequestVar(wp, wp->uploadVar, data, WEBS_VAR_BODY);
    }
    websConsumeInput(wp, nbytes);
    if (wp->clientFilename) {
        /*
            Now have all the data (we've seen the boundary)
         */
        close(wp->upfd);
        wp->upfd = -1;
        wfreeset(wp->clientFilename);
        wfreeset(wp->uploadTmp);
    }
    wp->uploadState = UPLOAD_BOUNDARY;
    return 1;
}


/*
    Return 1 for a complete delimiter suffix, 0 for data, or BOUNDARY_MORE for incomplete input.
 */
static int delimiterSuffix(cchar *cp, cchar *end)
{
    cchar *start;

    if (cp < end && *cp == '-') {
        if (cp + 1 >= end) {
            return BOUNDARY_MORE;
        }
        return cp[1] == '-';
    }
    for (start = cp; cp < end && (*cp == ' ' || *cp == '\t'); cp++) {
        if (cp - start >= MAX_PADDING) {
            return 0;
        }
    }
    if (cp + 1 >= end) {
        return cp < end && *cp != '\r' ? 0 : BOUNDARY_MORE;
    }
    return cp[0] == '\r' && cp[1] == '\n';
}


/*
    Resume delimiter scanning at boundaryScan. Return the delimiter CRLF, or 0 with boundaryScan at the first
    possible partial delimiter. Work stays linear in part size.
 */
static char *getBoundary(Webs *wp, char *buf, ssize bufLen)
{
    char  *cp, *end, *suffix;
    ssize avail;
    int   rc;

    assert(buf);

    end = buf + bufLen;
    for (cp = buf + wp->boundaryScan; cp < end; cp++) {
        if ((cp = memchr(cp, '\r', end - cp)) == 0) {
            break;
        }
        avail = end - cp;
        if (avail < wp->boundaryLen) {
            if (memcmp(cp, wp->boundary, avail) == 0) {
                wp->boundaryScan = cp - buf;
                return 0;
            }
            continue;
        }
        if (memcmp(cp, wp->boundary, wp->boundaryLen) == 0) {
            suffix = cp + wp->boundaryLen;
            if ((rc = delimiterSuffix(suffix, end)) == 1) {
                return cp;
            } else if (rc == BOUNDARY_MORE) {
                wp->boundaryScan = cp - buf;
                return 0;
            }
        }
    }
    wp->boundaryScan = bufLen;
    return 0;
}


WebsUpload *websLookupUpload(Webs *wp, cchar *key)
{
    WebsKey *sp;

    if (wp->files >= 0) {
        if ((sp = hashLookup(wp->files, key)) == 0) {
            return 0;
        }
        return sp->content.value.symbol;
    }
    return 0;
}


WebsHash websGetUpload(Webs *wp)
{
    return wp->files;
}


PUBLIC void websUploadOpen(void)
{
    uploadDir = ME_GOAHEAD_UPLOAD_DIR;
    if (*uploadDir == '\0') {
        uploadDir = websGetTempDir();
    }
    trace(4, "Upload directory is %s", uploadDir);
    websDefineHandler("upload", 0, uploadHandler, 0, 0);
}

#endif /* ME_GOAHEAD_UPLOAD */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */

#else
void dummyGoahead(){}
#endif /* ME_COM_GOAHEAD */
