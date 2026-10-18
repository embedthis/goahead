/* HTTP date output uses fixed English tokens and explicit GMT. */
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
    WebsFileInfo info;
    char *date;
    memset(&info, 0, sizeof(info));
    info.mtime = 0;
    date = websGetDateString(&info);
    tmatch(date, "Thu, 01 Jan 1970 00:00:00 GMT", "epoch HTTP date");
    wfree(date);
    info.mtime = 951827696;
    date = websGetDateString(&info);
    tmatch(date, "Tue, 29 Feb 2000 12:34:56 GMT", "leap-day HTTP date");
    wfree(date);
    date = websGetDateString(NULL);
    tnotnull(date, "current HTTP date");
    if (date) {
        teq(strlen(date), 29, "fixed date width");
        tmatch(date + 25, " GMT", "explicit GMT timezone");
    }
    wfree(date);
    return 0;
}
