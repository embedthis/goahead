/* websParseCookie must tolerate empty cookie segments. */

#include "testme.h"
#include "goahead.h"

static Webs *allocWebs(cchar *cookie)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->cookie = sclone(cookie);
    return wp;
}


static void freeWebs(Webs *wp)
{
    wfree(wp->cookie);
    wfree(wp);
}


/*
    Seed the stack with nonzero bytes to expose uninitialized pointers.
 */
static void __attribute__((noinline)) poisonStack(void)
{
    volatile char pad[4096];

    memset((char*) pad, 0xAA, sizeof(pad));
}


static char *parse(cchar *cookie, cchar *name)
{
    Webs *wp;
    char *value;

    wp = allocWebs(cookie);
    poisonStack();
    value = websParseCookie(wp, (char*) name);
    freeWebs(wp);
    return value;
}


/********************************* Suite A: empty segments **********************/

static void suiteA_emptySegments(void)
{
    cchar *headers[] = { ";", ";;", " ;", "; ", ";  ;", "; a=b", " ; ;x=1", "=", ";=", NULL };
    char  *value;
    int   i;

    tinfo("\n--- Suite A: empty segments are skipped ---\n");

    for (i = 0; headers[i]; i++) {
        value = parse(headers[i], "-goahead-session-");
        ttrue(value == 0, "an absent cookie must not be found in a header with empty segments");
        wfree(value);
    }
}


/********************************* Suite B: lookup still works ******************/

static void suiteB_lookup(void)
{
    char *value;

    tinfo("\n--- Suite B: named cookies are still found ---\n");

    value = parse("; sid=abc", "sid");
    ttrue(value && smatch(value, "abc"), "a cookie after an empty segment must be found");
    wfree(value);

    value = parse("a=1;; sid=abc ;", "sid");
    ttrue(value && smatch(value, "abc"), "a cookie between empty segments must be found");
    wfree(value);

    value = parse("sid=\"quoted\";", "sid");
    ttrue(value && smatch(value, "quoted"), "a quoted cookie before an empty segment must be found");
    wfree(value);
}


int main(int argc, char **argv)
{
    tinfo("cookie-empty-segment: websParseCookie must tolerate empty cookie segments\n");

    suiteA_emptySegments();
    suiteB_lookup();

    return 0;
}
