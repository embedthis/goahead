/*
    url-parse-fragment.tst.c - websUrlParse splits the fragment before the query, per RFC 3986
 */

#include "testme.h"
#include "goahead.h"

static void parses(cchar *url, cchar *path, cchar *query, cchar *reference)
{
    char *buf, *ppath, *pquery, *preference;
    int  rc;

    buf = ppath = pquery = preference = 0;
    rc = websUrlParse(url, &buf, NULL, NULL, NULL, &ppath, NULL, &preference, &pquery);
    teq(rc, 0, "websUrlParse(\"%s\") must succeed", url);
    ttrue(smatch(ppath, path), "\"%s\" must have path \"%s\", got \"%s\"", url, path, ppath ? ppath : "null");
    ttrue(query ? smatch(pquery, query) : pquery == 0, "\"%s\" must have query \"%s\", got \"%s\"", url,
          query ? query : "null", pquery ? pquery : "null");
    ttrue(reference ? smatch(preference, reference) : preference == 0, "\"%s\" must have reference \"%s\", got \"%s\"",
          url, reference ? reference : "null", preference ? preference : "null");
    wfree(buf);
}


int main(int argc, char **argv)
{
    parses("/p?a=1#b", "/p", "a=1", "b");
    parses("/p#?q=1", "/p", NULL, "?q=1");
    parses("/p#a?b=1", "/p", NULL, "a?b=1");
    parses("/p?a=1", "/p", "a=1", NULL);
    parses("http://example.com/p?a=1#b", "/p", "a=1", "b");
    return 0;
}
