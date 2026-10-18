/*
    cgi-vxworks-fixture.c - Drive the VxWorks CGI backend on the host against a simulated kernel
 */
#include "../../src/cgi.c"

/********************************** Simulated kernel ***************************/

#define MAX_TCB 16

struct windTcb {
    int  alive;
    char name[64];
};

static struct windTcb tcbs[MAX_TCB], serverTcb;
static int  nextTcb, reuseTcb = -1, failSpawn;
static int  lockDepth, deletes, deletesUnlocked, loads, unloads, moduleSerial, serverPriority;
static int  spawnPriority, spawnFdin, spawnFdout;
static bool imageEntry, moduleEntry, moduleLoaded;
static char spawnName[64];
static int  failures;

SYMTAB_ID sysSymTbl;

static int cgiEntry(int argc, char **argv) { return 0; }

TASK_ID taskSpawn(char *name, int priority, int options, size_t stackSize, void *entry, _Vx_usr_arg_t arg1,
    _Vx_usr_arg_t arg2, _Vx_usr_arg_t arg3, _Vx_usr_arg_t arg4, _Vx_usr_arg_t arg5, _Vx_usr_arg_t arg6,
    _Vx_usr_arg_t arg7, _Vx_usr_arg_t arg8, _Vx_usr_arg_t arg9, _Vx_usr_arg_t arg10)
{
    struct windTcb *tcb;

    if (failSpawn) {
        return (TASK_ID) ERROR;
    }
    tcb = reuseTcb >= 0 ? &tcbs[reuseTcb] : &tcbs[nextTcb++];
    tcb->alive = 1;
    snprintf(tcb->name, sizeof(tcb->name), "%s", name);
    snprintf(spawnName, sizeof(spawnName), "%s", name);
    spawnPriority = priority;
    spawnFdin = (int) arg4;
    spawnFdout = (int) arg5;
    return tcb;
}

TASK_ID taskIdSelf(void) { return &serverTcb; }
STATUS taskIdVerify(TASK_ID tid) { return tid && tid->alive ? OK : ERROR; }
char *taskName(TASK_ID tid) { return tid->name; }
STATUS taskLock(void) { lockDepth++; return OK; }
STATUS taskUnlock(void) { lockDepth--; return OK; }
STATUS taskPriorityGet(TASK_ID tid, int *priority) { *priority = serverPriority; return OK; }
void ioTaskStdSet(TASK_ID tid, int stdFd, int newFd) { }
STATUS envPrivateCreate(TASK_ID tid, int envSource) { return OK; }
STATUS envPrivateDestroy(TASK_ID tid) { return OK; }
int sysClkRateGet(void) { return 60; }

STATUS taskDelete(TASK_ID tid)
{
    deletes++;
    if (lockDepth <= 0) {
        deletesUnlocked++;
    }
    tid->alive = 0;
    return OK;
}

MODULE_ID loadModule(int fd, int symFlag)
{
    loads++;
    moduleLoaded = 1;
    return (MODULE_ID) (intptr_t) ++moduleSerial;
}

STATUS unldByModuleId(MODULE_ID moduleId, int options)
{
    unloads++;
    moduleLoaded = 0;
    return OK;
}

STATUS symFind(SYMTAB_ID symTbl, SYMBOL_DESC *desc)
{
    if (imageEntry || (moduleLoaded && moduleEntry)) {
        desc->value = (void*) cgiEntry;
        return OK;
    }
    return ERROR;
}

/********************************** Helpers ************************************/

#define check(cond, msg) if (!(cond)) { printf("FAILED: %s\n", msg); failures++; }

static char *modulePath;

static void reset(void)
{
    memset(tcbs, 0, sizeof(tcbs));
    nextTcb = 0;
    reuseTcb = -1;
    failSpawn = 0;
    lockDepth = deletes = deletesUnlocked = loads = unloads = 0;
    serverPriority = 100;
    imageEntry = moduleLoaded = 0;
    moduleEntry = 1;
}

/*
    Register and launch a request the way cgiHandler does.
 */
static Cgi *start(int *cidp)
{
    Cgi *cgip;
    int cid;

    cid = wallocObject(&cgiList, &cgiMax, sizeof(Cgi));
    cgip = cgiList[cid];
    cgip->stdIn = websTempFile(NULL, "vxcgi");
    cgip->stdOut = websTempFile(NULL, "vxcgi");
    cgip->cgiPath = sclone(modulePath);
    cgip->fdin = cgip->fdout = -1;
    cgip->handle = launchCgi(cgip, ".");
    *cidp = cid;
    return cgip;
}

/*
    Release a request the way websCgiPoll and websCgiDetach do. cgip->stdIn is borrowed from the request.
 */
static void finish(Cgi *cgip, int cid)
{
    char *stdIn;

    stdIn = cgip->stdIn;
    cgiCleanup(cgip, cid);
    wfree(stdIn);
}

static bool fdOpen(int fd)
{
    return fd >= 0 && fcntl(fd, F_GETFD) != -1;
}

/********************************** Scenarios **********************************/

