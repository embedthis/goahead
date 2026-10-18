/* "|" separates alternative abilities, and an empty ability set is no requirement */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING
static char messages[4096];

static void captureLog(int level, cchar *msg)
{
    if (slen(messages) + slen(msg) + 2 < (ssize) sizeof(messages)) {
        strcat(messages, msg);
        strcat(messages, "\n");
    }
}


static WebsHash abilitySet(cchar *first, cchar *second)
{
    WebsHash abilities;

    abilities = hashCreate(-1);
    hashEnter(abilities, first, valueInteger(0), 0);
    if (second) {
        hashEnter(abilities, second, valueInteger(0), 0);
    }
    return abilities;
}


static bool canWith(cchar *first, cchar *second)
{
    Webs     *wp;
    WebsHash abilities;
    bool     result;
    int      wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->username = sclone("viewer");
    wp->user = websLookupUser("viewer");
    abilities = abilitySet(first, second);
    result = websCan(wp, abilities);
    hashFree(abilities);
    websFree(wp);
    return result;
}


static void suiteA_alternatives(void)
{
    tinfo("\n--- Suite A: alternatives need one ability, a list needs all ---\n");

    ttrue(canWith("view|purchase", NULL), "A1 view satisfies view|purchase");
    ttrue(canWith("purchase|view", NULL), "A2 view satisfies purchase|view, the last alternative");
    ttrue(!canWith("purchase|manage", NULL), "A3 view does not satisfy purchase|manage");
    ttrue(!canWith("view", "purchase"), "A4 view alone does not satisfy view and purchase");
    ttrue(canWith("view", NULL), "A5 view satisfies view");
}


static bool service(Webs *wp)
{
    return 1;
}


static void suiteB_emptySet(void)
{
    WebsRoute      *gate, *all;
    WebsLogHandler prior;

    tinfo("\n--- Suite B: an empty ability set is no requirement ---\n");

    websCloseRoute();
    ttrue(websOpenRoute() == 0, "B0 open an empty route table");
    websDefineHandler("file", 0, service, 0, 0);
    gate = websAddRoute("/admin/", "file", -1);
    all = websAddRoute("/", "file", -1);
    ttrue(gate && all, "B0 add the routes");
    teq(websSetRouteMatch(gate, 0, 0, -1, -1, hashCreate(-1), -1), 0, "B1 an empty ability set is accepted");
    ttrue(gate->abilities < 0, "B2 an empty ability set leaves the route with no requirement");

    messages[0] = '\0';
    prior = logSetHandler(captureLog);
    websCheckRoutes();
    logSetHandler(prior);
    ttrue(strstr(messages, "no authenticated routes") != 0,
          "B3 a table of that route and a catch-all has no authenticated route");
}


int main(int argc, char **argv)
{
    teq(websOpen("../web", NULL), 0, "open the server");
    ttrue(websAddUser("viewer", "unused", "view") != 0, "add a user holding only view");
    websComputeAllUserAbilities();

    suiteA_alternatives();
    suiteB_emptySet();

    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication or logging is not enabled");
    return 0;
}
#endif
