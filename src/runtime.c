/*
    runtime.c -- Runtime support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/*********************************** Defines **********************************/
/*
    This structure stores scheduled events.
 */
typedef struct Callback {
    void (*routine)(void *arg, int id);
    void *arg;
    WebsTime at;
    int id;
} Callback;

/*********************************** Defines **********************************/
/*
    Class definitions
 */
#define CLASS_NORMAL    0               /* [All other]      Normal characters */
#define CLASS_PERCENT   1               /* [%]              Begin format */
#define CLASS_MODIFIER  2               /* [-+ #,]          Modifiers */
#define CLASS_ZERO      3               /* [0]              Special modifier - zero pad */
#define CLASS_STAR      4               /* [*]              Width supplied by arg */
#define CLASS_DIGIT     5               /* [1-9]            Field widths */
#define CLASS_DOT       6               /* [.]              Introduce precision */
#define CLASS_BITS      7               /* [hlL]            Length bits */
#define CLASS_TYPE      8               /* [cdefginopsSuxX] Type specifiers */

#define STATE_NORMAL    0               /* Normal chars in format string */
#define STATE_PERCENT   1               /* "%" */
#define STATE_MODIFIER  2               /* -+ #,*/
#define STATE_WIDTH     3               /* Width spec */
#define STATE_DOT       4               /* "." */
#define STATE_PRECISION 5               /* Precision spec */
#define STATE_BITS      6               /* Size spec */
#define STATE_TYPE      7               /* Data type */
#define STATE_COUNT     8

PUBLIC char stateMap[] = {
    /*     STATES:  Normal Percent Modifier Width  Dot  Prec Bits Type */
    /* CLASS           0      1       2       3     4     5    6    7  */
    /* Normal   0 */ 0,     0,      0,      0,    0,    0,   0,   0,
    /* Percent  1 */ 1,     0,      1,      1,    1,    1,   1,   1,
    /* Modifier 2 */ 0,     2,      2,      0,    0,    0,   0,   0,
    /* Zero     3 */ 0,     2,      2,      3,    5,    5,   0,   0,
    /* Star     4 */ 0,     3,      3,      0,    5,    0,   0,   0,
    /* Digit    5 */ 0,     3,      3,      3,    5,    5,   0,   0,
    /* Dot      6 */ 0,     4,      4,      4,    0,    0,   0,   0,
    /* Bits     7 */ 0,     6,      6,      6,    6,    6,   6,   0,
    /* Types    8 */ 0,     7,      7,      7,    7,    7,   7,   0,
};

/*
    Format:         %[modifier][width][precision][bits][type]

    The Class map will map from a specifier letter to a state.
 */
PUBLIC char classMap[] = {
    /*   0  ' '    !     "     #     $     %     &     ' */
    2,    0,    0,    2,    0,    1,    0,    0,
    /*  07   (     )     *     +     ,     -     .     / */
    0,    0,    4,    2,    2,    2,    6,    0,
    /*  10   0     1     2     3     4     5     6     7 */
    3,    5,    5,    5,    5,    5,    5,    5,
    /*  17   8     9     :     ;     <     =     >     ? */
    5,    5,    0,    0,    0,    0,    0,    0,
    /*  20   @     A     B     C     D     E     F     G */
    8,    0,    0,    0,    0,    0,    0,    0,
    /*  27   H     I     J     K     L     M     N     O */
    0,    0,    0,    0,    7,    0,    8,    0,
    /*  30   P     Q     R     S     T     U     V     W */
    0,    0,    0,    8,    0,    0,    0,    0,
    /*  37   X     Y     Z     [     \     ]     ^     _ */
    8,    0,    0,    0,    0,    0,    0,    0,
    /*  40   '     a     b     c     d     e     f     g */
    0,    0,    0,    8,    8,    8,    8,    8,
    /*  47   h     i     j     k     l     m     n     o */
    7,    8,    0,    0,    7,    0,    8,    8,
    /*  50   p     q     r     s     t     u     v     w */
    8,    0,    0,    8,    0,    8,    0,    8,
    /*  57   x     y     z  */
    8,    0,    0,
};

/*
    Flags
 */
#define SPRINTF_LEFT       0x1          /* Left align */
#define SPRINTF_SIGN       0x2          /* Always sign the result */
#define SPRINTF_LEAD_SPACE 0x4          /* put leading space for +ve numbers */
#define SPRINTF_ALTERNATE  0x8          /* Alternate format */
#define SPRINTF_LEAD_ZERO  0x10         /* Zero pad */
#define SPRINTF_SHORT      0x20         /* 16-bit */
#define SPRINTF_LONG       0x40         /* 32-bit */
#define SPRINTF_INT64      0x80         /* 64-bit */
#define SPRINTF_COMMA      0x100        /* Thousand comma separators */
#define SPRINTF_UPPER_CASE 0x200        /* As the name says for numbers */

typedef struct Format {
    uchar *buf;
    uchar *endbuf;
    uchar *start;
    uchar *end;
    ssize growBy;
    ssize maxsize;
    int precision;
    int radix;
    int width;
    int flags;
    int len;
} Format;

#define BPUT(fmt, c) \
        do { \
            /* Less one to allow room for the null */ \
            if ((fmt)->end >= ((fmt)->endbuf - sizeof(char))) { \
                if (growBuf(fmt) > 0) { \
                    *(fmt)->end++ = (c); \
                } \
            } else { \
                *(fmt)->end++ = (c); \
            } \
        } while (0)

#define BPUTNULL(fmt) \
        do { \
            if ((fmt)->end > (fmt)->endbuf) { \
                if (growBuf(fmt) > 0) { \
                    *(fmt)->end = '\0'; \
                } \
            } else { \
                *(fmt)->end = '\0'; \
            } \
        } while (0)

/*
    The handle list stores the length of the list and the number of used handles in the first two words.  These are
    hidden from the caller by returning a pointer to the third word to the caller.
 */
#define H_LEN    0          /* First entry holds length of list */
#define H_USED   1          /* Second entry holds number of used */
#define H_OFFSET 2          /* Offset to real start of list */
#define H_INCR   16         /* Grow handle list in chunks this size */

#define RINGQ_LEN(bp) ((bp->servp > bp->endp) ? (bp->buflen + (bp->endp - bp->servp)) : (bp->endp - bp->servp))

typedef struct HashTable {  /* Symbol table descriptor */
    WebsKey **hash_table;   /* Allocated at run time */
    int inuse;              /* Is this entry in use */
    int size;               /* Size of the table below */
} HashTable;

#ifndef LOG_ERR
    #define LOG_ERR 0
#endif
#if ME_WIN_LIKE
PUBLIC void syslog(int priority, char *fmt, ...);
#endif

PUBLIC int logLevel;                /* Log verbosity level */

/************************************* Locals *********************************/

static Callback **callbacks;
static int      callbackMax;

static HashTable **sym;             /* List of symbol tables */
static int       symMax;            /* One past the max symbol table */

char *embedthisGoAheadCopyright = EMBEDTHIS_GOAHEAD_COPYRIGHT;

#if ME_GOAHEAD_LOGGING
static char *logPath;               /* Log file name */
static int  logFd;                  /* Log file handle */
#endif

/********************************** Forwards **********************************/

static int calcPrime(int size);
static int getBinBlockSize(int size);
static int hashIndex(HashTable *tp, cchar *name);
static WebsKey *hash(HashTable *tp, cchar *name);

#if ME_GOAHEAD_LOGGING
static void defaultLogHandler(int level, cchar *buf);
static WebsLogHandler logHandler = defaultLogHandler;
#endif

static int  getState(char c, int state);
static int  growBuf(Format *fmt);
static char *sprintfCore(char *buf, ssize maxsize, cchar *fmt, va_list arg);
static void outNum(Format *fmt, char *prefix, uint64 val);
static void outString(Format *fmt, char *str, ssize len);
#if ME_FLOAT
static void outFloat(Format *fmt, char specChar, double value);
#endif

/************************************* Code ***********************************/

PUBLIC int websRuntimeOpen(void)
{
    symMax = 0;
    sym = 0;
    srand((uint) time(NULL));
    return 0;
}


