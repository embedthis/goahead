/*
    socket.c -- Sockets layer

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "goahead.h"

/************************************ Locals **********************************/

PUBLIC WebsSocket   **socketList;           /* List of open sockets */
PUBLIC int          socketMax;              /* Maximum size of socket */
PUBLIC Socket       socketHighestFd = -1;   /* Highest socket fd opened */
PUBLIC int          socketOpenCount = 0;    /* Number of task using sockets */

static int          hasIPv6;                /* System supports IPv6 */

/***************************** Forward Declarations ***************************/

static int ipv6(cchar *ip);
static void socketAccept(WebsSocket *sp);
static void socketDoEvent(WebsSocket *sp);

/*********************************** Code *************************************/

PUBLIC int socketOpen(void)
{
    Socket  fd;

    if (++socketOpenCount > 1) {
        return 0;
    }

#if ME_WIN_LIKE
{
    WSADATA     wsaData;
    if (WSAStartup(MAKEWORD(1,1), &wsaData) != 0) {
        return -1;
    }
    if (wsaData.wVersion != MAKEWORD(1,1)) {
        WSACleanup();
        return -1;
    }
}
#endif
    socketList = NULL;
    socketMax = 0;
    socketHighestFd = -1;
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
    int     i;

    if (--socketOpenCount <= 0) {
        for (i = socketMax; i >= 0; i--) {
            if (socketList && socketList[i]) {
                socketCloseConnection(i);
            }
        }
        socketOpenCount = 0;
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
#if defined(SO_REUSEPORT) && KEEP
    /*
        This permits multiple servers listening on the same endpoint
     */
    if (setsockopt(sp->sock, SOL_SOCKET, SO_REUSEPORT, (char*) &enable, sizeof(enable)) != 0) {
        error("Cannot set reuseport, errno %d", errno);
    }
#endif
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


#if KEEP
PUBLIC int socketConnect(char *ip, int port, int flags)
{
    WebsSocket              *sp;
    struct sockaddr_storage addr;
    socklen_t               addrlen;
    int                     family, protocol, sid, rc;

    if (port > SOCKET_PORT_MAX) {
        return -1;
    }
    if ((sid = socketAlloc(ip, port, NULL, flags)) < 0) {
        return -1;
    }
    sp = socketList[sid];
    assert(sp);

    if (socketInfo(ip, port, &family, &protocol, &addr, &addrlen) < 0) {
        return -1;
    }
    if ((sp->sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        socketFree(sid);
        return -1;
    }
    socketHighestFd = max(socketHighestFd, sp->sock);

#if ME_COMPILER_HAS_FCNTL
    fcntl(sp->sock, F_SETFD, FD_CLOEXEC);
#endif

    /*
        Connect to the remote server in blocking mode, then go into non-blocking mode if desired.
     */
    if (!(sp->flags & SOCKET_BLOCK)) {
#if ME_WIN_LIKE
        /*
            Set to non-blocking for an async connect
         */
        int flag = 1;
        if (ioctlsocket(sp->sock, FIONBIO, &flag) == SOCKET_ERROR) {
            socketFree(sid);
            return -1;
        }
        sp->flags |= SOCKET_ASYNC;
#else
        socketSetBlock(sid, 1);
#endif
    }
    if ((rc = connect(sp->sock, (struct sockaddr*) &addr, sizeof(addr))) < 0 &&
        (rc = tryAlternateConnect(sp->sock, (struct sockaddr*) &addr)) < 0) {
#if ME_WIN_LIKE
        if (socketGetError(sid) != EWOULDBLOCK) {
            socketFree(sid);
            return -1;
        }
#else
        socketFree(sid);
        return -1;

#endif
    }
    socketSetBlock(sid, (flags & SOCKET_BLOCK));
    return sid;
}


/*
    If the connection failed, swap the first two bytes in the sockaddr structure.  This is a kludge due to a change in
    VxWorks between versions 5.3 and 5.4, but we want the product to run on either.
 */
static int tryAlternateConnect(int sock, struct sockaddr *sockaddr)
{
#if VXWORKS || TIDSP
    char *ptr;

    ptr = (char*) sockaddr;
    *ptr = *(ptr+1);
    *(ptr+1) = 0;
    return connect(sock, sockaddr, sizeof(struct sockaddr));
#else
    return -1;
#endif
}
#endif


PUBLIC void socketCloseConnection(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    socketFree(sid);
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
    size_t                  len;
    char                    ipbuf[1024];
    int                     port, nid;

    assert(sp);

    /*
        Accept the connection and prevent inheriting by children (F_SETFD)
     */
    len = sizeof(addrStorage);
    addr = (struct sockaddr*) &addrStorage;
    if ((newSock = accept(sp->sock, addr, (Socklen*) &len)) == SOCKET_ERROR) {
        return;
    }
#if ME_COMPILER_HAS_FCNTL
    fcntl(newSock, F_SETFD, FD_CLOEXEC);
#endif
    socketHighestFd = max(socketHighestFd, newSock);

    /*
        Create a socket structure and insert into the socket list
     */
    if ((nid = socketAlloc(sp->ip, sp->port, sp->accept, sp->flags)) < 0) {
        return;
    }
    if ((nsp = socketList[nid]) == 0) {
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
        Call the user accept callback. The user must call socketCreateHandler to register for further events of interest.
     */
    if (sp->accept != NULL) {
        /* Get the remote client address */
        socketAddress(addr, (int) len, ipbuf, sizeof(ipbuf), &port);
        if ((sp->accept)(nid, ipbuf, port, sp->sid) < 0) {
            socketFree(nid);
        }
    }
}


PUBLIC void socketRegisterInterest(int sid, int handlerMask)
{
    WebsSocket  *sp;

    assert(socketPtr(sid));
    sp = socketPtr(sid);
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
    struct timeval  tv;
    WebsSocket      *sp;
    fd_set          readFds, writeFds, exceptFds;
    int             nEvents;
    int             all, socketHighestFd;   /* Highest socket fd opened */

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    socketHighestFd = -1;

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
        if (! all) {
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
        Wait for the event or a timeout
     */
    nEvents = select(socketHighestFd + 1, &readFds, &writeFds, &exceptFds, &tv);

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
        if (! all) {
            break;
        }
    }
    return nEvents;
}

#else /* !ME_WIN_LIKE */


PUBLIC int socketSelect(int sid, int timeout)
{
    WebsSocket      *sp;
    struct timeval  tv;
    fd_mask         *readFds, *writeFds, *exceptFds;
    ssize           bit, index;
    int             all, len, nwords, nEvents;

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
        bit = ((ssize) 1) << (sp->sock % (NBBY * sizeof(fd_mask)));
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
        if (! all) {
            break;
        }
    }
    /*
        Wait for the event or a timeout
     */
    nEvents = select(socketHighestFd + 1, (fd_set *) readFds, (fd_set *) writeFds, (fd_set *) exceptFds, &tv);
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
        bit = ((ssize) 1) << (sp->sock % (NBBY * sizeof(fd_mask)));

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
        if (! all) {
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
    WebsSocket  *sp;
    int         sid;

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
    int     sid;

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
    WebsSocket  *sp;
    int         oldBlock;

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
        setsockopt((SOCKET)sp->sock, SOL_SOCKET, SO_BLOCKING, &on, sizeof(on));
#else
        fcntl(sp->sock, F_SETFL, fcntl(sp->sock, F_GETFL) & ~O_NONBLOCK);
#endif

    } else {
#if ME_WIN_LIKE
        ulong flag = !on;
        int rc = ioctlsocket(sp->sock, FIONBIO, &flag);
        rc = rc;
#elif ECOS
        int on = 1;
        ioctl(sp->sock, FIONBIO, &on);
#elif VXWORKS
        int iflag = !on;
        ioctl(sp->sock, FIONBIO, &iflag);
#elif TIDSP
        setsockopt((SOCKET)sp->sock, SOL_SOCKET, SO_BLOCKING, &on, sizeof(on));
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
    Set the TCP delay behavior (nagle algorithm)
 */
PUBLIC int socketSetNoDelay(int sid, bool on)
{
    WebsSocket  *sp;
    int         oldDelay;

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
        BOOL    noDelay;
        noDelay = on ? 1 : 0;
        setsockopt(sp->sock, IPPROTO_TCP, TCP_NODELAY, (FAR char*) &noDelay, sizeof(BOOL));
    }
#else
    {
        int     noDelay;
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
    WebsSocket  *sp;
    ssize       len, written, sofar;
    int         errCode;

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
    WebsSocket  *sp;
    ssize       bytes;
    int         errCode;

    assert(buf);
    assert(bufsize > 0);

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    if (sp->flags & SOCKET_EOF) {
        return -1;
    }
    if ((bytes = recv(sp->sock, buf, (int) bufsize, 0)) < 0) {
        errCode = socketGetError(sid);
        if (errCode == EAGAIN || errCode == EWOULDBLOCK) {
            bytes = 0;
        } else {
            /* Conn reset or Some other error */
            sp->flags |= SOCKET_EOF;
            bytes = -errCode;
        }

    } else if (bytes == 0) {
        sp->flags |= SOCKET_EOF;
        bytes = -1;
    }
    return bytes;
}


/*
    Return true if EOF
 */
PUBLIC bool socketEof(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return 1;
    }
    return sp->flags & SOCKET_EOF;
}


PUBLIC void socketReservice(int sid)
{
    WebsSocket    *sp;

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
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    sp->handler = handler;
    sp->handler_data = data;
    socketRegisterInterest(sid, handlerMask);
}


PUBLIC void socketDeleteHandler(int sid)
{
    WebsSocket    *sp;

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
    WebsSocket  *sp;
    int         sid;

    if (socketMax >= FD_SETSIZE) {
        return -1;
    }
    if ((sid = wallocObject(&socketList, &socketMax, sizeof(WebsSocket))) < 0) {
        return -1;
    }
    sp = socketList[sid];
    sp->sid = sid;
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
    WebsSocket  *sp;
    char        buf[256];
    int         i;

    if ((sp = socketPtr(sid)) == NULL) {
        return;
    }
    /*
        To close a socket, remove any registered interests, set it to non-blocking so that the recv which follows won't
        block, do a shutdown on it so peers on the other end will receive a FIN, then read any data not yet retrieved
        from the receive buffer, and finally close it.  If these steps are not all performed RESETs may be sent to the
        other end causing problems.
     */
    socketRegisterInterest(sid, 0);
    if (sp->sock >= 0) {
        socketSetBlock(sid, 0);
        while (recv(sp->sock, buf, sizeof(buf), 0) > 0) {}
        if (shutdown(sp->sock, SHUT_RDWR) >= 0) {
            while (recv(sp->sock, buf, sizeof(buf), 0) > 0) {}
        }
        closesocket(sp->sock);
    }
    wfree(sp->ip);
    wfree(sp);
    socketMax = wfreeHandle(&socketList, sid);
    /*
        Calculate the new highest socket number
     */
    socketHighestFd = -1;
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
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        return -1;
    }
    return sp->sock;
}


/*
    Get blocking mode
 */
PUBLIC int socketGetBlock(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    return (sp->flags & SOCKET_BLOCK);
}


PUBLIC int socketGetMode(int sid)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return 0;
    }
    return sp->flags;
}


PUBLIC void socketSetMode(int sid, int mode)
{
    WebsSocket    *sp;

    if ((sp = socketPtr(sid)) == NULL) {
        assert(0);
        return;
    }
    sp->flags = mode;
}


PUBLIC int socketGetPort(int sid)
{
    WebsSocket    *sp;

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
    struct addrinfo     hints, *res, *r;
    char                portBuf[16];
    int                 v6;

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
    if (getaddrinfo(ip, portBuf, &hints, &res) != 0) {
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
    struct sockaddr_in  sa;

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
            VxWorks only supports one interface and this code only supports IPv4
         */
        sa.sin_addr.s_addr = (ulong) hostGetByName((char*) ip);
        if (sa.sin_addr.s_addr < 0) {
            assert(0);
            return 0;
        }
#else
        struct hostent *hostent;
        hostent = gethostbyname2(ip, AF_INET);
        if (hostent == 0) {
            hostent = gethostbyname2(ip, AF_INET6);
            if (hostent == 0) {
                return -1;
            }
        }
        memcpy((char*) &sa.sin_addr, (char*) hostent->h_addr_list[0], (ssize) hostent->h_length);
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
    char    service[NI_MAXSERV];

#if ME_WIN_LIKE || defined(IN6_IS_ADDR_V4MAPPED)
    if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*) addr;
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
        return -1;
    }
    if (port) {
        *port = atoi(service);
    }

#else
    struct sockaddr_in  *sa;

#if HAVE_NTOA_R
    sa = (struct sockaddr_in*) addr;
    inet_ntoa_r(sa->sin_addr, ip, ipLen);
#else
    uchar   *cp;
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
    char  *cp;
    int     colons;

    if (ip == 0 || *ip == 0) {
        /*
            Listening on just a bare port means IPv4 only.
         */
        return 0;
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
    char    *ip, *cp;

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
                if (*ip == '\0') {
                    ip = 0;
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
            if (*ip == '*' || *ip == '\0') {
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
    if (pip) {
        *pip = ip;
    } else if (ip) {
        wfree(ip);
    }
    return 0;
}


PUBLIC bool socketIsV6(int sid)
{
    WebsSocket    *sp;

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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
