/*
    temp-file-staging.tst.c - Regression: request data must not be staged where a client can read it.

    websTempFile named the CGI stdin and stdout files, the upload staging file, the PUT staging file
    and the auth file rewrite. It named them from a static counter that started at zero and reset on
    every restart, and it created nothing - the caller opened the name with O_CREAT some time later.
    Two consequences, and on VxWorks they compounded into a disclosure.

      1) The next name was known to everyone. A client that fetched "cgi-7.tmp" got another client's
         CGI response, and a local user who created that path first owned the file the server was
         about to write through.
      2) The VxWorks branch defaulted the directory to ".". cgiHandler chdir()s into the route's
         documents directory before spawning, so "." was the served tree: the staged response was
         fetchable over HTTP under a name the client could count to.

    The remedy is that websTempFile creates the file itself, with 64 bits from the system CSPRNG and
    an exclusive create that refuses a symlink, and that no platform defaults the directory to a
    relative path. Callers open an existing private file rather than creating one.

    Four suites:

      A) names are unpredictable and never repeat
      B) the file exists on return, is private, empty, and cannot be created a second time
      C) the default directory is absolute, so it is never the working directory the CGI runs in
      D) websCheckTempDirs refuses a staging directory inside the documents directory

    Suite D is the part that keeps an operator from re-creating the VxWorks configuration by hand:
    websOpen fails rather than serving one client's request data to another.
 */

#include "testme.h"
#include "goahead.h"

#define COUNT 64

/********************************* Suite A: unpredictable names *****************/

/*
    The name is prefix-HHHHHHHHHHHHHHHH.tmp with 16 lower-case hex digits. The counter it replaced
    produced prefix-0.tmp, prefix-1.tmp and so on, so a decimal suffix is the defect's signature.
 */
static int hasRandomSuffix(cchar *path)
{
    cchar *cp;
    int   digits;

    if ((cp = strrchr(path, '-')) == 0) {
        return 0;
    }
    for (digits = 0, cp++; *cp && *cp != '.'; cp++, digits++) {
        if (!((*cp >= '0' && *cp <= '9') || (*cp >= 'a' && *cp <= 'f'))) {
            return 0;
        }
    }
    return digits == 16 && smatch(cp, ".tmp");
}

static void suiteA_unpredictable(void)
{
    char *paths[COUNT];
    int  i, j, unique;

    tinfo("\n--- Suite A: temp file names are unpredictable ---\n");

    for (i = 0; i < COUNT; i++) {
        paths[i] = websTempFile(NULL, "reg");
        tnotnull(paths[i], "A1 websTempFile must return a name (iteration %d)", i);
        if (paths[i] == 0) {
            return;
        }
        ttrue(hasRandomSuffix(paths[i]), "A2 name must carry a random hex suffix, not a counter: %s",
              paths[i]);
    }
    /*
        A counter passes a uniqueness test too. A2 above is what separates the two; this catches a
        random source that is stuck or seeded per-process.
     */
    unique = 1;
    for (i = 0; i < COUNT && unique; i++) {
        for (j = i + 1; j < COUNT; j++) {
            if (smatch(paths[i], paths[j])) {
                unique = 0;
                break;
            }
        }
    }
    ttrue(unique, "A3 names must not repeat across %d calls", COUNT);

    for (i = 0; i < COUNT; i++) {
        unlink(paths[i]);
        wfreeset(paths[i]);
    }
}

/********************************* Suite B: created, private, exclusive *********/

