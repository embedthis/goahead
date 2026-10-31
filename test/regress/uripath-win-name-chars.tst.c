/*
    Reject Win32 name characters before filesystem lookup on Windows to prevent wildcard aliasing.
    Accept those characters on other platforms, except colons, which are always refused.
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

static bool accepted(cchar *uri)
{
    char *path;
    bool ok;

    path = websValidateUriPath(uri);
    ok = path != 0;
    wfree(path);
    return ok;
}

int main(int argc, char **argv)
{
    /* Encoded, as websValidUriChars refuses the raw character; ? is the query delimiter and only arrives encoded */
    cchar *invalid[] = {
        "/sub%3E/", "/sub%3E/index.html", "/a%3C.exe", "/cgi-bin/a%3C.exe", "/x%22y.html", "/a%7Cb", "/a%2A",
        "/a%3F.html", "/dir%3E/sub/file.txt", 0
    };
    /* Legal on every platform: a space that is not trailing, quotes the URI grammar allows, and a lone ~ */
    cchar *legal[] = {
        "/index.html", "/My%20Documents/index.html", "/a'b(c).html", "/~user/index.html", "/sub/a,b;c.html", 0
    };
    int i;

    for (i = 0; legal[i]; i++) {
        ttrue(accepted(legal[i]), "%s is a legal name on every platform and must be accepted", legal[i]);
    }
    for (i = 0; invalid[i]; i++) {
#if ME_WIN_LIKE
        ttrue(!accepted(invalid[i]), "%s holds a character Win32 refuses in a file name and must be refused",
              invalid[i]);
#else
        ttrue(accepted(invalid[i]), "%s is an ordinary file name off Windows and must be accepted", invalid[i]);
#endif
    }
    /* The colon is refused everywhere already (drive letters and streams); it must stay so */
    ttrue(!accepted("/a%3Ab"), "a colon must stay refused on every platform");
    return 0;
}