PUBLIC void websRuntimeClose(void)
{
}


/*
    This function is called when a scheduled process time has come.
 */
static void callEvent(int id)
{
    Callback *cp;

    assert(0 <= id && id < callbackMax);
    cp = callbacks[id];
    assert(cp);

    (cp->routine)(cp->arg, cp->id);
}


/*
    Schedule an event in delay milliseconds time. We will use 1 second granularity for webServer.
 */
PUBLIC int websStartEvent(int delay, WebsEventProc proc, void *arg)
{
    Callback *s;
    int      id;

    if ((id = wallocObject(&callbacks, &callbackMax, sizeof(Callback))) < 0) {
        return -1;
    }
    s = callbacks[id];
    s->routine = proc;
    s->arg = arg;
    s->id = id;

    /*
        Round the delay up to seconds.
     */
    s->at = ((delay + 500) / 1000) + time(0);
    return id;
}


PUBLIC void websRestartEvent(int id, int delay)
{
    Callback *s;

    if (callbacks == NULL || id == -1 || id >= callbackMax || (s = callbacks[id]) == NULL) {
        return;
    }
    s->at = ((delay + 500) / 1000) + time(0);
}


PUBLIC void websStopEvent(int id)
{
    Callback *s;

    if (callbacks == NULL || id == -1 || id >= callbackMax || (s = callbacks[id]) == NULL) {
        return;
    }
    wfree(s);
    callbackMax = wfreeHandle(&callbacks, id);
}


int websRunEvents(void)
{
    Callback *s;
    WebsTime now;
    int      i, delay, nextEvent;

    nextEvent = (MAXINT / 1000);
    now = time(0);

    for (i = 0; i < callbackMax; i++) {
        if ((s = callbacks[i]) != NULL) {
            if (s->at <= now) {
                callEvent(i);
                delay = MAXINT / 1000;
                /* Rescan in case event scheduled or modified an event */
                i = -1;
            } else {
                delay = (int) min(s->at - now, MAXINT / 1000);
            }
            nextEvent = min(delay, nextEvent);
        }
    }
    return nextEvent * 1000;
}


/*
    Allocating secure replacement for sprintf and vsprintf.
 */
PUBLIC char *sfmt(cchar *format, ...)
{
    va_list ap;
    char    *result;

    assert(format);

    va_start(ap, format);
    result = sprintfCore(NULL, -1, format, ap);
    va_end(ap);
    return result;
}


/*
    Replacement for sprintf
 */
PUBLIC char *fmt(char *buf, ssize bufsize, cchar *format, ...)
{
    va_list ap;
    char    *result;

    assert(buf);
    assert(format);

    if (bufsize <= 0) {
        return 0;
    }
    va_start(ap, format);
    result = sprintfCore(buf, bufsize, format, ap);
    va_end(ap);
    return result;
}


/*
    Secure vsprintf replacement
 */
PUBLIC char *sfmtv(cchar *fmt, va_list arg)
{
    assert(fmt);
    return sprintfCore(NULL, -1, fmt, arg);
}


static int getState(char c, int state)
{
    int chrClass;

    if (c < ' ' || c > 'z') {
        chrClass = CLASS_NORMAL;
    } else {
        assert((c - ' ') < (int) sizeof(classMap));
        chrClass = classMap[(c - ' ')];
    }
    assert((chrClass * STATE_COUNT + state) < (int) sizeof(stateMap));
    state = stateMap[chrClass * STATE_COUNT + state];
    return state;
}


