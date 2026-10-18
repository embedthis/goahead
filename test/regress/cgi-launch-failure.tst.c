/*
    cgi-launch-failure.tst.c - A CGI launch that fails releases its descriptors and its staging files
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

#if ME_GOAHEAD_CGI && ME_UNIX_LIKE
static int  failFork, reportedCode, tempCount;
static char *tempNames[4];

static pid_t fakeFork(void)
{
    if (failFork) {
        errno = EAGAIN;
        return -1;
    }
    return fork();
}

static int fakeStat(cchar *path, WebsStat *info)
{
    memset(info, 0, sizeof(*info));
    info->st_mode = S_IFREG | 0755;
    return 0;
}

static int fakeAccess(cchar *path, int mode) { return 0; }
static void fakeSetEnv(Webs *wp) {}
static WebsKey *fakeSetVar(Webs *wp, cchar *name, cchar *value) { return NULL; }
static WebsKey *fakeSetVarFmt(Webs *wp, cchar *name, cchar *format, ...) { return NULL; }
static void fakeError(Webs *wp, int code, cchar *format, ...) { reportedCode = code & 0xFFFF; }

static char *recordTempFile(cchar *dir, cchar *prefix)
{
    char *path;

    if ((path = websTempFile(dir, prefix)) != 0 && tempCount < 4) {
        tempNames[tempCount++] = sclone(path);
    }
    return path;
}

#define fork fakeFork
#undef stat
#define stat fakeStat
#define access fakeAccess
#define websSetEnv fakeSetEnv
#define websSetVar fakeSetVar
#define websSetVarFmt fakeSetVarFmt
#define websError fakeError
#define websTempFile recordTempFile
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/cgi.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef fork
#undef stat
#undef access
#undef websSetEnv
#undef websSetVar
#undef websSetVarFmt
#undef websError
#undef websTempFile

static CgiPid launch(cchar *path, char *in, char *out)
{
    Cgi cgi;

    memset(&cgi, 0, sizeof(cgi));
    cgi.cgiPath = (char*) path;
    cgi.stdIn = in;
    cgi.stdOut = out;
    return launchCgi(&cgi, "/");
}


static int lowestFree(void)
{
    int fd;

    fd = open("/dev/null", O_RDONLY);
    close(fd);
    return fd;
}


static void suiteA_stdoutOpen(void)
{
    char *in;
    int  before;

    tinfo("\n--- Suite A: the stdout file cannot be opened ---\n");
    in = websTempFile(NULL, "cgi");
    ttrue(in != 0, "create the stdin file");
    before = lowestFree();
    ttrue(launch("/bin/true", in, "/nonexistent/goahead-cgi-out") == -1, "the launch must fail");
    teq(lowestFree(), before, "the stdin descriptor must be closed");
    unlink(in);
    wfree(in);
}


static void suiteB_fork(void)
{
    char *in, *out;
    int  before;

    tinfo("\n--- Suite B: fork fails ---\n");
    in = websTempFile(NULL, "cgi");
    out = websTempFile(NULL, "cgi");
    ttrue(in && out, "create the stdin and stdout files");
    before = lowestFree();
    failFork = 1;
    ttrue(launch("/bin/true", in, out) == -1, "the launch must fail");
    failFork = 0;
    teq(lowestFree(), before, "both descriptors must be closed");
    unlink(in);
    unlink(out);
    wfree(in);
    wfree(out);
}


static CgiPid launchIn(cchar *path, cchar *dir, char *in, char *out)
{
    Cgi cgi;

    memset(&cgi, 0, sizeof(cgi));
    cgi.cgiPath = (char*) path;
    cgi.stdIn = in;
    cgi.stdOut = out;
    return launchCgi(&cgi, (char*) dir);
}


static void suiteD_childDirectory(void)
{
    char  before[ME_GOAHEAD_LIMIT_FILENAME], after[ME_GOAHEAD_LIMIT_FILENAME], response[256], *in, *out;
    ssize nbytes;
    int   fd, status;
    pid_t pid;

    tinfo("\n--- Suite D: the child cannot enter the CGI directory ---\n");
    in = websTempFile(NULL, "cgi");
    out = websTempFile(NULL, "cgi");
    ttrue(in && out, "create the stdin and stdout files");
    tnotnull(getcwd(before, sizeof(before)), "save the server directory");
    pid = launchIn("/bin/true", "/nonexistent/goahead-cgi-dir", in, out);
    ttrue(pid > 0, "the child must start");
    ttrue(waitpid(pid, &status, 0) == pid, "reap the child");
    ttrue(WIFEXITED(status) && WEXITSTATUS(status) == 127, "the child must exit without running the program");
    fd = open(out, O_RDONLY);
    ttrue(fd >= 0, "open the stdout file");
    nbytes = read(fd, response, sizeof(response) - 1);
    ttrue(nbytes > 0, "the child must report the failure");
    response[nbytes > 0 ? nbytes : 0] = '\0';
    close(fd);
    ttrue(sstarts(response, "Status: 500"), "the failure must be a 500: %s", response);
    tnotnull(getcwd(after, sizeof(after)), "read the server directory");
    tmatch(after, before, "the server directory is unchanged");
    unlink(in);
    unlink(out);
    wfree(in);
    wfree(out);
}


static void suiteC_handler(void)
{
    Webs      wp;
    WebsRoute route;
    struct stat info;
    int       before, i;

    tinfo("\n--- Suite C: the handler removes the staging files ---\n");
    memset(&wp, 0, sizeof(wp));
    memset(&route, 0, sizeof(route));
    route.extensions = -1;
    route.prefix = "/cgi-bin/";
    wp.route = &route;
    wp.path = "/cgi-bin/program";
    wp.query = "";
    wp.vars = hashCreate(7);
    wp.cgifd = -1;
    before = lowestFree();
    failFork = 1;
    cgiHandler(&wp);
    failFork = 0;
    teq(reportedCode, HTTP_CODE_INTERNAL_SERVER_ERROR, "a failed launch must be reported");
    teq(tempCount, 2, "the handler must have staged stdin and stdout files");
    for (i = 0; i < tempCount; i++) {
        ttrue(stat(tempNames[i], &info) < 0, "a staging file must be removed: %s", tempNames[i]);
        wfree(tempNames[i]);
    }
    teq(lowestFree(), before, "the handler must leave no descriptor open");
    wfree(wp.cgiStdin);
    hashFree(wp.vars);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && ME_UNIX_LIKE
    websOsOpen();
    websRuntimeOpen();
    suiteA_stdoutOpen();
    suiteB_fork();
    suiteC_handler();
    suiteD_childDirectory();
#else
    tinfo("The CGI launcher tested here is the Unix one");
#endif
    return 0;
}
