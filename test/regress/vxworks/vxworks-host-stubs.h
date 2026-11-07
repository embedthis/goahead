/*
    vxworks-host-stubs.h - Host stand-ins for the VxWorks kernel calls that the socket, HTTP and OS layers make.

    The CGI backend's stand-ins live in test/regress/vxworks/vxWorks.h, which this header includes. Force-include
    this header (-include) when compiling a library source in the VxWorks configuration on a POSIX host:

        -include vxworks-host-stubs.h -DME_OS_TYPE=16 -DVXWORKS=1 -D_WRS_VXWORKS_MAJOR=7 -D_WRS_VXWORKS_MINOR=0
        -I test/regress/vxworks

    A fixture that runs the compiled code defines the functions; a syntax check needs only the declarations.
 */
#ifndef _VXWORKS_HOST_STUBS_H
#define _VXWORKS_HOST_STUBS_H

#include "vxWorks.h"

/* ioLib.h: FIONBIO takes a pointer to an int flag */
#define FIONBIO 0x5421
#define ioctl   vxIoctl
int vxIoctl(int fd, int function, int *arg);

/* hostLib.h: returns the IPv4 address in network order, or ERROR */
int hostGetByName(char *name);

/* iosLib.h: a device header, and the lookup that splits a path into device and tail */
typedef struct DEV_HDR {
    char *name;
} DEV_HDR;
DEV_HDR *iosDevFind(char *name, const char **pNameTail);

#endif /* _VXWORKS_HOST_STUBS_H */