static char *sprintfCore(char *buf, ssize maxsize, cchar *spec, va_list args)
{
    Format fmt;
    ssize  len;
    int64  iValue;
    uint64 uValue;
    int    state;
    char   c, *safe;

    if (spec == 0) {
        spec = "";
    }
    if (buf != 0) {
        assert(maxsize > 0);
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[maxsize];
        fmt.growBy = -1;
    } else {
        if (maxsize <= 0) {
            maxsize = MAXINT;
        }
        len = min(256, maxsize);
        if ((buf = walloc(len)) == 0) {
            return 0;
        }
        fmt.buf = (uchar*) buf;
        fmt.endbuf = &fmt.buf[len];
        fmt.growBy = min(512, maxsize - len);
    }
    fmt.maxsize = maxsize;
    fmt.start = fmt.buf;
    fmt.end = fmt.buf;
    fmt.len = 0;
    *fmt.start = '\0';

    state = STATE_NORMAL;

    while ((c = *spec++) != '\0') {
        state = getState(c, state);

        switch (state) {
        case STATE_NORMAL:
            BPUT(&fmt, c);
            break;

        case STATE_PERCENT:
            fmt.precision = -1;
            fmt.width = 0;
            fmt.flags = 0;
            break;

        case STATE_MODIFIER:
            switch (c) {
            case '+':
                fmt.flags |= SPRINTF_SIGN;
                break;
            case '-':
                fmt.flags |= SPRINTF_LEFT;
                break;
            case '#':
                fmt.flags |= SPRINTF_ALTERNATE;
                break;
            case '0':
                fmt.flags |= SPRINTF_LEAD_ZERO;
                break;
            case ' ':
                fmt.flags |= SPRINTF_LEAD_SPACE;
                break;
            case ',':
                fmt.flags |= SPRINTF_COMMA;
                break;
            }
            break;

        case STATE_WIDTH:
            if (c == '*') {
                fmt.width = va_arg(args, int);
                if (fmt.width < 0) {
                    fmt.width = -fmt.width;
                    fmt.flags |= SPRINTF_LEFT;
                }
            } else {
                while (isdigit((uchar) c)) {
                    fmt.width = fmt.width * 10 + (c - '0');
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_DOT:
            fmt.precision = 0;
            break;

        case STATE_PRECISION:
            if (c == '*') {
                fmt.precision = va_arg(args, int);
            } else {
                while (isdigit((uchar) c)) {
                    fmt.precision = fmt.precision * 10 + (c - '0');
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_BITS:
            switch (c) {
            case 'L':
                fmt.flags |= SPRINTF_INT64;
                break;

            case 'l':
                fmt.flags |= SPRINTF_LONG;
                break;

            case 'h':
                fmt.flags |= SPRINTF_SHORT;
                break;
            }
            break;

        case STATE_TYPE:
            switch (c) {
            case 'e':
#if ME_FLOAT
            case 'g':
            case 'f':
                fmt.radix = 10;
                outFloat(&fmt, c, (double) va_arg(args, double));
                break;
#endif /* ME_FLOAT */

            case 'c':
                BPUT(&fmt, (char) va_arg(args, int));
                break;

            case 'S':
                /* Safe string */
#if ME_CHAR_LEN > 1 && KEEP
                if (fmt.flags & SPRINTF_LONG) {
                    //  UNICODE - not right wchar
                    safe = websEscapeHtml(va_arg(args, wchar*));
                    outWideString(&fmt, safe, -1);
                    wfree(safe);
                } else
#endif
                {
                    safe = websEscapeHtml(va_arg(args, char*));
                    outString(&fmt, safe, -1);
                    wfree(safe);
                }
                break;

            case 'w':
                /* Wide string of wchar characters (Same as %ls"). Null terminated. */
#if ME_CHAR_LEN > 1 && KEEP
                outWideString(&fmt, va_arg(args, wchar*), -1);
                break;
#else
                /* Fall through */
#endif

            case 's':
                /* Standard string */
#if ME_CHAR_LEN > 1 && KEEP
                if (fmt.flags & SPRINTF_LONG) {
                    outWideString(&fmt, va_arg(args, wchar*), -1);
                } else
#endif
                outString(&fmt, va_arg(args, char*), -1);
                break;

            case 'i':
            case 'd':
                fmt.radix = 10;
                if (fmt.flags & SPRINTF_SHORT) {
                    iValue = (short) va_arg(args, int);
                } else if (fmt.flags & SPRINTF_LONG) {
                    iValue = (long) va_arg(args, long);
                } else if (fmt.flags & SPRINTF_INT64) {
                    iValue = (int64) va_arg(args, int64);
                } else {
                    iValue = (int) va_arg(args, int);
                }
                if (iValue >= 0) {
                    if (fmt.flags & SPRINTF_LEAD_SPACE) {
                        outNum(&fmt, " ", iValue);
                    } else if (fmt.flags & SPRINTF_SIGN) {
                        outNum(&fmt, "+", iValue);
                    } else {
                        outNum(&fmt, 0, iValue);
                    }
                } else {
                    outNum(&fmt, "-", -iValue);
                }
                break;

            case 'X':
                fmt.flags |= SPRINTF_UPPER_CASE;
#if ME_64
                fmt.flags &= ~(SPRINTF_SHORT | SPRINTF_LONG);
                fmt.flags |= SPRINTF_INT64;
#else
                fmt.flags &= ~(SPRINTF_INT64);
#endif
            /*  Fall through  */
            case 'o':
            case 'x':
            case 'u':
                if (fmt.flags & SPRINTF_SHORT) {
                    uValue = (ushort) va_arg(args, uint);
                } else if (fmt.flags & SPRINTF_LONG) {
                    uValue = (ulong) va_arg(args, ulong);
                } else if (fmt.flags & SPRINTF_INT64) {
                    uValue = (uint64) va_arg(args, uint64);
                } else {
                    uValue = va_arg(args, uint);
                }
                if (c == 'u') {
                    fmt.radix = 10;
                    outNum(&fmt, 0, uValue);
                } else if (c == 'o') {
                    fmt.radix = 8;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        outNum(&fmt, "0", uValue);
                    } else {
                        outNum(&fmt, 0, uValue);
                    }
                } else {
                    fmt.radix = 16;
                    if (fmt.flags & SPRINTF_ALTERNATE && uValue != 0) {
                        if (c == 'X') {
                            outNum(&fmt, "0X", uValue);
                        } else {
                            outNum(&fmt, "0x", uValue);
                        }
                    } else {
                        outNum(&fmt, 0, uValue);
                    }
                }
                break;

            case 'n':       /* Count of chars seen thus far */
                if (fmt.flags & SPRINTF_SHORT) {
                    short *count = va_arg(args, short*);
                    *count = (int) (fmt.end - fmt.start);
                } else if (fmt.flags & SPRINTF_LONG) {
                    long *count = va_arg(args, long*);
                    *count = (int) (fmt.end - fmt.start);
                } else {
                    int *count = va_arg(args, int*);
                    *count = (int) (fmt.end - fmt.start);
                }
                break;

            case 'p':       /* Pointer */
#if ME_64
                uValue = (uint64) va_arg(args, void*);
#else
                uValue = (uint) PTOI(va_arg(args, void*));
#endif
                fmt.radix = 16;
                outNum(&fmt, "0x", uValue);
                break;

            default:
                BPUT(&fmt, c);
            }
        }
    }
    BPUTNULL(&fmt);
    return (char*) fmt.buf;
}


static void outString(Format *fmt, char *str, ssize len)
{
    char  *cp;
    ssize i;

    if (str == NULL) {
        str = "null";
        len = 4;
    } else if (fmt->flags & SPRINTF_ALTERNATE) {
        str++;
        len = (ssize) * str;
    } else if (fmt->precision >= 0) {
        for (cp = str, len = 0; len < fmt->precision; len++) {
            if (*cp++ == '\0') {
                break;
            }
        }
    } else if (len < 0) {
        len = slen(str);
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
    for (i = 0; i < len && *str; i++) {
        BPUT(fmt, *str++);
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
}


#if ME_CHAR_LEN > 1 && KEEP
static void outWideString(Format *fmt, wchar *str, ssize len)
{
    wchar *cp;
    int   i;

    if (str == 0) {
        BPUT(fmt, (char) 'n');
        BPUT(fmt, (char) 'u');
        BPUT(fmt, (char) 'l');
        BPUT(fmt, (char) 'l');
        return;
    } else if (fmt->flags & SPRINTF_ALTERNATE) {
        str++;
        len = (ssize) * str;
    } else if (fmt->precision >= 0) {
        for (cp = str, len = 0; len < fmt->precision; len++) {
            if (*cp++ == 0) {
                break;
            }
        }
    } else if (len < 0) {
        for (cp = str, len = 0; *cp++ == 0; len++);
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
    for (i = 0; i < len && *str; i++) {
        BPUT(fmt, *str++);
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = len; i < fmt->width; i++) {
            BPUT(fmt, (char) ' ');
        }
    }
}
#endif


static void outNum(Format *fmt, char *prefix, uint64 value)
{
    char numBuf[64];
    char *cp;
    char *endp;
    char c;
    int  letter, len, leadingZeros, i, fill;

    endp = &numBuf[sizeof(numBuf) - 1];
    *endp = '\0';
    cp = endp;

    /*
     *  Convert to ascii
     */
    if (fmt->radix == 16) {
        do {
            letter = (int) (value % fmt->radix);
            if (letter > 9) {
                if (fmt->flags & SPRINTF_UPPER_CASE) {
                    letter = 'A' + letter - 10;
                } else {
                    letter = 'a' + letter - 10;
                }
            } else {
                letter += '0';
            }
            *--cp = letter;
            value /= fmt->radix;
        } while (value > 0);

    } else if (fmt->flags & SPRINTF_COMMA) {
        i = 1;
        do {
            *--cp = '0' + (int) (value % fmt->radix);
            value /= fmt->radix;
            if ((i++ % 3) == 0 && value > 0) {
                *--cp = ',';
            }
        } while (value > 0);
    } else {
        do {
            *--cp = '0' + (int) (value % fmt->radix);
            value /= fmt->radix;
        } while (value > 0);
    }

    len = (int) (endp - cp);
    fill = fmt->width - len;

    if (prefix != 0) {
        fill -= (int) slen(prefix);
    }
    leadingZeros = (fmt->precision > len) ? fmt->precision - len : 0;
    fill -= leadingZeros;

    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(fmt, c);
        }
    }
    if (prefix != 0) {
        while (*prefix) {
            BPUT(fmt, *prefix++);
        }
    }
    for (i = 0; i < leadingZeros; i++) {
        BPUT(fmt, '0');
    }
    while (*cp) {
        BPUT(fmt, *cp);
        cp++;
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(fmt, ' ');
        }
    }
}


#if ME_FLOAT
static void outFloat(Format *fmt, char specChar, double value)
{
    char result[ME_GOAHEAD_LIMIT_STRING], *cp;
    int  c, fill, i, len, index;

    result[0] = '\0';
    if (specChar == 'f') {
        snprintf(result, sizeof(result), "%.*f", fmt->precision, value);

    } else if (specChar == 'g') {
        snprintf(result, sizeof(result), "%*.*g", fmt->width, fmt->precision, value);

    } else if (specChar == 'e') {
        snprintf(result, sizeof(result), "%*.*e", fmt->width, fmt->precision, value);
    }
    len = (int) slen(result);
    fill = fmt->width - len;
    if (fmt->flags & SPRINTF_COMMA) {
        if (((len - 1) / 3) > 0) {
            fill -= (len - 1) / 3;
        }
    }

    if (fmt->flags & SPRINTF_SIGN && value > 0) {
        BPUT(fmt, '+');
        fill--;
    }
    if (!(fmt->flags & SPRINTF_LEFT)) {
        c = (fmt->flags & SPRINTF_LEAD_ZERO) ? '0': ' ';
        for (i = 0; i < fill; i++) {
            BPUT(fmt, c);
        }
    }
    index = len;
    for (cp = result; *cp; cp++) {
        BPUT(fmt, *cp);
        if (fmt->flags & SPRINTF_COMMA) {
            if ((--index % 3) == 0 && index > 0) {
                BPUT(fmt, ',');
            }
        }
    }
    if (fmt->flags & SPRINTF_LEFT) {
        for (i = 0; i < fill; i++) {
            BPUT(fmt, ' ');
        }
    }
    BPUTNULL(fmt);
}
#endif /* ME_FLOAT */


/*
    Grow the buffer to fit new data. Return 1 if the buffer can grow.
    Grow using the growBy size specified when creating the buffer.
 */
static int growBuf(Format *fmt)
{
    uchar *newbuf;
    ssize buflen;

    buflen = (int) (fmt->endbuf - fmt->buf);
    if (fmt->maxsize >= 0 && buflen >= fmt->maxsize) {
        return 0;
    }
    if (fmt->growBy <= 0) {
        /*
            User supplied buffer
         */
        return 0;
    }
    if ((newbuf = walloc(buflen + fmt->growBy)) == 0) {
        return -1;
    }
    if (fmt->buf) {
        memcpy(newbuf, fmt->buf, buflen);
        wfree(fmt->buf);
    }
    buflen += fmt->growBy;
    fmt->end = newbuf + (fmt->end - fmt->buf);
    fmt->start = newbuf + (fmt->start - fmt->buf);
    fmt->buf = newbuf;
    fmt->endbuf = &fmt->buf[buflen];

    /*
        Increase growBy to reduce overhead
     */
    if ((buflen + (fmt->growBy * 2)) < fmt->maxsize) {
        fmt->growBy *= 2;
    }
    return 1;
}


WebsValue valueInteger(long value)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = integer;
    v.value.integer = value;
    return v;
}


WebsValue valueSymbol(void *value)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = symbol;
    v.value.symbol = value;
    return v;
}


