/*
    updater -- Main program

    update --host Domain --token Token --product ProductID --device DeviceID --version 1.2.3 \
        key=value key=value ...
 */

/********************************** Includes **********************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "updater.h"

/********************************** Locals ************************************/

#define IMAGE_PATH  "update.bin"
#define SERVER_PORT 443
#define BUFFER_SIZE 4096

static cchar *cmd, *device, *file, *host, *product, *token, *version;
static char  *properties;
static int   verbose = 0;

/********************************** Forwards **********************************/

static int parseArgs(int argc, char **argv);

/************************************ Code ************************************/

static int usage(void)
{
    fprintf(stderr, "usage: update [options] [key=value,...]\n"
            "--cmd script        # Script to invoke to apply the update\n"
            "--device ID         # Unique device ID\n"
            "--file image/path   # Path to save the downloaded update\n"
            "--host host.domain  # Device cloud endpoint from the Builder cloud edit panel\n"
            "--product ProductID # ProductID from the Buidler token list\n"
            "--token TokenID     # CloudAPI access token from the Builder token list\n"
            "--version SemVer    # Current device firmware version\n"
            "--verbose           # Trace execution\n"
            "key:value,...       # Device-specific properties for the distribution policy\n");
    exit(2);
}

int main(int argc, char **argv)
{
    int rc;

    if (parseArgs(argc, argv) < 0) {
        return -1;
    }
    if (!host || !product || !token || !device || !version) {
        usage();
    }
    rc = update(host, product, token, device, version, properties, file, cmd, verbose);
    free(properties);
    return rc;
}

static int parseArgs(int argc, char **argv)
{
    char *argp, *key, *value, pbuf[BUFFER_SIZE];
    int  count, mark, nextArg;

    file = IMAGE_PATH;
    for (nextArg = 1; nextArg < argc; nextArg++) {
        argp = argv[nextArg];
        if (*argp != '-') {
            break;
        }
        if (strcmp(argp, "--cmd") == 0) {
            if (nextArg >= argc) {
                usage();
            }
            cmd = argv[++nextArg];

        } else if (strcmp(argp, "--file") == 0) {
            if (nextArg >= argc) {
                usage();
            }
            file = argv[++nextArg];

        } else if (strcmp(argp, "--host") == 0) {
            if (nextArg >= argc) {
                usage();
            }
            host = argv[++nextArg];

        } else if (strcmp(argp, "--product") == 0) {
            if (nextArg >= argc) {
                usage();
            }
            product = argv[++nextArg];

        } else if (strcmp(argp, "--token") == 0) {
            if (nextArg >= argc) {
                usage();
            }
            token = argv[++nextArg];

        } else if (strcmp(argp, "--device") == 0) {
            if (nextArg >= argc) {
                usage();
            }
            device = argv[++nextArg];

        } else if (strcmp(argp, "--version") == 0) {
            if (nextArg >= argc) {
                usage();
            }
            version = argv[++nextArg];

        } else if (strcmp(argp, "--verbose") == 0 || strcmp(argp, "-v") == 0) {
            verbose = 1;

        } else {
            usage();
        }
    }
    if (argc > nextArg) {
        for (mark = 0; nextArg < argc; nextArg++) {
            if ((key = strtok_r(strdup(argv[nextArg]), "=", &value)) == NULL) {
                usage();
            }
            count = snprintf(&pbuf[mark], sizeof(pbuf) - mark - 1, "\"%s\":\"%s\",", key, value);
            if (count < 0 || mark + count >= BUFFER_SIZE) {
                fprintf(stderr, "Parameter buffer overflow - arguments too long");
                usage();
            }
            mark += count;
            if (mark >= BUFFER_SIZE) {
                fprintf(stderr, "Too many parameters");
                usage();
            }
            free(key);
        }
        if (*pbuf) {
            pbuf[strlen(pbuf) - 1] = '\0';
        }
        properties = strdup(pbuf);
    }
    return 0;
}

