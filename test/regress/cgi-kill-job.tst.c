/*
    cgi-kill-job.tst.c - Aborting a Windows CGI ends the programs it started and releases its staging files

    Windows sibling of cgi-kill-group.tst.c. killCgi called TerminateProcess on the
    CGI alone and did not wait for it: a helper the CGI had started kept running, and the staging files the
    CGI still held open could not be deleted by the cleanup that follows.
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

#if ME_GOAHEAD_CGI && WINDOWS
#include "../../src/cgi.c"

static bool running(DWORD pid)
{
    HANDLE h;
    DWORD  code;
    bool   alive;

    if ((h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) == NULL) {
        return 0;
    }
    alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
}


static DWORD readPid(cchar *path)
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
    return (DWORD) pid;
}


/*
    Print what the CGI wrote and whether it is still running, so a helper that never starts says why
 */
static void showCgi(Cgi *cgi, cchar *out)
{
    FILE   *fp;
    char   buf[2048];
    size_t len;
    DWORD  code;

    code = 0;
    if (cgi->handle != (CgiPid) -1 && GetExitCodeProcess(cgi->handle, &code)) {
        tinfo("CGI %s, exit code %lu\n", code == STILL_ACTIVE ? "still running" : "exited", (ulong) code);
    }
    if ((fp = fopen(out, "r")) != 0) {
        len = fread(buf, 1, sizeof(buf) - 1, fp);
        buf[len] = '\0';
        fclose(fp);
        tinfo("CGI output:\n%s\n", buf);
    }
}


/*
    The CGI role: start a helper that outlives this process, publish its pid, then wait to be killed.
    Publish by rename so the test never reads a half-written file. Errors go to stdout, which showCgi reports.
 */
static int runCgi(cchar *pidFile)
{
    STARTUPINFOA        si;
    PROCESS_INFORMATION pi;
    FILE                *fp;
    char                self[MAX_PATH], cmd[MAX_PATH + 16], tmp[MAX_PATH + 8];

    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(cmd, sizeof(cmd), "\"%s\" --helper", self);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        printf("Cannot start the helper, error %lu\n", (ulong) GetLastError());
        return 1;
    }
    snprintf(tmp, sizeof(tmp), "%s.tmp", pidFile);
    if ((fp = fopen(tmp, "w")) == 0) {
        printf("Cannot create %s\n", tmp);
        return 1;
    }
    fprintf(fp, "%lu\n", (ulong) pi.dwProcessId);
    fclose(fp);
    if (!MoveFileExA(tmp, pidFile, MOVEFILE_REPLACE_EXISTING)) {
        printf("Cannot rename %s, error %lu\n", tmp, (ulong) GetLastError());
        return 1;
    }
    Sleep(60000);
    return 0;
}


static void killJob(void)
{
    Cgi   cgi;
    char  *in, *out, *pidFile, *self, *argv[4], *envp[8], *value, path[MAX_PATH];
    cchar *names[] = { "SystemRoot", "PATH", "TEMP", "TMP", NULL };
    DWORD helper;
    int   i, n;

    tinfo("\n--- A CGI that starts a helper and is aborted ---\n");

    in = websTempFile(NULL, "cgi");
    out = websTempFile(NULL, "cgi");
    pidFile = websTempFile(NULL, "cgipid");
    ttrue(in && out && pidFile, "create the staging files");
    unlink(pidFile);

    //  This test program is both the CGI and its helper
    GetModuleFileNameA(NULL, path, sizeof(path));
    self = sclone(path);
    argv[0] = self;
    argv[1] = "--cgi";
    argv[2] = pidFile;
    argv[3] = NULL;
    //  PATH locates the goahead library this program links
    for (i = 0, n = 0; names[i]; i++) {
        if ((value = getenv(names[i])) != 0) {
            envp[n++] = sfmt("%s=%s", names[i], value);
        }
    }
    envp[n] = NULL;

    memset(&cgi, 0, sizeof(cgi));
    cgi.cgiPath = self;
    cgi.argp = argv;
    cgi.envp = envp;
    cgi.stdIn = in;
    cgi.stdOut = out;
    cgi.handle = launchCgi(&cgi, "C:\\");
    ttrue(cgi.handle != (CgiPid) -1, "the CGI starts");

    helper = 0;
    for (i = 0; i < 600 && cgi.handle != (CgiPid) -1 && (helper = readPid(pidFile)) == 0; i++) {
        Sleep(50);
    }
    if (helper == 0) {
        showCgi(&cgi, out);
    }
    ttrue(helper > 0, "the CGI starts its helper");

    if (cgi.handle != (CgiPid) -1) {
        killCgi(&cgi);
        /*
            The staging file was open in the CGI; after killCgi returns it must be deletable at once.
         */
        teqi(unlink(out), 0, "the stdout staging file is released when killCgi returns");
    }
    if (helper > 0) {
        for (i = 0; i < 40 && running(helper); i++) {
            Sleep(50);
        }
        tfalse(running(helper), "the helper does not outlive the aborted CGI");
    }
    unlink(in);
    unlink(out);
    unlink(pidFile);
    wfree(in);
    wfree(out);
    wfree(pidFile);
    wfree(self);
    for (i = 0; i < n; i++) {
        wfree(envp[i]);
    }
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_CGI && WINDOWS
    if (argc > 1 && strcmp(argv[1], "--helper") == 0) {
        Sleep(60000);
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "--cgi") == 0) {
        return runCgi(argv[2]);
    }
    websOsOpen();
    websRuntimeOpen();
    killJob();
#else
    tskip("The CGI job applies to the Windows launcher");
#endif
    return 0;
}
