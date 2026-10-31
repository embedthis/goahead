/*
    event-runs-once.tst.c - A due event runs once; only a callback that reschedules it runs again
 */
#include "testme.h"
#include "goahead.h"

static int onceCount, againCount, againId;

static void runsOnce(void *arg, int id)
{
    onceCount++;
}


static void runsAgain(void *arg, int id)
{
    againCount++;
    if (againCount == 1) {
        websRestartEvent(id, 0);
    }
}


int main(int argc, char **argv)
{
    teq(websOpen("../web", NULL), 0, "open the server");
    ttrue(websStartEvent(0, runsOnce, 0) >= 0, "schedule an event that does not reschedule itself");
    againId = websStartEvent(0, runsAgain, 0);
    ttrue(againId >= 0, "schedule an event that reschedules itself once");
#if ME_UNIX_LIKE
    //  A dispatcher that re-runs a fired event never returns and is killed here
    alarm(5);
#endif
    websRunEvents();
    websRunEvents();
#if ME_UNIX_LIKE
    alarm(0);
#endif
    teq(onceCount, 1, "an event that does not reschedule itself runs exactly once");
    teq(againCount, 2, "an event that reschedules itself once runs twice");
    websClose();
    return 0;
}
