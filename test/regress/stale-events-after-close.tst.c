/*
    stale-events-after-close.tst.c - An event left pending at websClose does not survive into a reopened server

    websClose stops the events the library started (session and nonce pruning, request timeouts, socket linger) but
    websRuntimeClose leaves every other pending event in the callback table: the table is a static in runtime.c that
    websOpen does not reset. An embedder that restarts the server in process (websClose then websOpen) therefore
    reopens with the previous instance's events still scheduled, and websRunEvents runs them with the arguments they
    were given in the closed instance, which may already be freed. The entries also leak when the process exits.

    Invariant: after websClose, no event of the closed server runs in the reopened one, and the reopened server's
    first event takes the same handle as the closed server's first event did.
 */
#include "testme.h"
#include "goahead.h"

static int fired;

static void staleEvent(void *arg, int id)
{
    fired++;
    websStopEvent(id);
}

static void liveEvent(void *arg, int id)
{
}

int main(int argc, char **argv)
{
    int first, second;

    teq(websOpen("../web", NULL), 0, "initialize the server");
    first = websStartEvent(0, staleEvent, 0);
    ttrue(first >= 0, "schedule an event that is never serviced before the close");
    websClose();

    teq(websOpen("../web", NULL), 0, "reopen the server in process");
    second = websStartEvent(60000, liveEvent, 0);
    websRunEvents();
    teq(fired, 0, "an event left pending at websClose must not run after the server is reopened");
    teq(second, first, "events of the closed server must not occupy handles in the reopened one");
    websStopEvent(second);
    websClose();
    return 0;
}
