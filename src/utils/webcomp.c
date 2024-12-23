/*
    webcomp -- Compile web pages into C source

    Usage: webcomp --strip strip filelist >webrom.c
    Where:
        filelist is a file containing the pathnames of all web pages
        strip is a path prefix to remove from all the web page pathnames
        webrom.c is the resulting C source file to compile and link.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/**************************** Forward Declarations ****************************/

static int  compile(char *fileList, char *strip);
static void usage();

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
            if (argind >= argc) usage();
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
        output.c is the resulting C source file to compile and link.\n");
    exit(2);
}


static int compile(char *fileList, char *strip)
{
    WebsStat        sbuf;
    WebsTime        now;
    FILE            *lp;
    char            buf[512], file[ME_GOAHEAD_LIMIT_FILENAME], *cp, *sl;
    uchar           *p;
    ssize           len;
    int             j, i, fd, nFile;

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
        Open each input file and compile each web page
     */
    nFile = 0;
    while (fgets(file, sizeof(file), lp) != NULL) {
        if ((p = (uchar*) strchr(file, '\n')) || (p = (uchar*) strchr(file, '\r'))) {
            *p = '\0';
        }
        if (*file == '\0') {
            continue;
        }
        if (stat(file, &sbuf) == 0 && sbuf.st_mode & S_IFDIR) {
            continue;
        }
        if ((fd = open(file, O_RDONLY | O_BINARY, 0644)) < 0) {
            fprintf(stderr, "Cannot open file %s\n", file);
            return -1;
        }
        fprintf(stdout, "/* %s */\n", file);
        fprintf(stdout, "static uchar p%d[] = {\n", nFile);

        while ((len = read(fd, buf, sizeof(buf))) > 0) {
            p = (uchar*)buf;
            for (i = 0; i < len; ) {
                fprintf(stdout, "\t");
                for (j = 0; p< (uchar*)(&buf[len]) && j < 16; j++, p++) {
                    fprintf(stdout, "%4d,", *p);
                }
                i += j;
                fprintf(stdout, "\n");
            }
        }
        fprintf(stdout, "\t   0\n};\n\n");
        close(fd);
        nFile++;
    }
    fclose(lp);

    /*
        Output the page index
     */
    fprintf(stdout, "WebsRomIndex websRomIndex[] = {\n");

    if ((lp = fopen(fileList, "r")) == NULL) {
        fprintf(stderr, "Cannot open file list %s\n", fileList);
        return -1;
    }
    nFile = 0;
    while (fgets(file, sizeof(file), lp) != NULL) {
        if ((p = (uchar*) strchr(file, '\n')) || (p = (uchar *) strchr(file, '\r'))) {
            *p = '\0';
        }
        if (*file == '\0') {
            continue;
        }
        /*
            Remove the prefix and add a leading "/" when we print the path
         */
        if (strncmp(file, strip, strlen(strip)) == 0) {
            cp = &file[strlen(strip)];
        } else {
            cp = file;
        }
        while ((sl = strchr(file, '\\')) != NULL) {
            *sl = '/';
        }
        if (*cp == '/') {
            cp++;
        }

        if (stat(file, &sbuf) == 0 && sbuf.st_mode & S_IFDIR) {
            fprintf(stdout, "\t{ \"/%s\", 0, 0 },\n", cp);
            continue;
        }
        fprintf(stdout, "\t{ \"/%s\", p%d, %d },\n", cp, nFile, (int) sbuf.st_size);
        nFile++;
    }
    fclose(lp);
    fprintf(stdout, "\t{ 0, 0, 0 }\n");
    fprintf(stdout, "};\n");
    fprintf(stdout, "#else\n");
    fprintf(stdout, "WebsRomIndex websRomIndex[] = {\n");
    fprintf(stdout, "\t{ 0, 0, 0 }\n};\n");
    fprintf(stdout, "#endif\n");
    fflush(stdout);
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
