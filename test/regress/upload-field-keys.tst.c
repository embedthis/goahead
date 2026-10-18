/* Long upload field names retain distinct metadata keys. */
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
#include "../../src/upload.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
int main(int argc, char **argv)
{
#if ME_GOAHEAD_UPLOAD
    Webs *wp;
    WebsUpload file;
    char names[2][100], *key;
    cchar *prefixes[] = { "FILE_CLIENT_FILENAME_", "FILE_CONTENT_TYPE_", "FILE_FILENAME_", "FILE_SIZE_" };
    cchar *values[2][4] = { { "one.txt", "text/plain", "/tmp/one", "12" },
                          { "two.bin", "application/octet-stream", "/tmp/two", "34" } };
    int i, j, wid;
    wid = websAlloc(-1);
    ttrue(wid >= 0, "allocate request");
    wp = webs[wid];
    memset(&file, 0, sizeof(file));
    wp->currentFile = &file;
    for (i = 0; i < 2; i++) {
        memset(names[i], 'a', 90);
        names[i][90] = (char) ('0' + i);
        names[i][91] = '\0';
        wp->uploadVar = names[i];
        file.clientFilename = (char*) values[i][0];
        file.contentType = (char*) values[i][1];
        file.filename = (char*) values[i][2];
        file.size = i ? 34 : 12;
        defineUploadVars(wp);
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 4; j++) {
            key = sfmt("%s%s", prefixes[j], names[i]);
            cchar *value = websGetVar(wp, key, "missing");
            tmatch(value, values[i][j], "full upload field key retains its own metadata");
            wfree(key);
        }
    }
    wp->currentFile = NULL;
    wp->uploadVar = NULL;
    websFree(wp);
#else
    tskip("Upload disabled");
#endif
    return 0;
}