static void suiteB_created(void)
{
    WebsStat sbuf;
    char     *path;
    int      fd;

    tinfo("\n--- Suite B: the file exists, is private, and the create was exclusive ---\n");

    path = websTempFile(NULL, "reg");
    tnotnull(path, "B1 websTempFile must return a name");
    if (path == 0) {
        return;
    }
    ttrue(stat(path, &sbuf) == 0, "B2 the file must exist when the name is returned: %s", path);
    ttrue(S_ISREG(sbuf.st_mode), "B3 the staging file must be a regular file");
    ttrue(sbuf.st_size == 0, "B4 the staging file must start empty");
#if ME_UNIX_LIKE
    /*
        The file holds another client's request body or CGI response until the request completes, so
        it must not be readable by other users on the device.
     */
    ttrue((sbuf.st_mode & 0777) == 0600, "B5 the staging file must be private, found mode 0%o",
          (int) (sbuf.st_mode & 0777));
#endif
    /*
        The name is only safe because the file behind it is already claimed. If an exclusive create
        of the same path succeeds, websTempFile handed out a name it had not taken.
     */
    fd = open(path, O_CREAT | O_EXCL | O_RDWR | O_BINARY, 0600);
    ttrue(fd < 0, "B6 the returned path must already be claimed by an exclusive create");
    if (fd >= 0) {
        close(fd);
    }
    /*
        The caller's contract: open the existing file rather than create it.
     */
    fd = open(path, O_RDWR | O_BINARY | O_NOFOLLOW, 0600);
    ttrue(fd >= 0, "B7 the caller must be able to open the staging file");
    if (fd >= 0) {
        close(fd);
    }
    unlink(path);
    wfreeset(path);
}

/********************************* Suite C: never the working directory *********/

static void suiteC_absolute(void)
{
    char *path;
    int  absolute;

    tinfo("\n--- Suite C: the default staging directory is absolute ---\n");

    path = websTempFile(NULL, "reg");
    tnotnull(path, "C1 websTempFile must return a name");
    if (path == 0) {
        return;
    }
    /*
        cgiHandler runs the request with the working directory set to the route's documents
        directory. A relative default therefore stages the response inside the served tree - the
        VxWorks defect - so the default must resolve nowhere near the working directory.
     */
    absolute = (path[0] == '/' || path[0] == '\\');
#if ME_WIN_LIKE
    if (isalpha((uchar) path[0]) && path[1] == ':') {
        absolute = 1;
    }
#endif
    ttrue(absolute, "C2 the default staging directory must be absolute, got: %s", path);
    ttrue(!sstarts(path, "./") && !sstarts(path, ".\\"), "C3 the default must not be the working "
          "directory, got: %s", path);
    unlink(path);
    wfreeset(path);
}

/********************************* Suite D: the configuration check *************/

static void suiteD_checkTempDirs(void)
{
    char *saved;

    tinfo("\n--- Suite D: a staging directory inside the documents directory is refused ---\n");

    saved = websGetDocuments() ? sclone(websGetDocuments()) : 0;

    /*
        The documents directory is the staging directory. This is the shape of the VxWorks defect and
        of any hand configuration that serves the temp directory, and websOpen must not start.
     */
    websSetDocuments(ME_GOAHEAD_TEMP_DIR);
    ttrue(websCheckTempDirs() > 0, "D1 staging inside the documents directory must be reported");

    /*
        A neighbour that merely shares a prefix is not inside it. Without this the check would refuse
        sound configurations, and a check that cries wolf gets configured away.
     */
    websSetDocuments(ME_GOAHEAD_TEMP_DIR "dir");
    ttrue(websCheckTempDirs() == 0, "D2 a directory that only shares a prefix must be accepted");

    websSetDocuments(ME_GOAHEAD_TEMP_DIR "/served");
    ttrue(websCheckTempDirs() == 0, "D3 a documents directory inside the staging directory is not "
          "the reported case - the staging file is still outside the served tree");

    if (saved) {
        websSetDocuments(saved);
        wfreeset(saved);
    }
}

/********************************* Main *****************************************/

int main(int argc, char **argv)
{
    tinfo("temp-file-staging: regression - request data must not be staged where a client can read it\n");

    logOpen();
    suiteA_unpredictable();
    suiteB_created();
    suiteC_absolute();
    suiteD_checkTempDirs();
    logClose();

    return 0;
}