WebsValue valueString(cchar *value, int flags)
{
    WebsValue v;

    memset(&v, 0x0, sizeof(v));
    v.valid = 1;
    v.type = string;
    if (flags & VALUE_ALLOCATE) {
        v.allocated = 1;
        v.value.string = sclone(value);
    } else {
        v.allocated = 0;
        v.value.string = (char*) value;
    }
    return v;
}


PUBLIC void valueFree(WebsValue *v)
{
    if (v->valid && v->allocated && v->type == string && v->value.string != NULL) {
        wfree(v->value.string);
        v->value.string = NULL;
    }
    v->type = undefined;
    v->valid = 0;
    v->allocated = 0;
}


#if ME_GOAHEAD_LOGGING
static void defaultLogHandler(int flags, cchar *buf)
{
    char prefix[ME_GOAHEAD_LIMIT_STRING];

    if (logFd >= 0) {
        if (flags & WEBS_RAW_MSG) {
            write(logFd, buf, (int) slen(buf));
        } else {
            fmt(prefix, sizeof(prefix), "%s: %d: ", ME_NAME, flags & WEBS_LEVEL_MASK);
            write(logFd, prefix, (int) slen(prefix));
            write(logFd, buf, (int) slen(buf));
            write(logFd, "\n", 1);
#if ME_WIN_LIKE || ME_UNIX_LIKE
            if (flags & WEBS_ERROR_MSG && websGetBackground()) {
                syslog(LOG_ERR, "%s", buf);
            }
#endif
        }
    }
}


PUBLIC void error(cchar *fmt, ...)
{
    va_list args;
    char    *message;

    if (!logHandler) {
        return;
    }
    va_start(args, fmt);
    message = sfmtv(fmt, args);
    va_end(args);
    logHandler(WEBS_ERROR_MSG, message);
    wfree(message);
}


PUBLIC void assertError(WEBS_ARGS_DEC, char *fmt, ...)
{
    va_list args;
    char    *fmtBuf, *message;

    va_start(args, fmt);
    fmtBuf = sfmtv(fmt, args);

    message = sfmt("Assertion %s, failed at %s %d\n", fmtBuf, WEBS_ARGS);
    va_end(args);
    wfree(fmtBuf);
    if (logHandler) {
        logHandler(WEBS_ASSERT_MSG, message);
    }
    wfree(message);
}


PUBLIC void logmsgProc(int level, cchar *fmt, ...)
{
    va_list args;
    char    *message;

    if ((level & WEBS_LEVEL_MASK) <= logLevel && logHandler) {
        va_start(args, fmt);
        message = sfmtv(fmt, args);
        logHandler(level | WEBS_LOG_MSG, message);
        wfree(message);
        va_end(args);
    }
}


PUBLIC void traceProc(int level, cchar *fmt, ...)
{
    va_list args;
    char    *message;

    if ((level & WEBS_LEVEL_MASK) <= logLevel && logHandler) {
        va_start(args, fmt);
        message = sfmtv(fmt, args);
        logHandler(level | WEBS_TRACE_MSG, message);
        wfree(message);
        va_end(args);
    }
}


PUBLIC int websGetLogLevel(void)
{
    return logLevel;
}


void websSetLogLevel(int level)
{
    logLevel = level;
}


WebsLogHandler logGetHandler(void)
{
    return logHandler;
}


/*
    Replace the default trace handler. Return a pointer to the old handler.
 */
WebsLogHandler logSetHandler(WebsLogHandler handler)
{
    WebsLogHandler oldHandler;

    oldHandler = logHandler;
    if (handler) {
        logHandler = handler;
    }
    return oldHandler;
}


PUBLIC int logOpen(void)
{
    if (!logPath) {
        /* This definition comes from main.me goahead.logfile */
        logSetPath(ME_GOAHEAD_LOGFILE);
    }
    if (smatch(logPath, "stdout")) {
        logFd = 1;
    } else if (smatch(logPath, "stderr")) {
        logFd = 2;
#if !ME_ROM
    } else {
        if ((logFd = open(logPath, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0666)) < 0) {
            return -1;
        }
        lseek(logFd, 0, SEEK_END);
#endif
    }
    logSetHandler(logHandler);
    return 0;
}


PUBLIC void logClose(void)
{
#if !ME_ROM
    if (logFd > 2) {
        close(logFd);
        logFd = -1;
    }
#endif
}


PUBLIC void logSetPath(cchar *path)
{
    char *lp;

    wfree(logPath);
    logPath = sclone(path);
    if ((lp = strchr(logPath, ':')) != 0) {
        *lp++ = '\0';
        logLevel = atoi(lp);
    }
}
#endif


/*
    Convert a string to lower case
 */
PUBLIC char *slower(char *string)
{
    char *s;

    if (string == NULL) {
        return NULL;
    }
    s = string;
    while (*s) {
        if (isupper(*s)) {
            *s = (char) tolower((uchar) * s);
        }
        s++;
    }
    *s = '\0';
    return string;
}


/*
    Convert a string to upper case
 */
PUBLIC char *supper(char *string)
{
    char *s;

    if (string == NULL) {
        return NULL;
    }
    s = string;
    while (*s) {
        if (islower((uchar) * s)) {
            *s = (char) toupper((uchar) * s);
        }
        s++;
    }
    *s = '\0';
    return string;
}


PUBLIC char *itosbuf(char *buf, ssize size, int64 value, int radix)
{
    char *cp, *end;
    char digits[] = "0123456789ABCDEF";
    int  negative;

    if ((radix != 10 && radix != 16) || size < 2) {
        return 0;
    }
    end = cp = &buf[size];
    *--cp = '\0';

    if (value < 0) {
        negative = 1;
        value = -value;
        size--;
    } else {
        negative = 0;
    }
    do {
        *--cp = digits[value % radix];
        value /= radix;
    } while (value > 0 && cp > buf);

    if (negative) {
        if (cp <= buf) {
            return 0;
        }
        *--cp = '-';
    }
    if (buf < cp) {
        /* Move the null too */
        memmove(buf, cp, end - cp);
    }
    return buf;
}


/*
    Allocate a new file handle. On the first call, the caller must set the handle map to be a pointer to a null
    pointer.  map points to the second element in the handle array.
 */
