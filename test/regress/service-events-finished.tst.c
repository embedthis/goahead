/*
    service-events-finished.tst.c - websServiceEvents honors a finished flag set before it is called
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

int main(int argc, char **argv)
{
    int finished;

    teq(websOpen("../web", NULL), 0, "initialize the HTTP engine");
    finished = 1;
#if ME_UNIX_LIKE
    //  A loop that ignores the flag is killed here and the test fails
    alarm(5);
#endif
    websServiceEvents(&finished);
#if ME_UNIX_LIKE
    alarm(0);
#endif
    teq(finished, 1, "websServiceEvents returns with the flag still set");
    websClose();
    return 0;
}
