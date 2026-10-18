/* A repeated Content-Type in a multipart file part does not leak */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#include "heap-fixture.h"

#if ME_GOAHEAD_UPLOAD && HEAP_MEASURABLE
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/upload.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define WARMUP      200
#define ITERATIONS  20000
#define RUNS        2

/*
    Allow allocator growth while detecting the larger per-operation leak.
 */
#define MAX_GROWTH  (160 * 1024)

static void repeat(Webs *wp, int count)
{
    char *line;
    int  i;

    for (i = 0; i < count; i++) {
        line = sclone(i & 1 ? "Content-Type: text/plain" : "Content-Type: application/octet-stream");
        processUploadHeader(wp, line);
        wfree(line);
    }
}

static void campaign(void *arg)
{
    repeat((Webs*) arg, ITERATIONS);
}

int main(int argc, char **argv)
{
    Webs  *wp;
    ssize growth;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->upfd = -1;
    wp->clientFilename = sclone("part.txt");
    wp->currentFile = walloc(sizeof(WebsUpload));
    memset(wp->currentFile, 0, sizeof(WebsUpload));

    repeat(wp, WARMUP);
    growth = heapGrowth(campaign, wp, RUNS);
    ttrue(growth < MAX_GROWTH, "repeated Content-Type lines must not grow the heap: grew at least %zd bytes per %d",
          growth, ITERATIONS);
    tmatch(wp->currentFile->contentType, "text/plain", "the part keeps the last Content-Type");

    freeUploadFile(wp->currentFile);
    wfree(wp->clientFilename);
    wfree(wp);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Uploads are not in this build, or the heap in use cannot be measured here");
    return 0;
}
#endif