PUBLIC int wallocHandle(void *mapArg)
{
    void  ***map;
    ssize *mp;
    int   handle, len, memsize, incr;

    map = (void***) mapArg;
    assert(map);

    if (*map == NULL) {
        incr = H_INCR;
        memsize = (incr + H_OFFSET) * sizeof(void*);
        if ((mp = walloc(memsize)) == NULL) {
            return -1;
        }
        memset(mp, 0, memsize);
        mp[H_LEN] = incr;
        mp[H_USED] = 0;
        *map = (void*) &mp[H_OFFSET];
    } else {
        mp = &((*(ssize**) map)[-H_OFFSET]);
    }
    len = (int) mp[H_LEN];

    /*
        Find the first null handle
     */
    if (mp[H_USED] < mp[H_LEN]) {
        for (handle = 0; handle < len; handle++) {
            if (mp[handle + H_OFFSET] == 0) {
                mp[H_USED]++;
                return handle;
            }
        }
    } else {
        handle = len;
    }

    /*
        No free handle so grow the handle list. Grow list in chunks of H_INCR.
     */
    len += H_INCR;
    memsize = (len + H_OFFSET) * sizeof(void*);
    if ((mp = wrealloc(mp, memsize)) == NULL) {
        return -1;
    }
    *map = (void*) &mp[H_OFFSET];
    mp[H_LEN] = len;
    memset(&mp[H_OFFSET + len - H_INCR], 0, sizeof(ssize*) * H_INCR);
    mp[H_USED]++;
    return handle;
}


/*
    Free a handle. This function returns the value of the largest handle in use plus 1, to be saved as a max value.
 */
PUBLIC int wfreeHandle(void *mapArg, int handle)
{
    void  ***map;
    ssize *mp;
    int   len;

    map = (void***) mapArg;
    assert(map);
    mp = &((*(ssize**) map)[-H_OFFSET]);
    assert(mp[H_LEN] >= H_INCR);

    assert(mp[handle + H_OFFSET]);
    assert(mp[H_USED]);
    mp[handle + H_OFFSET] = 0;
    if (--(mp[H_USED]) == 0) {
        wfree((void*) mp);
        *map = NULL;
    }
    /*
        Find the greatest handle number in use.
     */
    if (*map == NULL) {
        handle = -1;
    } else {
        len = (int) mp[H_LEN];
        if (mp[H_USED] < mp[H_LEN]) {
            for (handle = len - 1; handle >= 0; handle--) {
                if (mp[handle + H_OFFSET])
                    break;
            }
        } else {
            handle = len;
        }
    }
    return handle + 1;
}


/*
    Allocate an entry in the halloc array
 */
PUBLIC int wallocObject(void *listArg, int *max, int size)
{
    void ***list;
    char *cp;
    int  id;

    list = (void***) listArg;
    assert(list);
    assert(max);

    if ((id = wallocHandle(list)) < 0) {
        return -1;
    }
    if (size > 0) {
        if ((cp = walloc(size)) == NULL) {
            wfreeHandle(list, id);
            return -1;
        }
        assert(cp);
        memset(cp, 0, size);
        (*list)[id] = (void*) cp;
    }
    if (id >= *max) {
        *max = id + 1;
    }
    return id;
}


/*
    Create a new buf. "increment" is the amount to increase the size of the buf should it need to grow to accommodate
    data being added. "maxsize" is an upper limit (sanity level) beyond which the buffer must not grow. Set maxsize to
       -1 to
    imply no upper limit. The buffer for the buf is always *  dynamically allocated. Set maxsize
 */
PUBLIC int bufCreate(WebsBuf *bp, int initSize, int maxsize)
{
    int increment;

    assert(bp);

    if (initSize <= 0) {
        initSize = ME_GOAHEAD_LIMIT_BUFFER;
    }
    if (maxsize == 0) {
        maxsize = initSize;
    }
    assert(initSize >= 0);
    memset(bp, 0, sizeof(WebsBuf));

    increment = getBinBlockSize(initSize);
    if ((bp->buf = walloc((increment))) == NULL) {
        return -1;
    }
    bp->maxsize = maxsize;
    bp->buflen = increment;
    bp->increment = increment;
    bp->endbuf = &bp->buf[bp->buflen];
    bp->servp = bp->buf;
    bp->endp = bp->buf;
    *bp->servp = '\0';
    return 0;
}


/*
    Delete a buf and free the buf buffer.
 */
PUBLIC void bufFree(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp == NULL) {
        return;
    }
    bufFlush(bp);
    wfree((char*) bp->buf);
    bp->buf = NULL;
    bp->servp = NULL;
    bp->endp = NULL;
    bp->buflen = 0;
}


/*
    Return the length of the data in the buf. Users must fill the queue to a high water mark of at most one less than
    the queue size.
 */
PUBLIC ssize bufLen(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp > bp->endp) {
        return bp->buflen + bp->endp - bp->servp;
    } else {
        return bp->endp - bp->servp;
    }
}


/*
    Return the reference to the start of data in the buffer
 */
PUBLIC char *bufStart(WebsBuf *bp)
{
    assert(bp);
    return bp->servp;
}


/*
    Get a byte from the queue
 */
PUBLIC int bufGetc(WebsBuf *bp)
{
    char c, *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp == bp->endp) {
        return -1;
    }

    cp = (char*) bp->servp;
    c = *cp++;
    bp->servp = cp;
    if (bp->servp >= bp->endbuf) {
        bp->servp = bp->buf;
    }
    return (int) ((uchar) c);
}


/*
    Add a char to the queue. Note if being used to store wide strings this does not add a trailing '\0'. Grow the buffer
       as
    required.
 */
PUBLIC int bufPutc(WebsBuf *bp, char c)
{
    char *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if ((bufRoom(bp) < (int) sizeof(char)) && !bufGrow(bp, 0)) {
        return -1;
    }
    cp = (char*) bp->endp;
    *cp++ = (char) c;
    bp->endp = cp;
    if (bp->endp >= bp->endbuf) {
        bp->endp = bp->buf;
    }
    return 0;
}


/*
    Insert a wide character at the front of the queue
 */
PUBLIC int bufInsertc(WebsBuf *bp, char c)
{
    char *cp;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) < (int) sizeof(char) && !bufGrow(bp, 0)) {
        return -1;
    }
    if (bp->servp <= bp->buf) {
        bp->servp = bp->endbuf;
    }
    cp = (char*) bp->servp;
    *--cp = (char) c;
    bp->servp = cp;
    return 0;
}


PUBLIC ssize bufPut(WebsBuf *bp, cchar *fmt, ...)
{
    va_list ap;
    char    *str;
    ssize   rc;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (fmt == 0) {
        return 0;
    }
    va_start(ap, fmt);
    str = sfmtv(fmt, ap);
    va_end(ap);

    rc = bufPutBlk(bp, str, strlen(str) * sizeof(char));
    if (rc) {
        *((char*) bp->endp) = (char) '\0';
    }
    return rc;
}


/*
    Add a string to the queue. Add a trailing null (maybe two nulls)
 */
PUBLIC ssize bufPutStr(WebsBuf *bp, cchar *str)
{
    ssize rc;

    assert(bp);
    assert(str);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    rc = bufPutBlk(bp, str, strlen(str) * sizeof(char));
    if (rc) {
        *((char*) bp->endp) = (char) '\0';
    }
    return rc;
}


/*
    Add a null terminator. This does NOT increase the size of the queue
 */
PUBLIC void bufAddNull(WebsBuf *bp)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) < (int) sizeof(char)) {
        bufGrow(bp, 0);
        return;
    }
    *((char*) bp->endp) = (char) '\0';
}


#if UNICODE
/*
    Get a byte from the queue
 */
PUBLIC int bufGetcA(WebsBuf *bp)
{
    uchar c;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bp->servp == bp->endp) {
        return -1;
    }

    c = *bp->servp++;
    if (bp->servp >= bp->endbuf) {
        bp->servp = bp->buf;
    }
    return c;
}


/*
    Add a byte to the queue. Note if being used to store strings this does not add a trailing '\0'.
    Grow the buffer as required.
 */
PUBLIC int bufPutcA(WebsBuf *bp, char c)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) == 0 && !bufGrow(bp)) {
        return -1;
    }
    *bp->endp++ = (uchar) c;
    if (bp->endp >= bp->endbuf) {
        bp->endp = bp->buf;
    }
    return 0;
}


/*
    Insert a byte at the front of the queue
 */
PUBLIC int bufInsertcA(WebsBuf *bp, char c)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    if (bufRoom(bp) == 0 && !bufGrow(bp)) {
        return -1;
    }
    if (bp->servp <= bp->buf) {
        bp->servp = bp->endbuf;
    }
    *--bp->servp = (uchar) c;
    return 0;
}


