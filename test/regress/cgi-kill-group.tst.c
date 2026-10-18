/*
    cgi-kill-group.tst.c - Aborting a CGI kills the programs it started, not only the CGI process
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
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/cgi.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/*
    Count zombies as gone: they execute nothing, and container init may not reap them.
 */
static bool running(pid_t pid)
{
    FILE *fp;
    char path[64], state;
    int  n;

    if (kill(pid, 0) < 0) {
        return 0;
    }
    fmt(path, sizeof(path), "/proc/%d/stat", (int) pid);
    if ((fp = fopen(path, "r")) != 0) {
        n = fscanf(fp, "%*d %*s %c", &state);
        fclose(fp);
        if (n == 1 && state == 'Z') {
            return 0;
        }
    }
    return 1;
}


static pid_t readPid(cchar *path)
{
    FILE *fp;
    int  pid;

    pid = 0;
    if ((fp = fopen(path, "r")) != 0) {
        if (fscanf(fp, "%d", &pid) != 1) {
            pid = 0;
        }
        fclose(fp);
    }
    return (pid_t) pid;
}


static void killGroup(void)
{
    Cgi   cgi;
    char  *in, *out, *pidFile, *script, *argv[4], *envp[2];
    pid_t helper;
    int   i;

    tinfo("\n--- A CGI that starts a helper and is aborted ---\n");

    in = websTempFile(NULL, "cgi");
    out = websTempFile(NULL, "cgi");
    pidFile = websTempFile(NULL, "cgipid");
    ttrue(in && out && pidFile, "create the staging files");
    unlink(pidFile);

    /* Publish the pid file by rename so the test never reads it half written */
    script = sfmt("sleep 30 & echo $! > %s.tmp; mv %s.tmp %s; sleep 30", pidFile, pidFile, pidFile);
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = script;
    argv[3] = NULL;
    envp[0] = "PATH=/bin:/usr/bin";
    envp[1] = NULL;

    memset(&cgi, 0, sizeof(cgi));
    cgi.cgiPath = "/bin/sh";
    cgi.argp = argv;
    cgi.envp = envp;
    cgi.stdIn = in;
    cgi.stdOut = out;
    cgi.handle = launchCgi(&cgi, "/");
    ttrue(cgi.handle > 0, "the CGI starts");

    helper = 0;
    for (i = 0; i < 100 && cgi.handle > 0 && (helper = readPid(pidFile)) == 0; i++) {
        usleep(50 * 1000);
    }
    ttrue(helper > 0, "the CGI starts its helper");

    if (cgi.handle > 0) {
        teqi(getpgid(cgi.handle), (int) cgi.handle, "the CGI leads its own process group");
        killCgi(&cgi);
    }
    if (helper > 0) {
        for (i = 0; i < 40 && running(helper); i++) {
            usleep(50 * 1000);
        }
        tfalse(running(helper), "the helper does not outlive the aborted CGI");
        kill(helper, SIGKILL);
    }
    unlink(in);
    unlink(out);
    unlink(pidFile);
    wfree(in);
    wfree(out);
    wfree(pidFile);
    wfree(script);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && ME_UNIX_LIKE
    websOsOpen();
    websRuntimeOpen();
    killGroup();
#else
    tskip("The CGI process group applies to the Unix launcher");
#endif
    return 0;
}
