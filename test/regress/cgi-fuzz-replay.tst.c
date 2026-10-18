/*
    cgi-fuzz-replay.tst.c - The seed corpus of the CGI harness passes the ISINDEX, environment and response oracles
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

#if defined(__has_include)
#if __has_include("../sec/cgi-harness.c") && ME_GOAHEAD_CGI && ME_UNIX_LIKE && !VXWORKS
#define HAVE_HARNESS 1
#endif
#endif

#if HAVE_HARNESS
#define CGI_HARNESS_REPLAY 1
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "../sec/cgi-harness.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <dirent.h>

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    The working directory depends on the runner, so probe upwards for the corpus.
 */
static cchar *findCorpus(void)
{
    static cchar *dirs[] = { "sec/corpus/cgi", "test/sec/corpus/cgi", "../sec/corpus/cgi", "../test/sec/corpus/cgi", 0 };
    struct stat  info;
    int          i;

    for (i = 0; dirs[i]; i++) {
        if (stat(dirs[i], &info) == 0 && S_ISDIR(info.st_mode)) {
            return dirs[i];
        }
    }
    return 0;
}

static int replay(cchar *dir)
{
    struct dirent *entry;
    DIR           *dp;
    FILE          *fp;
    char          path[ME_GOAHEAD_LIMIT_FILENAME], *data;
    long          size;
    int           count;

    count = 0;
    if ((dp = opendir(dir)) == 0) {
        return 0;
    }
    while ((entry = readdir(dp)) != 0) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        fmt(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if ((fp = fopen(path, "rb")) == 0) {
            continue;
        }
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        data = malloc((size_t) size + 1);
        if (size > 0 && fread(data, 1, (size_t) size, fp) == (size_t) size) {
            ttrue(cgiOracle(data, (size_t) size), "the oracle must accept the seed %s", entry->d_name);
            count++;
        }
        free(data);
        fclose(fp);
    }
    closedir(dp);
    return count;
}
#endif

int main(int argc, char **argv)
{
#if HAVE_HARNESS
    cchar *dir;
    int   count;

    if ((dir = findCorpus()) == 0) {
        tskip("the CGI harness corpus is not present");
        return 0;
    }
    openEngine();
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    count = replay(dir);
    ttrue(count >= 30, "the corpus must hold the seed inputs, replayed %d", count);
    closeEngine();
#else
    tskip("the CGI harness is not present in this build");
#endif
    return 0;
}
