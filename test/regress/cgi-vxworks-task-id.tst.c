/* VxWorks CGI task IDs must not be narrowed to int. */

#include "testme.h"
#include "goahead.h"

/********************************* Suite A: the defect shape ********************/

static void suiteA_defectShape(void)
{
    void    *taskId;
    int     narrowed;
    ssize   high;

    tinfo("\n--- Suite A: a pointer task ID does not fit in int ---\n");

    if (sizeof(void*) <= sizeof(int)) {
        tskip("Host pointers fit in int, so narrowing cannot be shown here");
        return;
    }
    /* A task control block address above 4 GB, as a 64-bit kernel allocates */
    high = ((ssize) 0x7f00 << 32) | 0x12345678;
    taskId = (void*) high;

    narrowed = (int) (ssize) taskId;
    tfalse((void*) (ssize) narrowed == taskId, "A1 narrowing a 64-bit task ID to int must lose its upper bits");
    ttrue((void*) (ssize) taskId == taskId, "A2 a pointer-sized handle must preserve the task ID");
    tfalse((void*) (ssize) narrowed == (void*) (ssize) -1,
           "A3 the narrowed task ID is not the spawn failure sentinel, so the handler accepts it");
}

/********************************* Suite B: the source invariant ****************/

#define VX_69_GUARD \
    "#elif VXWORKS && (_WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 9))"

/*
    Find src/ by walking upward from the runner's directory; return NULL if absent.
 */
static cchar *findSourceDir(void)
{
    static cchar *roots[] = { "../src", "../../src", "src", "../../../src", 0 };
    FILE         *fp;
    char         path[ME_GOAHEAD_LIMIT_FILENAME];
    int          i;

    for (i = 0; roots[i]; i++) {
        fmt(path, sizeof(path), "%s/cgi.c", roots[i]);
        if ((fp = fopen(path, "rb")) != 0) {
            fclose(fp);
            return roots[i];
        }
    }
    return 0;
}

static char *readSource(cchar *dir, cchar *name)
{
    FILE  *fp;
    char  path[ME_GOAHEAD_LIMIT_FILENAME], *buf;
    ssize len;

    fmt(path, sizeof(path), "%s/%s", dir, name);
    if ((fp = fopen(path, "rb")) == 0) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    len = (ssize) ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = walloc(len + 1);
    len = (ssize) fread(buf, 1, (size_t) len, fp);
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

/*
    Return true if a line declares an int list that names the given identifier.
 */
static bool declaresInt(cchar *line, cchar *name)
{
    cchar  *cp, *end;
    size_t len;

    while (*line == ' ') {
        line++;
    }
    if (strncmp(line, "int ", 4) != 0) {
        return 0;
    }
    len = slen(name);
    end = strchr(line, ';');
    for (cp = line + 4; (cp = strstr(cp, name)) != 0 && (!end || cp < end); cp += len) {
        if ((cp[-1] == ' ' || cp[-1] == ',') && (cp[len] == ',' || cp[len] == ';')) {
            return 1;
        }
    }
    return 0;
}


static bool declares(cchar *text, cchar *type, cchar *name)
{
    cchar  *cp, *np;
    size_t len;

    len = slen(type);
    for (cp = text; (cp = strstr(cp, type)) != 0; cp += len) {
        for (np = cp + len; *np == ' '; np++) { }
        if (np > cp + len && sstarts(np, name) && np[slen(name)] == ';') {
            return 1;
        }
    }
    return 0;
}

static void suiteB_sourceInvariant(void)
{
    cchar *dir;
    char  *buf, *section, *sectionEnd, *line, *next, *decl, *guard;
    int   intTaskIds;

    tinfo("\n--- Suite B: cgi.c keeps VxWorks task IDs pointer sized ---\n");

    if ((dir = findSourceDir()) == 0) {
        tskip("Source tree not present beside the test directory");
        return;
    }
    if ((buf = readSource(dir, "cgi.c")) == 0) {
        ttrue(0, "B0 %s/cgi.c must be readable", dir);
        return;
    }
    decl = strstr(buf, "typedef TASK_ID CgiPid;");
    tnotnull(decl, "B1 CgiPid must be TASK_ID on VxWorks");
    if (decl) {
        guard = decl;
        while (guard > buf && strncmp(guard, "#elif ", 6) != 0) {
            guard--;
        }
        ttrue(strncmp(guard, VX_69_GUARD, sizeof(VX_69_GUARD) - 1) == 0,
              "B1a the TASK_ID CgiPid must be guarded for VxWorks 6.9 and later");
    }

    section = strstr(buf, "\n#if VXWORKS\n#if _WRS_VXWORKS_MAJOR");
    sectionEnd = section ? strstr(section, "#endif /* VXWORKS */") : 0;
    tnotnull(section, "B2 cgi.c must have a VxWorks launcher section");
    tnotnull(sectionEnd, "B3 the VxWorks launcher section must be terminated");
    if (section && sectionEnd) {
        *sectionEnd = '\0';
        ttrue(declares(section, "CgiPid", "rc"), "B4 launchCgi must hold the taskSpawn result in a CgiPid");
        ttrue(declares(section, "CgiPid", "taskId"), "B5 vxWebsCgiEntry must hold taskIdSelf() in a CgiPid");
        tnull(strstr(section, "rc = fd = -1"), "B6 the spawn result must not share an int initializer with fd");

        intTaskIds = 0;
        for (line = section; line && *line; line = next) {
            if ((next = strchr(line, '\n')) != 0) {
                *next++ = '\0';
            }
            if (declaresInt(line, "rc") || declaresInt(line, "taskId") || declaresInt(line, "handle")) {
                tinfo("int task ID declaration: %s\n", line);
                intTaskIds++;
            }
        }
        teqi(intTaskIds, 0, "B7 no task ID in the VxWorks section may be declared int");
    }
    wfreeset(buf);
}

/********************************* Main *****************************************/

int main(int argc, char **argv)
{
    tinfo("cgi-vxworks-task-id: regression - VxWorks CGI task IDs must not be narrowed to int\n");

    suiteA_defectShape();
    suiteB_sourceInvariant();

    return 0;
}
