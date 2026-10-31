/*
    args-bound.tst.c -- jsArgs reads no more than argc arguments, whether or not argv is NULL terminated.
 */
#include "testme.h"
#include "goahead.h"
#include "js.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_JAVASCRIPT
    char *args[] = { "one", "two", "beyond", NULL };
    char *a, *b, *c;
    int  count;

    a = b = c = NULL;
    count = jsArgs(2, args, "%s %s %s", &a, &b, &c);
    teqi(count, 2, "jsArgs must stop at argc");
    tmatch(a, "one", "first argument");
    tmatch(b, "two", "second argument");
    tnull(c, "an entry beyond argc must not be read");

    count = jsArgs(0, args, "%s", &a);
    teqi(count, 0, "no arguments are read when argc is zero");
#else
    tskip("JavaScript is not enabled");
#endif
    return 0;
}
