/*
    log-path-colon.tst.c - The log level suffix is the last colon followed by digits, so a Windows drive letter or a
    colon inside the file name stays part of the path.
 */
#include "testme.h"
#include "goahead.h"

int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetPath("C:\\logs\\goahead.log:3");
    tmatch(logGetPath(), "C:\\logs\\goahead.log", "a drive-letter path keeps its drive");
    teq(websGetLogLevel(), 3, "the level follows the last colon");

    logSetPath("stderr:2");
    tmatch(logGetPath(), "stderr", "the stream name is the path");
    teq(websGetLogLevel(), 2, "the level is read");

    logSetPath("with:colon.log");
    tmatch(logGetPath(), "with:colon.log", "a colon followed by a name is part of the path");
    teq(websGetLogLevel(), 2, "a path without a level leaves the level alone");

    logSetPath("C:\\logs\\goahead.log");
    tmatch(logGetPath(), "C:\\logs\\goahead.log", "a drive-letter path without a level is kept whole");

    logSetPath("plain.log:0");
    tmatch(logGetPath(), "plain.log", "a plain path with a level");
    teq(websGetLogLevel(), 0, "level zero is read");
#else
    tskip("Logging is not enabled");
#endif
    return 0;
}