/*
    Add a string to the queue. Add a trailing null (not really in the q). ie. beyond the last valid byte.
 */
PUBLIC int bufPutStrA(WebsBuf *bp, char *str)
{
    int rc;

    assert(bp);
    assert(str);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    rc = (int) bufPutBlk(bp, str, strlen(str));
    bp->endp[0] = '\0';
    return rc;
}
#endif


/*
    Add a block of data to the buf. Return the number of bytes added. Grow the buffer as required.
 */
PUBLIC ssize bufPutBlk(WebsBuf *bp, cchar *buf, ssize size)
{
    ssize this, added;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(buf);
    assert(0 <= size);

    /*
        Loop adding the maximum bytes we can add in a single straight line copy
     */
    added = 0;
    while (size > 0) {
        this = min(bufRoom(bp), size);
        if (this <= 0) {
            if (!bufGrow(bp, 0)) {
                break;
            }
            this = min(bufRoom(bp), size);
        }
        memcpy(bp->endp, buf, this);
        buf += this;
        bp->endp += this;
        size -= this;
        added += this;
        if (bp->endp >= bp->endbuf) {
            bp->endp = bp->buf;
        }
    }
    return added;
}


/*
    Get a block of data from the buf. Return the number of bytes returned.
 */
PUBLIC ssize bufGetBlk(WebsBuf *bp, char *buf, ssize size)
{
    ssize this, bytes_read;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(buf);
    assert(0 <= size && size < bp->buflen);

    /*
        Loop getting the maximum bytes we can get in a single straight line copy
     */
    bytes_read = 0;
    while (size > 0) {
        this = bufGetBlkMax(bp);
        this = min(this, size);
        if (this <= 0) {
            break;
        }
        memcpy(buf, bp->servp, this);
        buf += this;
        bp->servp += this;
        size -= this;
        bytes_read += this;

        if (bp->servp >= bp->endbuf) {
            bp->servp = bp->buf;
        }
    }
    return bytes_read;
}


/*
    Return the maximum number of bytes the buffer can accept via a single block copy. Useful if the user is doing their
    own data insertion.
 */
PUBLIC ssize bufRoom(WebsBuf *bp)
{
    ssize space, in_a_line;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    space = bp->buflen - RINGQ_LEN(bp) - 1;
    in_a_line = bp->endbuf - bp->endp;

    return min(in_a_line, space);
}


/*
    Return the maximum number of bytes the buffer can provide via a single block copy. Useful if the user is doing their
    own data retrieval.
 */
PUBLIC ssize bufGetBlkMax(WebsBuf *bp)
{
    ssize len, in_a_line;

    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));

    len = RINGQ_LEN(bp);
    in_a_line = bp->endbuf - bp->servp;

    return min(in_a_line, len);
}


/*
    Adjust the endp pointer after the user has copied data into the queue.
 */
PUBLIC void bufAdjustEnd(WebsBuf *bp, ssize size)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(0 <= size && size < bp->buflen);

    bp->endp += size;
    if (bp->endp >= bp->endbuf) {
        bp->endp -= bp->buflen;
    }
    /*
        Flush the queue if the endp pointer is corrupted via a bad size
     */
    if (bp->endp >= bp->endbuf) {
        error("Bad end pointer");
        bufFlush(bp);
    }
}


/*
    Adjust the servp pointer after the user has copied data from the queue.
 */
PUBLIC void bufAdjustStart(WebsBuf *bp, ssize size)
{
    assert(bp);
    assert(bp->buflen == (bp->endbuf - bp->buf));
    assert(0 <= size && size < bp->buflen);

    bp->servp += size;
    if (bp->servp >= bp->endbuf) {
        bp->servp -= bp->buflen;
    }
    /*
        Flush the queue if the servp pointer is corrupted via a bad size
     */
    if (bp->servp >= bp->endbuf) {
        error("Bad serv pointer");
        bufFlush(bp);
    }
}


/*
    Flush all data in a buffer. Reset the pointers.
 */
PUBLIC void bufFlush(WebsBuf *bp)
{
    assert(bp);
    assert(bp->servp);

    if (bp->buf) {
        bp->servp = bp->buf;
        bp->endp = bp->buf;
        if (bp->servp) {
            *bp->servp = '\0';
        }
    }
}


PUBLIC void bufCompact(WebsBuf *bp)
{
    ssize len;

    if (bp->buf) {
        if ((len = bufLen(bp)) > 0) {
            if (bp->servp < bp->endp && bp->servp > bp->buf) {
                bufAddNull(bp);
                memmove(bp->buf, bp->servp, len + 1);
                bp->endp -= bp->servp - bp->buf;
                bp->servp = bp->buf;
            }
        } else {
            bp->servp = bp->endp = bp->buf;
            *bp->servp = '\0';
        }
    }
}


/*
    Reset pointers if empty
 */
PUBLIC void bufReset(WebsBuf *bp)
{
    if (bp->buf && bp->servp == bp->endp && bp->servp > bp->buf) {
        bp->servp = bp->endp = bp->buf;
        *bp->servp = '\0';
    }
}


/*
    Grow the buffer. Return true if the buffer can be grown. Grow using the increment size specified when opening the
    buf. Don't grow beyond the maximum possible size.
 */
PUBLIC bool bufGrow(WebsBuf *bp, ssize room)
{
    char  *newbuf;
    ssize len;

    assert(bp);

    if (room == 0) {
        if (bp->maxsize >= 0 && bp->buflen >= bp->maxsize) {
#if BLD_DEBUG
            error("Cannot grow buf. Needed %d, max %d", room, bp->maxsize);
#endif
            return 0;
        }
        room = bp->increment;
        /*
            Double the increment so the next grow will line up with walloc'ed memory
         */
        bp->increment = getBinBlockSize(2 * bp->increment);
    }
    len = bufLen(bp);
    if ((newbuf = walloc(bp->buflen + room)) == NULL) {
        return 0;
    }
    bufGetBlk(bp, newbuf, bufLen(bp));
    wfree((char*) bp->buf);

    bp->buflen += room;
    bp->buf = newbuf;
    bp->endbuf = &bp->buf[bp->buflen];
    bp->servp = newbuf;
    bp->endp = &newbuf[len];
    return 1;
}


/*
    Find the smallest binary memory size that "size" will fit into.  This makes the buf and bufGrow routines much
    more efficient. The walloc routine likes powers of 2 minus 1.
 */
static int  getBinBlockSize(int size)
{
    int q;

    size = size >> WEBS_SHIFT;
    for (q = 0; size; size >>= 1) {
        q++;
    }
    return 1 << (WEBS_SHIFT + q);
}


WebsHash hashCreate(int size)
{
    WebsHash  sd;
    HashTable *tp;

    if (size < 0) {
        size = WEBS_SMALL_HASH;
    }
    assert(size > 2);

    /*
        Create a new handle for this symbol table
     */
    if ((sd = wallocHandle(&sym)) < 0) {
        return -1;
    }

    /*
        Create a new symbol table structure and zero
     */
    if ((tp = (HashTable*) walloc(sizeof(HashTable))) == NULL) {
        symMax = wfreeHandle(&sym, sd);
        return -1;
    }
    memset(tp, 0, sizeof(HashTable));
    if (sd >= symMax) {
        symMax = sd + 1;
    }
    assert(0 <= sd && sd < symMax);
    sym[sd] = tp;

    /*
        Now create the hash table for fast indexing.
     */
    tp->size = calcPrime(size);
    if ((tp->hash_table = (WebsKey**) walloc(tp->size * sizeof(WebsKey*))) == 0) {
        wfreeHandle(&sym, sd);
        wfree(tp);
        return -1;
    }
    assert(tp->hash_table);
    memset(tp->hash_table, 0, tp->size * sizeof(WebsKey*));
    return sd;
}


/*
    Close this symbol table. Call a cleanup function to allow the caller to free resources associated with each symbol
    table entry.
 */
