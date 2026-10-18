/*
    gopass.c -- Authorization password management.

    gopass [--cipher md5|blowfish] [--file filename] [--password password] realm user roles...

    This file provides facilities for creating passwords in a route configuration file.
    It supports either MD5 or Blowfish ciphers.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

#if ME_GOAHEAD_AUTH
/********************************** Locals ************************************/

#define MAX_PASS    64

/********************************* Forwards ***********************************/

static char *getPassword(void);
static void printUsage(void);
static int writeAuthFile(char *path);

#if ME_WIN_LIKE || VXWORKS
static char *getpass(char *prompt);
#endif

/*********************************** Code *************************************/

int main(int argc, char *argv[])
{
    WebsBuf     buf;
    char        *cipher, *password, *authFile, *username, *encodedPassword, *realm, *cp, *roles;
    int         i, errflg, nextArg;

    username = 0;
    errflg = 0;
    password = 0;
    authFile = 0;
    cipher = "blowfish";

    for (i = 1; i < argc && !errflg; i++) {
        if (argv[i][0] != '-') {
            break;
        }
        for (cp = &argv[i][1]; *cp && !errflg; cp++) {
            if (*cp == '-') {
                cp++;
            }
            if (smatch(cp, "cipher")) {
                if (++i == argc) {
                    errflg++;
                } else {
                    cipher = argv[i];
                    if (!smatch(cipher, "md5") && !smatch(cipher, "blowfish")) {
                        error("Unknown cipher \"%s\". Use \"md5\" or \"blowfish\".", cipher);
                    }
                    break;
                }
            } else if (smatch(cp, "file") || smatch(cp, "f")) {
                if (++i == argc) {
                    errflg++;
                } else {
                    authFile = argv[i];
                    break;
                }

            } else if (smatch(cp, "password") || smatch(cp, "p")) {
                if (++i == argc) {
                    errflg++;
                } else {
                    password = argv[i];
                    break;
                }

            } else {
                errflg++;
            }
        }
    }
    nextArg = i;

    if ((nextArg + 3) > argc) {
        errflg++;
    }
    if (errflg) {
        printUsage();
        exit(2);
    }
    realm = argv[nextArg++];
    username = argv[nextArg++];

    bufCreate(&buf, 0, 0);
    for (i = nextArg; i < argc; ) {
        bufPutStr(&buf, argv[i]);
        if (++i < argc) {
            bufPutc(&buf, ',');
        }
    }
    roles = sclone(buf.servp);

    logOpen();
    websOpenAuth(1);

    if (authFile && access(authFile, R_OK) == 0) {
        if (websLoad(authFile) < 0) {
            exit(2);
        }
        if (access(authFile, W_OK) < 0) {
            error("Cannot write to %s", authFile);
            exit(4);
        }
    }
    if (!password && (password = getPassword()) == 0) {
        exit(1);
    }
    if (*password == '\0') {
        error("Password must not be empty");
        exit(3);
    }
    if (slen(password) > WEBS_MAX_PASSWORD) {
        error("Password exceeds maximum length of %d characters", WEBS_MAX_PASSWORD);
        exit(3);
    }
    if (smatch(cipher, "md5")) {
        encodedPassword = websMD5(sfmt("%s:%s:%s", username, realm, password));
    } else {
        /* This uses the more secure blowfish cipher */
        encodedPassword = websMakePassword(sfmt("%s:%s:%s", username, realm, password), 16, 128);
    }
    if (encodedPassword == 0) {
        error("Cannot encode password for user \"%s\"", username);
        exit(3);
    }
    if (authFile) {
        websRemoveUser(username);
        if (websAddUser(username, encodedPassword, roles) == 0) {
            exit(7);
        }
        if (writeAuthFile(authFile) < 0) {
            exit(6);
        }
    } else {
        printf("%s\n", encodedPassword);
    }
    websCloseAuth();
    return 0;
}


