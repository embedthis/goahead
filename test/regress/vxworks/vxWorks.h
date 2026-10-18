/*
    vxWorks.h - Host stand-in for the VxWorks kernel API used by the GoAhead VxWorks backends
 */
#ifndef _VXWORKS_STUB_H
#define _VXWORKS_STUB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

typedef int STATUS;
typedef int BOOL;
#define OK      0
#define ERROR   (-1)
#ifndef O_BINARY
    #define O_BINARY 0
#endif

typedef struct windTcb *TASK_ID;
typedef intptr_t _Vx_usr_arg_t;
typedef struct moduleS *MODULE_ID;
typedef void *SYMTAB_ID;

typedef struct {
    unsigned mask;
    char     *name;
    void     *value;
} SYMBOL_DESC;

#define SYM_FIND_BY_NAME     1
#define LOAD_GLOBAL_SYMBOLS  4

extern SYMTAB_ID sysSymTbl;

STATUS symFind(SYMTAB_ID symTbl, SYMBOL_DESC *desc);
MODULE_ID loadModule(int fd, int symFlag);
STATUS unldByModuleId(MODULE_ID moduleId, int options);
TASK_ID taskSpawn(char *name, int priority, int options, size_t stackSize, void *entry, _Vx_usr_arg_t arg1,
    _Vx_usr_arg_t arg2, _Vx_usr_arg_t arg3, _Vx_usr_arg_t arg4, _Vx_usr_arg_t arg5, _Vx_usr_arg_t arg6,
    _Vx_usr_arg_t arg7, _Vx_usr_arg_t arg8, _Vx_usr_arg_t arg9, _Vx_usr_arg_t arg10);
TASK_ID taskIdSelf(void);
STATUS taskIdVerify(TASK_ID tid);
char *taskName(TASK_ID tid);
STATUS taskDelete(TASK_ID tid);
STATUS taskLock(void);
STATUS taskUnlock(void);
STATUS taskPriorityGet(TASK_ID tid, int *priority);
void ioTaskStdSet(TASK_ID tid, int stdFd, int newFd);
STATUS envPrivateCreate(TASK_ID tid, int envSource);
STATUS envPrivateDestroy(TASK_ID tid);
int sysClkRateGet(void);

#endif