PUBLIC void hashFree(WebsHash sd)
{
    HashTable *tp;
    WebsKey   *sp, *forw;
    int       i;

    if (sd < 0) {
        return;
    }
    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Free all symbols in the hash table, then the hash table itself.
     */
    for (i = 0; i < tp->size; i++) {
        for (sp = tp->hash_table[i]; sp; sp = forw) {
            forw = sp->forw;
            valueFree(&sp->name);
            valueFree(&sp->content);
            wfree((void*) sp);
            sp = forw;
        }
    }
    wfree((void*) tp->hash_table);
    symMax = wfreeHandle(&sym, sd);
    wfree((void*) tp);
}


/*
    Return the first symbol in the hashtable if there is one. This call is used as the first step in traversing the
    table. A call to hashFirst should be followed by calls to hashNext to get all the rest of the entries.
 */
WebsKey *hashFirst(WebsHash sd)
{
    HashTable *tp;
    WebsKey   *sp;
    int       i;

    assert(0 <= sd && sd < symMax);
    if (sd < 0 || sd >= symMax) {
        return 0;
    }
    tp = sym[sd];
    assert(tp);

    /*
        Find the first symbol in the hashtable and return a pointer to it.
     */
    for (i = 0; i < tp->size; i++) {
        if ((sp = tp->hash_table[i]) != 0) {
            return sp;
        }
    }
    return 0;
}


/*
    Return the next symbol in the hashtable if there is one. See hashFirst.
 */
WebsKey *hashNext(WebsHash sd, WebsKey *last)
{
    HashTable *tp;
    WebsKey   *sp;
    int       i;

    assert(0 <= sd && sd < symMax);
    if (sd < 0) {
        return 0;
    }
    tp = sym[sd];
    assert(tp);
    if (last == 0) {
        return hashFirst(sd);
    }
    if (last->forw) {
        return last->forw;
    }
    for (i = last->bucket + 1; i < tp->size; i++) {
        if ((sp = tp->hash_table[i]) != 0) {
            return sp;
        }
    }
    return NULL;
}


/*
    Lookup a symbol and return a pointer to the symbol entry. If not present then return a NULL.
 */
WebsKey *hashLookup(WebsHash sd, cchar *name)
{
    HashTable *tp;
    WebsKey   *sp;
    char      *cp;

    assert(0 <= sd && sd < symMax);
    if (sd < 0 || (tp = sym[sd]) == NULL) {
        return NULL;
    }
    if (name == NULL || *name == '\0') {
        return NULL;
    }
    /*
        Do an initial hash and then follow the link chain to find the right entry
     */
    for (sp = hash(tp, name); sp; sp = sp->forw) {
        cp = sp->name.value.string;
        if (cp[0] == name[0] && strcmp(cp, name) == 0) {
            break;
        }
    }
    return sp;
}


void *hashLookupSymbol(WebsHash sd, cchar *name)
{
    WebsKey *kp;

    if ((kp = hashLookup(sd, name)) == 0) {
        return 0;
    }
    return kp->content.value.symbol;
}


/*
    Enter a symbol into the table. If already there, update its value.  Always succeeds if memory available. We allocate
    a copy of "name" here so it can be a volatile variable. The value "v" is just a copy of the passed in value, so it
    MUST be persistent.
 */
WebsKey *hashEnter(WebsHash sd, cchar *name, WebsValue v, int arg)
{
    HashTable *tp;
    WebsKey   *sp, *last;
    char      *cp;
    int       hindex;

    assert(name);
    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Calculate the first daisy-chain from the hash table. If non-zero, then we have daisy-chain, so scan it and look
        for the symbol.
     */
    last = NULL;
    hindex = hashIndex(tp, name);
    if ((sp = tp->hash_table[hindex]) != NULL) {
        for (; sp; sp = sp->forw) {
            cp = sp->name.value.string;
            if (cp[0] == name[0] && strcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
        if (sp) {
            /*
                Found, so update the value If the caller stores handles which require freeing, they will be lost here.
                It is the callers responsibility to free resources before overwriting existing contents. We will here
                free allocated strings which occur due to value_instring().  We should consider providing the cleanup
                function on the open rather than the close and then we could call it here and solve the problem.
             */
            if (sp->content.valid) {
                valueFree(&sp->content);
            }
            sp->content = v;
            sp->arg = arg;
            return sp;
        }
        /*
            Not found so allocate and append to the daisy-chain
         */
        if ((sp = (WebsKey*) walloc(sizeof(WebsKey))) == 0) {
            return NULL;
        }
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->content = v;
        sp->forw = (WebsKey*) NULL;
        sp->arg = arg;
        sp->bucket = hindex;
        last->forw = sp;

    } else {
        /*
            Daisy chain is empty so we need to start the chain
         */
        if ((sp = (WebsKey*) walloc(sizeof(WebsKey))) == 0) {
            return NULL;
        }
        tp->hash_table[hindex] = sp;
        tp->hash_table[hashIndex(tp, name)] = sp;

        sp->forw = (WebsKey*) NULL;
        sp->content = v;
        sp->arg = arg;
        sp->name = valueString(name, VALUE_ALLOCATE);
        sp->bucket = hindex;
    }
    return sp;
}


/*
    Delete a symbol from a table
 */
PUBLIC int hashDelete(WebsHash sd, cchar *name)
{
    HashTable *tp;
    WebsKey   *sp, *last;
    char      *cp;
    int       hindex;

    assert(name && *name);
    assert(0 <= sd && sd < symMax);
    tp = sym[sd];
    assert(tp);

    /*
        Calculate the first daisy-chain from the hash table. If non-zero, then we have daisy-chain, so scan it and look
        for the symbol.
     */
    last = NULL;
    hindex = hashIndex(tp, name);
    if ((sp = tp->hash_table[hindex]) != NULL) {
        for ( ; sp; sp = sp->forw) {
            cp = sp->name.value.string;
            if (cp[0] == name[0] && strcmp(cp, name) == 0) {
                break;
            }
            last = sp;
        }
    }
    if (sp == (WebsKey*) NULL) {              /* Not Found */
        return -1;
    }
    /*
         Unlink and free the symbol. Last will be set if the element to be deleted is not first in the chain.
     */
    if (last) {
        last->forw = sp->forw;
    } else {
        tp->hash_table[hindex] = sp->forw;
    }
    valueFree(&sp->name);
    valueFree(&sp->content);
    wfree((void*) sp);
    return 0;
}


/*
    Hash a symbol and return a pointer to the hash daisy-chain list. All symbols reside on the chain (ie. none stored in
    the hash table itself)
 */
static WebsKey *hash(HashTable *tp, cchar *name)
{
    assert(tp);

    return tp->hash_table[hashIndex(tp, name)];
}


/*
    Compute the hash function and return an index into the hash table We use a basic additive function that is then made
    modulo the size of the table.
 */
static int hashIndex(HashTable *tp, cchar *name)
{
    uint sum;
    int  i;

    assert(tp);
    /*
        Add in each character shifted up progressively by 7 bits. The shift amount is rounded so as to not shift too
        far. It thus cycles with each new cycle placing character shifted up by one bit.
     */
    i = 0;
    sum = 0;
    while (*name) {
        sum += (((int) *name++) << i);
        i = (i + 7) % (BITS(int) - BITSPERBYTE);
    }
    return sum % tp->size;
}


/*
    Check if this number is a prime
 */
static int isPrime(int n)
{
    int i, max;

    assert(n > 0);

    max = n / 2;
    for (i = 2; i <= max; i++) {
        if (n % i == 0) {
            return 0;
        }
    }
    return 1;
}


/*
    Calculate the largest prime smaller than size.
 */
static int calcPrime(int size)
{
    int count;

    assert(size > 0);

    for (count = size; count > 0; count--) {
        if (isPrime(count)) {
            return count;
        }
    }
    return 1;
}


/*
    Convert a hex string to an integer
 */
uint hextoi(cchar *hexstring)
{
    cchar *h;
    uint  c, v;

    if (!hexstring) {
        return 0;
    }
    v = 0;
    h = hexstring;
    if (*h == '0' && (*(h + 1) == 'x' || *(h + 1) == 'X')) {
        h += 2;
    }
    while ((c = (uint) * h++) != 0) {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'f') {
            c = (c - 'a') + 10;
        } else if (c >=  'A' && c <= 'F') {
            c = (c - 'A') + 10;
        } else {
            break;
        }
        v = (v * 0x10) + c;
    }
    return v;
}


PUBLIC char *sclone(cchar *s)
{
    char  *buf;
    ssize len;

    if (s == NULL) {
        s = "";
    }
    len = strlen(s) + 1;
    if ((buf = walloc(len)) != 0) {
        scopy(buf, len, s);
    }
    return buf;
}



/*
    Clone a sub-string of a specified length. The null is added after the length. The given len can be longer than the
    source string.
 */
PUBLIC char *snclone(cchar *str, ssize len)
{
    char  *ptr;
    ssize size, l;

    if (str == 0) {
        str = "";
    }
    l = slen(str);
    len = min(l, len);
    size = len + 1;
    if ((ptr = walloc(size)) != 0) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
    }
    return ptr;
}


