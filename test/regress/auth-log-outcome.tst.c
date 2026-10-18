/*
    auth-log-outcome.tst.c - Authentication outcomes are logged at the default level, not through trace
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

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING

#define MAX_LINES 16

static char lines[MAX_LINES][512];
static int  lineFlags[MAX_LINES];
static int  lineCount;

static void captureLog(int flags, cchar *msg)
{
    if (lineCount < MAX_LINES) {
        scopy(lines[lineCount], sizeof(lines[0]), msg);
        lineFlags[lineCount] = flags;
        lineCount++;
    }
}


static bool acceptPass(Webs *wp)
{
    return smatch(wp->password, "right");
}


static bool hasControl(cchar *str)
{
    cuchar *cp;

    for (cp = (cuchar*) str; *cp; cp++) {
        if (*cp < 0x20 || *cp == 0x7f) {
            return 1;
        }
    }
    return 0;
}


static Webs *allocWebs(WebsRoute *route)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->route = route;
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "192.0.2.77");
    return wp;
}


static void freeWebs(Webs *wp)
{
    wp->route = 0;
    wfree(wp->username);
    wfree(wp->password);
    wfree(wp);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_AUTH && ME_GOAHEAD_LOGGING
    WebsRoute route;
    Webs      *wp;
    char      *longName;

    teq(websOpen("../web", NULL), 0, "initialize the server");
    logSetHandler(captureLog);
    websSetLogLevel(0);

    memset(&route, 0, sizeof(route));
    route.prefix = "/action/login";
    route.prefixLen = 13;
    route.verify = acceptPass;
    wp = allocWebs(&route);

    lineCount = 0;
    ttrue(!websLoginUser(wp, "joshua", "wrong"), "a wrong password must fail the login");
    teq(lineCount, 1, "a failed login must write exactly one line at level 0");
    ttrue(lineFlags[0] & WEBS_LOG_MSG, "the failure must be a log message, not a trace");
    ttrue(!(lineFlags[0] & WEBS_TRACE_MSG), "the failure must not be a trace message");
    tcontains(lines[0], "failed", "the line must say the login failed");
    tcontains(lines[0], "\"joshua\"", "the line must name the claimed user");
    tcontains(lines[0], "192.0.2.77", "the line must name the client address");
    tcontains(lines[0], "/action/login", "the line must name the route");
    tcontains(lines[0], "form", "the line must name the scheme");
    ttrue(strstr(lines[0], "wrong") == 0, "the line must not carry the password");

    lineCount = 0;
    ttrue(websLoginUser(wp, "joshua", "right"), "the right password must log in");
    teq(lineCount, 1, "a successful login must write exactly one line at level 0");
    ttrue(lineFlags[0] & WEBS_LOG_MSG, "the login must be a log message, not a trace");
    tcontains(lines[0], "succeeded", "the line must say the login succeeded");
    tcontains(lines[0], "\"joshua\"", "the line must name the user");
    tcontains(lines[0], "192.0.2.77", "the line must name the client address");
    ttrue(strstr(lines[0], "right") == 0, "the line must not carry the password");

    /* A user name is request data: it is truncated and escaped */
    longName = walloc(300);
    memset(longName, 'u', 299);
    longName[299] = '\0';
    longName[0] = '\n';
    lineCount = 0;
    ttrue(!websLoginUser(wp, longName, "wrong"), "a failed login with a long name");
    teq(lineCount, 1, "a long user name must not split the line");
    ttrue(!hasControl(lines[0]), "a control byte in the user name must be escaped");
    ttrue(strlen(lines[0]) < 200, "a long user name must be truncated");
    wfree(longName);

    freeWebs(wp);
    websClose();
#else
    tskip("Authentication or logging is not compiled in");
#endif
    return 0;
}
