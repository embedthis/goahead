/*
    route-load-leak.tst.c - A rejected configuration line releases the selector tables it built

    websLoad builds the methods, extensions, abilities and redirects tables of a route line before it validates the
    line, and websAddRole takes its abilities table by value. When the line is refused (an unknown keyword, an unknown
    handler, a missing uri, a duplicate role) those tables must be released: the hash table store is a handle array
    that is never shrunk, so an embedder that reloads configuration and tolerates the failure leaks one table per
    selector per refused line.

    Invariant: after a refused websLoad, the lowest free hash handle is the same as before it. Handles are allocated
    lowest-first, so a table left behind is observable as a higher handle without reaching into the runtime.
 */
#include "testme.h"
#include "goahead.h"

static WebsHash lowestFreeHandle(void)
{
    WebsHash h;

    h = hashCreate(-1);
    hashFree(h);
    return h;
}

static void writeConfig(cchar *path, cchar *content)
{
    int fd;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ttrue(fd >= 0, "write the configuration file");
    if (fd >= 0) {
        write(fd, content, (int) strlen(content));
        close(fd);
    }
}

static void checkRefusedLoad(cchar *path, cchar *content, cchar *what)
{
    WebsHash before, after;
    int      rc;

    writeConfig(path, content);
    before = lowestFreeHandle();
    rc = websLoad(path);
    after = lowestFreeHandle();
    unlink(path);
    teq(rc, -1, "%s", what);
    teq(after, before, "%s", what);
}

int main(int argc, char **argv)
{
#if ME_GOAHEAD_AUTH
    teq(websOpen("../web", NULL), 0, "initialize the server");

    checkRefusedLoad("route-load-leak-1.txt",
                     "route uri=/leak1/ methods=GET,POST abilities=manage extensions=html redirect=401@/x bogus=1\n",
                     "an unknown route keyword releases the selector tables");
    checkRefusedLoad("route-load-leak-2.txt",
                     "route uri=/leak2/ handler=nosuchhandler methods=GET abilities=manage\n",
                     "an unknown handler releases the selector tables");
    checkRefusedLoad("route-load-leak-3.txt",
                     "route methods=GET,POST abilities=manage\n",
                     "a route without a uri releases the selector tables");

    writeConfig("route-load-leak-4.txt", "role name=leakrole abilities=a,b\n");
    teq(websLoad("route-load-leak-4.txt"), 0, "define the role once");
    unlink("route-load-leak-4.txt");
    checkRefusedLoad("route-load-leak-5.txt",
                     "role name=leakrole abilities=c,d\n",
                     "a duplicate role releases the abilities table it was given");
    checkRefusedLoad("route-load-leak-6.txt",
                     "role abilities=c,d\n",
                     "a role without a name releases the abilities table it was given");
    checkRefusedLoad("route-load-leak-7.txt",
                     "role name=otherrole abilities=c,d bogus=1\n",
                     "an unknown role keyword releases the abilities table");
    websClose();
#else
    tskip("Auth disabled");
#endif
    return 0;
}