PUBLIC bool snumber(cchar *s)
{
    if (!s) {
        return 0;
    }
    if (*s == '-' || *s == '+') {
        s++;
    }
    return s && *s && strspn(s, "1234567890") == strlen(s);
}


/*
    Convert a string to an integer. If the string starts with "0x" or "0X" a hexadecimal conversion is done.
 */
uint strtoi(char *s)
{
    if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')) {
        s += 2;
        return hextoi(s);
    }
    return atoi(s);
}


PUBLIC int scaselesscmp(cchar *s1, cchar *s2)
{
    if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncaselesscmp(s1, s2, max(slen(s1), slen(s2)));
}


PUBLIC bool scaselessmatch(cchar *s1, cchar *s2)
{
    return scaselesscmp(s1, s2) == 0;
}


PUBLIC bool smatch(cchar *s1, cchar *s2)
{
    return scmp(s1, s2) == 0;
}


PUBLIC bool sstarts(cchar *str, cchar *prefix)
{
    if (str == 0 || prefix == 0) {
        return 0;
    }
    if (strncmp(str, prefix, slen(prefix)) == 0) {
        return 1;
    }
    return 0;
}


PUBLIC int scmp(cchar *s1, cchar *s2)
{
    if (s1 == s2) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncmp(s1, s2, max(slen(s1), slen(s2)));
}


PUBLIC ssize slen(cchar *s)
{
    return s ? strlen(s) : 0;
}


PUBLIC ssize scopy(char *dest, ssize destMax, cchar *src)
{
    ssize len;

    assert(src);
    assert(dest);
    assert(0 < dest && destMax < MAXINT);

    len = slen(src);
    if (destMax <= len) {
        if (destMax > 0) {
            *dest = '\0';
        }
        return -1;
    }
    strcpy(dest, src);
    return len;
}


/*
    This routine copies at most "count" characters from a string. It ensures the result is always null terminated and
    the buffer does not overflow. Returns -1 if the buffer is too small.
 */
PUBLIC ssize sncopy(char *dest, ssize destMax, cchar *src, ssize count)
{
    ssize len;

    assert(dest);
    assert(src);
    assert(src != dest);
    assert(0 <= count && count < MAXINT);
    assert(0 < destMax && destMax < MAXINT);

    //  OPT need snlen(src, count);
    len = slen(src);
    len = min(len, count);
    if (destMax <= len) {
        if (destMax > 0) {
            *dest = '\0';
        }
        return -1;
    }
    if (len > 0) {
        memcpy(dest, src, len);
        dest[len] = '\0';
    } else {
        *dest = '\0';
        len = 0;
    }
    return len;
}


#if KEEP
/*
    Return the length of a string limited by a given length
 */
PUBLIC ssize strnlen(cchar *s, ssize n)
{
    ssize len;

    len = strlen(s);
    return min(len, n);
}
#endif


/*
    Case sensitive string comparison. Limited by length
 */
PUBLIC int sncmp(cchar *s1, cchar *s2, ssize n)
{
    int rc;

    assert(0 <= n && n < MAXINT);

    if (s1 == 0 && s2 == 0) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = *s1 - *s2;
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}


PUBLIC int sncaselesscmp(cchar *s1, cchar *s2, ssize n)
{
    int rc;

    assert(0 <= n && n < MAXINT);

    if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = tolower((uchar) * s1) - tolower((uchar) * s2);
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}


/*
    Split a string at a delimiter and return the parts.
    This differs from stok in that it never returns null. Also, stok eats leading deliminators, whereas
    ssplit will return an empty string if there are leading deliminators.
    Note: Modifies the original string and returns the string for chaining.
 */
PUBLIC char *ssplit(char *str, cchar *delim, char **last)
{
    char *end;

    if (last) {
        *last = "";
    }
    if (str == 0) {
        return "";
    }
    if (delim == 0 || *delim == '\0') {
        return str;
    }
    if ((end = strpbrk(str, delim)) != 0) {
        *end++ = '\0';
        end += strspn(end, delim);
    } else {
        end = "";
    }
    if (last) {
        *last = end;
    }
    return str;
}


/*
    Note "str" is modified as per strtok()
    WARNING:  this does not allocate
 */
PUBLIC char *stok(char *str, cchar *delim, char **last)
{
    char  *start, *end;
    ssize i;

    start = str ? str : *last;
    if (start == 0) {
        *last = 0;
        return 0;
    }
    i = strspn(start, delim);
    start += i;
    if (*start == '\0') {
        *last = 0;
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        *end++ = '\0';
        i = strspn(end, delim);
        end += i;
    }
    *last = end;
    return start;
}


PUBLIC char *strim(char *str, cchar *set, int where)
{
    char  *s;
    ssize len, i;

    if (str == 0 || set == 0) {
        return 0;
    }
    if (where & WEBS_TRIM_START) {
        i = strspn(str, set);
    } else {
        i = 0;
    }
    s = (char*) &str[i];
    if (where & WEBS_TRIM_END) {
        len = strlen(s);
        while (len > 0 && strspn(&s[len - 1], set) > 0) {
            s[len - 1] = '\0';
            len--;
        }
    }
    return s;
}


/*
    Parse the args and return the count of args. If argv is NULL, the args are parsed read-only. If argv is set,
    then the args will be extracted, back-quotes removed and argv will be set to point to all the args.
    NOTE: this routine does not allocate.
 */
PUBLIC int websParseArgs(char *args, char **argv, int maxArgc)
{
    char *dest, *src, *start;
    int  quote, argc;

    /*
        Example     "showColors" red 'light blue' "yellow white" 'Cannot \"render\"'
        Becomes:    ["showColors", "red", "light blue", "yellow white", "Cannot \"render\""]
     */
    for (argc = 0, src = args; src && *src != '\0' && argc < maxArgc; argc++) {
        while (isspace((uchar) * src)) {
            src++;
        }
        if (*src == '\0') {
            break;
        }
        start = dest = src;
        if (*src == '"' || *src == '\'') {
            quote = *src;
            src++;
            dest++;
        } else {
            quote = 0;
        }
        if (argv) {
            argv[argc] = src;
        }
        while (*src) {
            if (*src == '\\' && src[1] && (src[1] == '\\' || src[1] == '"' || src[1] == '\'')) {
                src++;
            } else {
                if (quote) {
                    if (*src == quote && !(src > start && src[-1] == '\\')) {
                        break;
                    }
                } else if (*src == ' ') {
                    break;
                }
            }
            if (argv) {
                *dest++ = *src;
            }
            src++;
        }
        if (*src != '\0') {
            src++;
        }
        if (argv) {
            *dest++ = '\0';
        }
    }
    return argc;
}


#if ME_GOAHEAD_LEGACY
PUBLIC int fmtValloc(char **sp, int n, cchar *format, va_list args)
{
    *sp = sfmtv(format, args);
    return (int) slen(*sp);
}


PUBLIC int fmtAlloc(char **sp, int n, cchar *format, ...)
{
    va_list args;

    va_start(args, format);
    *sp = sfmtv(format, args);
    va_end(args);
    return (int) slen(*sp);
}
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
