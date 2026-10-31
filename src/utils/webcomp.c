/*
    webcomp -- Compile web pages into C source

    Usage: webcomp --strip strip filelist >webrom.c
    Where:
        filelist is a file containing the pathnames of all web pages
        strip is a path prefix to remove from all the web page pathnames
        webrom.c is the resulting C source file to compile and link.
    Each page's index entry records its source file's modified time and a 64-bit FNV-1a hash of its content, from
    which the server derives the page's Last-Modified and ETag.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/**************************** Forward Declarations ****************************/

static int    compile(char *fileList, char *strip);
static void   usage();

/*********************************** Code *************************************/

int main(int argc, char* argv[])
{
    char    *argp, *fileList, *strip;
    int     argind;

    fileList = NULL;
    strip = "";

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;
        } else if (strcmp(argp, "--prefix") == 0 || strcmp(argp, "--strip") == 0) {
            if (argind + 1 >= argc) usage();
            strip = argv[++argind];
        }
    }
    if (argind >= argc) {
        usage();
    }
    fileList = argv[argind];
    if (compile(fileList, strip) < 0) {
        return -1;
    }
    return 0;
}


static void usage()
{
    fprintf(stdout, "usage: webcomp [--strip strip] filelist >output.c\n\
        --strip specifies is a path prefix to remove from all the web page pathnames\n\
        filelist is a file containing the pathnames of all web pages\n\
        output.c is the resulting C source file to compile and link.\n\
        Each page is indexed with its source file's modified time and a hash of its content.\n");
    exit(2);
}


typedef struct Page {
    char   *path;                   /* Index path with the prefix removed and separators normalized */
    ssize  size;                    /* Bytes emitted into the page array */
    time_t mtime;                   /* Source file modified time */
    uint64 hash;                    /* FNV-1a of the emitted bytes */
    int    index;                   /* Page array number, or -1 for a directory */
} Page;


/*
    FNV-1a step. The multiply wraps by design.
 */
static ME_NO_SANITIZE_WRAP uint64 hashByte(uint64 hash, uchar c)
{
    return (hash ^ c) * 0x100000001b3ULL;
}


static void printPath(cchar *path)
{
    for (; *path; path++) {
        if (*path == '"' || *path == '\\') {
            fprintf(stdout, "\\%c", *path);
        } else if ((uchar) *path < 0x20 || (uchar) *path >= 0x7f) {
            fprintf(stdout, "\\%03o", (uchar) *path);
        } else {
            fputc(*path, stdout);
        }
    }
}


static int compile(char *fileList, char *strip)
{
    WebsStat sbuf;
    time_t   now;
    FILE     *lp;
    Page     *pages, *pp;
    char     buf[512], file[ME_GOAHEAD_LIMIT_FILENAME], *cp, *sl;
    uchar    *p;
    ssize    len, size;
    uint64   hash;
    int      j, i, fd, nFile, nPages, maxPages;

    if ((lp = fopen(fileList, "r")) == NULL) {
        fprintf(stderr, "Cannot open file list %s\n", fileList);
        return -1;
    }
    time(&now);
    fprintf(stdout, "/*\n   rom.c \n");
    fprintf(stdout, "   Compiled by webcomp: %s */\n\n", ctime(&now));
    fprintf(stdout, "#include \"goahead.h\"\n\n");
    fprintf(stdout, "#if ME_ROM\n\n");

    /*
        Record size and hash from the emitted bytes so the index remains valid if the source file changes.
     */
    nFile = nPages = maxPages = 0;
    pages = NULL;
    while (fgets(file, sizeof(file), lp) != NULL) {
        if ((p = (uchar*) strchr(file, '\n')) || (p = (uchar*) strchr(file, '\r'))) {
            *p = '\0';
        }
        if (*file == '\0') {
            continue;
        }
        if (stat(file, &sbuf) < 0) {
            fprintf(stderr, "Cannot stat file %s\n", file);
            goto fail;
        }
        if (nPages >= maxPages) {
            maxPages += 64;
            pages = realloc(pages, maxPages * sizeof(Page));
        }
        pp = &pages[nPages++];
        memset(pp, 0, sizeof(Page));
        pp->index = -1;
        pp->mtime = sbuf.st_mtime;
        if (!(sbuf.st_mode & S_IFDIR)) {
            if ((fd = open(file, O_RDONLY | O_BINARY, 0644)) < 0) {
                fprintf(stderr, "Cannot open file %s\n", file);
                goto fail;
            }
            fprintf(stdout, "/* page %d */\n", nFile);
            fprintf(stdout, "static uchar p%d[] = {\n", nFile);
            hash = 0xcbf29ce484222325ULL;
            size = 0;
            while ((len = read(fd, buf, sizeof(buf))) > 0) {
                p = (uchar*) buf;
                for (i = 0; i < len; ) {
                    fprintf(stdout, "\t");
                    for (j = 0; p < (uchar*) (&buf[len]) && j < 16; j++, p++) {
                        fprintf(stdout, "%4d,", *p);
                        hash = hashByte(hash, *p);
                    }
                    i += j;
                    fprintf(stdout, "\n");
                }
                size += len;
            }
            fprintf(stdout, "\t   0\n};\n\n");
            close(fd);
            pp->index = nFile++;
            pp->size = size;
            pp->hash = hash;
        }

        cp = (strncmp(file, strip, strlen(strip)) == 0) ? &file[strlen(strip)] : file;
        while ((sl = strchr(cp, '\\')) != NULL) {
            *sl = '/';
        }
        if (*cp == '/') {
            cp++;
        }
        pp->path = strdup(cp);
    }
    fclose(lp);

    /*
        Output the page index
     */
    fprintf(stdout, "WebsRomIndex websRomIndex[] = {\n");
    for (i = 0; i < nPages; i++) {
        pp = &pages[i];
        fprintf(stdout, "\t{ \"/");
        printPath(pp->path);
        if (pp->index < 0) {
            fprintf(stdout, "\", 0, 0, 0, 0 },\n");
        } else {
            fprintf(stdout, "\", p%d, %d, %lld, 0x%016llxULL },\n", pp->index, (int) pp->size,
                    (long long) pp->mtime, (unsigned long long) pp->hash);
        }
        free(pp->path);
    }
    free(pages);
    fprintf(stdout, "\t{ 0, 0, 0, 0, 0 }\n");
    fprintf(stdout, "};\n");
    fprintf(stdout, "#else\n");
    fprintf(stdout, "WebsRomIndex websRomIndex[] = {\n");
    fprintf(stdout, "\t{ 0, 0, 0, 0, 0 }\n};\n");
    fprintf(stdout, "#endif\n");
    fflush(stdout);
    return 0;

fail:
    for (i = 0; i < nPages; i++) {
        free(pages[i].path);
    }
    free(pages);
    fclose(lp);
    return -1;
}


/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
