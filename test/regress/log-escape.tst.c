/*
    log-escape.tst.c - Request data cannot forge an error or trace log line
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

#if ME_GOAHEAD_LOGGING
static char captured[4096];
static int  capturedCount;

static void captureLog(int flags, cchar *msg)
{
    scopy(captured, sizeof(captured), msg);
    capturedCount++;
}


static bool hasControl(cchar *str, cchar *keep)
{
    cuchar *cp;

    for (cp = (cuchar*) str; *cp; cp++) {
        if ((*cp < 0x20 || *cp == 0x7f) && !(keep && strchr(keep, *cp))) {
            return 1;
        }
    }
    return 0;
}
#endif


int main(int argc, char **argv)
{
    char *escaped;

    escaped = websEscapeControl("a\nb\rc\td\x1b[2Je\x7f", NULL, NULL);
    tmatch(escaped, "a\\x0ab\\x0dc\\x09d\\x1b[2Je\\x7f", "every control byte must be written as \\xHH");
    wfree(escaped);

    escaped = websEscapeControl("a\nb\x1b", "\n", "\"");
    tmatch(escaped, "a\nb\\x1b", "control bytes in keep must be left as they are");
    wfree(escaped);

    escaped = websEscapeControl("say \"hi\"", NULL, "\"");
    tmatch(escaped, "say \\x22hi\\x22", "characters in escape must be written as \\xHH");
    wfree(escaped);

    escaped = websEscapeControl(NULL, NULL, NULL);
    tmatch(escaped, "", "a null string must yield an empty string");
    wfree(escaped);

#if ME_GOAHEAD_LOGGING
    logSetHandler(captureLog);
    websSetLogLevel(5);

    capturedCount = 0;
    tfalse(websValidUriChars("/a\ngoahead:\t0:\tFORGED\r\x1b[2J"), "a URI with control bytes must be rejected");
    teq(capturedCount, 1, "a bad URI must log exactly one message");
    tfalse(hasControl(captured, NULL), "no raw control byte from the URI may reach the log handler");
    tmatch(captured, "Bad character 0x0a in URI at offset 2", "only the offset and byte value are logged");
    ttrue(strstr(captured, "FORGED") == 0, "the request target must not be logged");

    capturedCount = 0;
    error("user %s", "admin\nFORGED");
    teq(capturedCount, 1, "error must log one message");
    tmatch(captured, "user admin\\x0aFORGED", "error arguments must be escaped");

    logmsgProc(1, "user %s", "admin\r\x1b[31m");
    tmatch(captured, "user admin\\x0d\\x1b[31m", "logmsg arguments must be escaped");

    traceProc(1, "user %s", "admin\x7f");
    tmatch(captured, "user admin\\x7f", "trace arguments must be escaped");

    traceProc(1 | WEBS_RAW_MSG, "Header: %s\r\n", "value\x1b[2J");
    tmatch(captured, "Header: value\\x1b[2J\r\n", "raw messages must keep line breaks but escape other controls");
#endif
    return 0;
}