static void moduleLifetime(void)
{
    Cgi *a, *b, *c;
    int ca, cb, cc;

    reset();
    a = start(&ca);
    check(a->handle != (CgiPid) -1, "the first request spawns a task");
    check(loads == 1, "the first request loads the module");
    b = start(&cb);
    check(b->handle != (CgiPid) -1, "an overlapping request spawns a task");
    check(loads == 1, "an overlapping request reuses the loaded module");
    check(unloads == 0, "no request unloads a module a running task executes");

    a->handle->alive = 0;
    check(checkCgi(a) == 0, "an exited task is reported gone");
    finish(a, ca);
    check(unloads == 0, "the module stays loaded while another task runs it");
    check(checkCgi(b) != 0, "the other task is still running");

    b->handle->alive = 0;
    check(checkCgi(b) == 0, "the last task is reported gone");
    finish(b, cb);
    check(unloads == 1, "the module is unloaded when its last task has gone");

    c = start(&cc);
    check(loads == 2, "the next request loads the current module file");
    c->handle->alive = 0;
    finish(c, cc);
    check(unloads == 2, "that module is unloaded in turn");
    check(cgiModuleMax == 0 || cgiModules[0] == NULL, "no module record is left behind");
    printf("module lifetime passed\n");
}

static void taskIdentity(void)
{
    Cgi *a, *b;
    int ca, cb;

    reset();
    a = start(&ca);
    check(strcmp(spawnName, "vxcgi_cgientry") != 0, "the task is not named after the entry symbol");
    b = start(&cb);
    check(strcmp(a->handle->name, b->handle->name) != 0, "each CGI task has a unique name");

    /* The first task exits and an unrelated task is created with the same ID */
    a->handle->alive = 1;
    snprintf(a->handle->name, sizeof(a->handle->name), "tShell0");
    check(checkCgi(a) == 0, "a reused task ID is not mistaken for the CGI task");
    killCgi(a);
    check(deletes == 0, "aborting the CGI does not delete the unrelated task with its ID");
    check(a->handle->alive, "the unrelated task keeps running");
    finish(a, ca);

    killCgi(b);
    check(deletes == 1, "aborting a running CGI deletes its task");
    check(deletesUnlocked == 0, "the task is checked and deleted with the scheduler locked");
    check(lockDepth == 0, "every scheduler lock is released");
    finish(b, cb);
    printf("task identity passed\n");
}

static void abortedStdio(void)
{
    Cgi *a;
    int ca, fdin, fdout;

    reset();
    a = start(&ca);
    fdin = a->fdin;
    fdout = a->fdout;
    check(fdOpen(fdin) && fdOpen(fdout), "the server opens the task's stdio");
    check(spawnFdin == fdin && spawnFdout == fdout, "the task is given the server's descriptors");
    killCgi(a);
    finish(a, ca);
    check(!fdOpen(fdin) && !fdOpen(fdout), "an aborted task's stdio is closed");
    printf("aborted stdio passed\n");
}

static void priority(void)
{
    Cgi *a;
    int ca;

    reset();
    a = start(&ca);
    check(spawnPriority == 101, "the CGI task runs one step below the server");
    a->handle->alive = 0;
    finish(a, ca);

    reset();
    serverPriority = 255;
    a = start(&ca);
    check(spawnPriority == 255, "the CGI task priority is clamped to the lowest priority");
    a->handle->alive = 0;
    finish(a, ca);
    printf("priority passed\n");
}

static void imageEntryPoint(void)
{
    Cgi *a;
    int ca;

    reset();
    imageEntry = 1;
    a = start(&ca);
    check(a->handle != (CgiPid) -1, "an entry point linked into the image is spawned");
    check(loads == 0, "an entry point in the image loads no module");
    a->handle->alive = 0;
    finish(a, ca);
    check(unloads == 0, "an entry point in the image is never unloaded");
    printf("image entry passed\n");
}

static void launchFailure(void)
{
    Cgi *a;
    int ca, before;

    reset();
    moduleEntry = 0;
    a = start(&ca);
    check(a->handle == (CgiPid) -1, "a module without the entry point does not launch");
    check(loads == 1 && unloads == 1, "a module that cannot be used is unloaded");
    a->handle = 0;
    finish(a, ca);

    reset();
    failSpawn = 1;
    before = dup(0);
    close(before);
    a = start(&ca);
    check(a->handle == (CgiPid) -1, "a failed spawn is reported");
    check(unloads == 1, "the module loaded for a failed spawn is unloaded");
    a->handle = 0;
    finish(a, ca);
    check(dup(0) == before, "a failed spawn leaves no descriptor open");
    close(before);
    printf("launch failure passed\n");
}

int main(int argc, char **argv)
{
    FILE *fp;
    char *dir;

    websOsOpen();
    websRuntimeOpen();
    /* The entry symbol, vxcgi_cgientry, is named from the module basename */
    dir = sfmt("%s/vxcgi-%d", websGetTempDir(), (int) getpid());
    mkdir(dir, 0700);
    modulePath = sfmt("%s/vxcgi.o", dir);
    if ((fp = fopen(modulePath, "w")) != NULL) {
        fputs("object module", fp);
        fclose(fp);
    }
    moduleLifetime();
    taskIdentity();
    abortedStdio();
    priority();
    imageEntryPoint();
    launchFailure();
    unlink(modulePath);
    rmdir(dir);
    return failures ? 1 : 0;
}
