/* Isolate the actual CGI handler from request setup and process creation. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_GOAHEAD_CGI && !VXWORKS
static int primaryMode, fallbackMode, reportedCode, statCalls, tempCalls;
static int fixtureAccessError, fixtureArgAllocError;
static char **capturedEnv;
static cchar *expectedEntry;
static bool checkEnv;
static void (*inspectEnv)(char **env);
static void fakeSetEnv(Webs *wp) {}
static WebsKey *fakeSetVar(Webs *wp, cchar *name, cchar *value) { return NULL; }
static WebsKey *fakeSetVarFmt(Webs *wp, cchar *name, cchar *format, ...) { return NULL; }
static void fakeError(Webs *wp, int code, cchar *format, ...) { reportedCode = code & 0xFFFF; }
static int fakeStat(cchar *path, WebsStat *info)
{
    int mode = statCalls++ == 0 ? primaryMode : fallbackMode;
    memset(info, 0, sizeof(*info));
    info->st_mode = (ushort) mode;
    return mode ? 0 : -1;
}
static int fakeAccess(cchar *path, int mode) { return fixtureAccessError ? -1 : 0; }
static void *captureAlloc(ssize size)
{
    void *ptr;
    if (fixtureArgAllocError && size == 10 * (ssize) sizeof(char*)) return NULL;
    ptr = walloc(size);
    if (size == 64 * (ssize) sizeof(char*)) {
        capturedEnv = ptr;
        memset(ptr, 0xA5, (size_t) size);
    }
    return ptr;
}
static char *fakeTempFile(cchar *dir, cchar *prefix)
{
    uintptr_t poison;
    memset(&poison, 0xA5, sizeof(poison));
    tempCalls++;
    if (inspectEnv) {
        inspectEnv(capturedEnv);
    }
    if (checkEnv) {
        tnotnull(capturedEnv, "environment allocated");
        if (expectedEntry) {
            ttrue((uintptr_t) capturedEnv[0] != poison, "first environment slot initialized");
            tmatch(capturedEnv[0], expectedEntry, "zero-handle variable forwarded");
            tnull(capturedEnv[1], "environment terminated after entry");
        } else {
            tnull(capturedEnv[0], "empty environment terminates at index zero");
        }
    }
    return NULL; /* Exercise cleanup without spawning a process or creating a file. */
}
#define websSetEnv fakeSetEnv
#define websSetVar fakeSetVar
#define websSetVarFmt fakeSetVarFmt
#define websError fakeError
#undef stat
#define stat fakeStat
#define access fakeAccess
#define walloc captureAlloc
#define websTempFile fakeTempFile
/* Use the production module's normal signed-conversion policy. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/cgi.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#undef websSetEnv
#undef websSetVar
#undef websSetVarFmt
#undef websError
#undef stat
#undef access
#undef walloc
#undef websTempFile
static void runHandler(WebsHash vars)
{
    Webs wp;
    WebsRoute route;
    memset(&wp, 0, sizeof(wp));
    memset(&route, 0, sizeof(route));
    route.extensions = -1;
    route.prefix = "/cgi-bin";
    route.prefixLen = slen(route.prefix);
    wp.route = &route;
    wp.path = "/cgi-bin/test.exe";
    wp.query = "key=value";
    wp.vars = vars;
    wp.cgifd = -1;
    capturedEnv = NULL;
    reportedCode = statCalls = tempCalls = 0;
    cgiHandler(&wp);
}

#endif