static int writeAuthFile(char *path)
{
    FILE        *fp;
    WebsKey     *kp, *ap;
    WebsRole    *role;
    WebsUser    *user;
    WebsHash    roles, users;
    char        *dir, *sep, *tempFile;

    assert(path && *path);

    /*
        Stage the new file beside the old one so the final rename stays on one filesystem and replaces it atomically.
     */
    dir = sclone(path);
    if ((sep = strrchr(dir, '/')) != 0 || (sep = strrchr(dir, '\\')) != 0) {
        sep[sep == dir ? 1 : 0] = '\0';
    } else {
        wfree(dir);
        dir = sclone(".");
    }
    tempFile = websTempFile(dir, "gp");
    wfree(dir);
    if (tempFile == 0) {
        error("Cannot create a temp file to rewrite %s", path);
        return -1;
    }
    if ((fp = fopen(tempFile, "w" FILE_TEXT)) == 0) {
        error("Cannot open %s", tempFile);
        wfree(tempFile);
        return -1;
    }
    fprintf(fp, "#\n#   %s - Authorization data\n#\n\n", basename(path));

    roles = websGetRoles();
    if (roles >= 0) {
        for (kp = hashFirst(roles); kp; kp = hashNext(roles, kp)) {
            role = kp->content.value.symbol;
            fprintf(fp, "role name=%s abilities=", kp->name.value.string);
            for (ap = hashFirst(role->abilities); ap; ap = hashNext(role->abilities, ap)) {
                fprintf(fp, "%s,", ap->name.value.string);
            }
            fputc('\n', fp);
        }
        fputc('\n', fp);
    }
    users = websGetUsers();
    if (users >= 0) {
        for (kp = hashFirst(users); kp; kp = hashNext(users, kp)) {
            user = kp->content.value.symbol;
            fprintf(fp, "user name=%s password=%s roles=%s", user->name, user->password, user->roles);
            fputc('\n', fp);
        }
    }
    fclose(fp);
#if ME_WIN_LIKE
    //  Windows rename does not replace an existing file
    unlink(path);
#endif
    if (rename(tempFile, path) < 0) {
        error("Cannot create new %s from %s errno %d", path, tempFile, errno);
        unlink(tempFile);
        wfree(tempFile);
        return -1;
    }
    wfree(tempFile);
    return 0;
}


static char *getPassword(void)
{
    char    *password, *confirm;

    /*
        getpass may return the same static buffer for both prompts, so keep a copy of the first answer.
     */
    password = sclone(getpass("New password: "));
    confirm = getpass("Confirm password: ");
    if (smatch(password, confirm)) {
        return password;
    }
    wfree(password);
    error("Password not verified");
    return 0;
}



#if WINCE
static char *getpass(char *prompt)
{
    return "NOT-SUPPORTED";
}

#elif ME_WIN_LIKE || VXWORKS
static char *getpass(char *prompt)
{
    static char password[MAX_PASS];
    int     c, i;

    fputs(prompt, stdout);
    for (i = 0; i < (int) sizeof(password) - 1; i++) {
#if VXWORKS
        c = getchar();
#else
        c = _getch();
#endif
        if (c == '\r' || c == EOF) {
            break;
        }
        if ((c == '\b' || c == 127) && i > 0) {
            password[--i] = '\0';
            fputs("\b \b", stdout);
            i--;
        } else if (c == 26) {           /* Control Z */
            c = EOF;
            break;
        } else if (c == 3) {            /* Control C */
            fputs("^C\n", stdout);
            exit(255);
        } else if (!iscntrl((uchar) c) && (i < (int) sizeof(password) - 1)) {
            password[i] = (char) c;
            fputc('*', stdout);
        } else {
            fputc('', stdout);
            i--;
        }
    }
    if (c == EOF) {
        return "";
    }
    fputc('\n', stdout);
    password[i] = '\0';
    return sclone(password);
}

#endif /* ME_WIN_LIKE */

/*
    Display the usage
 */

static void printUsage(void)
{
    fprintf(stderr, "usage: gopass [--cipher cipher] [--file path] [--password password] realm user roles...\n"
        "Options:\n"
        "    --cipher md5|blowfish Select the encryption cipher. Defaults to blowfish\n"
        "    --file filename       Modify the password file\n"
        "    --password password   Use the specified password\n"
        "\n");
}

#else

int main(int argc, char *argv[])
{
    fprintf(stderr, "gopass: authentication is not enabled in this build (ME_GOAHEAD_AUTH)\n");
    return 1;
}
#endif /* ME_GOAHEAD_AUTH */


/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
