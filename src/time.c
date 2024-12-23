/**
    time.c - Date and Time handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************** Defines ***********************************/

#define SEC_PER_MIN  (60)
#define SEC_PER_HOUR (60 * 60)
#define SEC_PER_DAY  (86400)
#define SEC_PER_YEAR (INT64(31556952))

/*
    Token types
 */
#define TOKEN_DAY       0x01000000
#define TOKEN_MONTH     0x02000000
#define TOKEN_ZONE      0x04000000
#define TOKEN_OFFSET    0x08000000

typedef struct TimeToken {
    char    *name;
    int     value;
    int     type;
} TimeToken;

static WebsHash timeTokens = -1;

static TimeToken days[] = {
    { "sun",  0, TOKEN_DAY },
    { "mon",  1, TOKEN_DAY },
    { "tue",  2, TOKEN_DAY },
    { "wed",  3, TOKEN_DAY },
    { "thu",  4, TOKEN_DAY },
    { "fri",  5, TOKEN_DAY },
    { "sat",  6, TOKEN_DAY },
    { 0, 0 },
 };

static TimeToken fullDays[] = {
    { "sunday",     0, TOKEN_DAY },
    { "monday",     1, TOKEN_DAY },
    { "tuesday",    2, TOKEN_DAY },
    { "wednesday",  3, TOKEN_DAY },
    { "thursday",   4, TOKEN_DAY },
    { "friday",     5, TOKEN_DAY },
    { "saturday",   6, TOKEN_DAY },
    { 0, 0 },
 };

/*
    Make origin 1 to correspond to user date entries 10/28/2014
 */
static TimeToken months[] = {
    { "jan",  1, TOKEN_MONTH },
    { "feb",  2, TOKEN_MONTH },
    { "mar",  3, TOKEN_MONTH },
    { "apr",  4, TOKEN_MONTH },
    { "may",  5, TOKEN_MONTH },
    { "jun",  6, TOKEN_MONTH },
    { "jul",  7, TOKEN_MONTH },
    { "aug",  8, TOKEN_MONTH },
    { "sep",  9, TOKEN_MONTH },
    { "oct", 10, TOKEN_MONTH },
    { "nov", 11, TOKEN_MONTH },
    { "dec", 12, TOKEN_MONTH },
    { 0, 0 },
 };

static TimeToken fullMonths[] = {
    { "january",    1, TOKEN_MONTH },
    { "february",   2, TOKEN_MONTH },
    { "march",      3, TOKEN_MONTH },
    { "april",      4, TOKEN_MONTH },
    { "may",        5, TOKEN_MONTH },
    { "june",       6, TOKEN_MONTH },
    { "july",       7, TOKEN_MONTH },
    { "august",     8, TOKEN_MONTH },
    { "september",  9, TOKEN_MONTH },
    { "october",   10, TOKEN_MONTH },
    { "november",  11, TOKEN_MONTH },
    { "december",  12, TOKEN_MONTH },
    { 0, 0 }
 };

static TimeToken ampm[] = {
    { "am", 0, TOKEN_OFFSET },
    { "pm", (12 * 3600), TOKEN_OFFSET },
    { 0, 0 },
 };


static TimeToken zones[] = {
    { "ut",      0, TOKEN_ZONE },
    { "utc",     0, TOKEN_ZONE },
    { "gmt",     0, TOKEN_ZONE },
    { "edt",  -240, TOKEN_ZONE },
    { "est",  -300, TOKEN_ZONE },
    { "cdt",  -300, TOKEN_ZONE },
    { "cst",  -360, TOKEN_ZONE },
    { "mdt",  -360, TOKEN_ZONE },
    { "mst",  -420, TOKEN_ZONE },
    { "pdt",  -420, TOKEN_ZONE },
    { "pst",  -480, TOKEN_ZONE },
    { 0, 0 },
 };


static TimeToken offsets[] = {
    { "tomorrow",    86400, TOKEN_OFFSET },
    { "yesterday",  -86400, TOKEN_OFFSET },
    { "next week",  (86400 * 7), TOKEN_OFFSET },
    { "last week", -(86400 * 7), TOKEN_OFFSET },
    { 0, 0 },
};

static int timeSep = ':';

static int normalMonthStart[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 0,
};
static int leapMonthStart[] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 0
};

static int leapYear(int year);
static void validateTime(struct tm *tm, struct tm *defaults);

/************************************ Code ************************************/

PUBLIC int websTimeOpen(void)
{
    TimeToken           *tt;

    timeTokens = hashCreate(59);
    for (tt = days; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = fullDays; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = months; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = fullMonths; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = ampm; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = zones; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    for (tt = offsets; tt->name; tt++) {
        hashEnter(timeTokens, tt->name, valueSymbol(tt), 0);
    }
    return 0;
}


PUBLIC void websTimeClose(void)
{
    if (timeTokens >= 0) {
        hashFree(timeTokens);
        timeTokens = -1;
    }
}


static int leapYear(int year)
{
    if (year % 4) {
        return 0;
    } else if (year % 400 == 0) {
        return 1;
    } else if (year % 100 == 0) {
        return 0;
    }
    return 1;
}


static int daysSinceEpoch(int year)
{
    int     days;

    days = 365 * (year - 1970);
    days += ((year-1) / 4) - (1970 / 4);
    days -= ((year-1) / 100) - (1970 / 100);
    days += ((year-1) / 400) - (1970 / 400);
    return days;
}


static WebsTime makeTime(struct tm *tp)
{
    int     days, year, month;

    year = tp->tm_year + 1900 + tp->tm_mon / 12;
    month = tp->tm_mon % 12;
    if (month < 0) {
        month += 12;
        --year;
    }
    days = daysSinceEpoch(year);
    days += leapYear(year) ? leapMonthStart[month] : normalMonthStart[month];
    days += tp->tm_mday - 1;
    return (days * SEC_PER_DAY) + ((((((tp->tm_hour * 60)) + tp->tm_min) * 60) + tp->tm_sec));
}


static int lookupSym(char *token, int kind)
{
    TimeToken   *tt;

    if ((tt = (TimeToken*) hashLookupSymbol(timeTokens, token)) == 0) {
        return -1;
    }
    if (kind != tt->type) {
        return -1;
    }
    return tt->value;
}


static int getNum(char **token, int sep)
{
    int     num;

    if (*token == 0) {
        return 0;
    }

    num = atoi(*token);
    *token = strchr(*token, sep);
    if (*token) {
        *token += 1;
    }
    return num;
}


static int getNumOrSym(char **token, int sep, int kind, int *isAlpah)
{
    char    *cp;
    int     num;

    assert(token && *token);

    if (token == NULL || *token == NULL || **token == '\0') {
        return 0;
    }
    if (isalpha((uchar) **token)) {
        *isAlpah = 1;
        cp = strchr(*token, sep);
        if (cp) {
            *cp++ = '\0';
        }
        num = lookupSym(*token, kind);
        *token = cp;
        return num;
    }
    num = atoi(*token);
    *token = strchr(*token, sep);
    if (*token) {
        *token += 1;
    }
    *isAlpah = 0;
    return num;
}


static void swapDayMonth(struct tm *tp)
{
    int     tmp;

    tmp = tp->tm_mday;
    tp->tm_mday = tp->tm_mon;
    tp->tm_mon = tmp;
}


/*
    Parse the a date/time string and return the result in *time. Missing date items may be provided
    via the defaults argument. This is a tolerant parser. It is not validating and will do its best
    to parse any possible date string.
 */
PUBLIC int websParseDateTime(WebsTime *time, cchar *dateString, struct tm *defaults)
{
    TimeToken       *tt;
    struct tm       tm;
    char            *str, *next, *token, *cp, *sep;
    int64           value;
    int             kind, hour, min, negate, value1, value2, value3, alpha, alpha2, alpha3;
    int             dateSep, offset, zoneOffset, fullYear;

    if (!dateString) {
        dateString = "";
    }
    offset = 0;
    zoneOffset = 0;
    sep = ", \t";
    cp = 0;
    next = 0;
    fullYear = 0;

    /*
        Set these mandatory values to -1 so we can tell if they are set to valid values
        WARNING: all the calculations use tm_year with origin 0, not 1900. It is fixed up below.
     */
    tm.tm_year = -MAXINT;
    tm.tm_mon = tm.tm_mday = tm.tm_hour = tm.tm_sec = tm.tm_min = tm.tm_wday = -1;
    tm.tm_min = tm.tm_sec = tm.tm_yday = -1;

#if ME_UNIX_LIKE && !CYGWIN
    tm.tm_gmtoff = 0;
    tm.tm_zone = 0;
#endif

    /*
        Set to -1 to try to determine if DST is in effect
     */
    tm.tm_isdst = -1;
    str = slower(sclone(dateString));

    /*
        Handle ISO dates: 2009-05-21t16:06:05.000z
     */
    if (strchr(str, ' ') == 0 && strchr(str, '-') && str[slen(str) - 1] == 'z') {
        for (cp = str; *cp; cp++) {
            if (*cp == '-') {
                *cp = '/';
            } else if (*cp == 't' && cp > str && isdigit((uchar) cp[-1]) && isdigit((uchar) cp[1]) ) {
                *cp = ' ';
            }
        }
    }
    token = stok(str, sep, &next);

    while (token && *token) {
        if (snumber(token)) {
            /*
                Parse either day of month or year. Priority to day of month. Format: <29> Jan <15> <2014>
             */
            value = atoi(token);
            if (value > 3000) {
                *time = value;
                wfree(str);
                return 0;
            } else if (value > 32 || (tm.tm_mday >= 0 && tm.tm_year == -MAXINT)) {
                if (value >= 1000) {
                    fullYear = 1;
                }
                tm.tm_year = (int) value - 1900;
            } else if (tm.tm_mday < 0) {
                tm.tm_mday = (int) value;
            }

        } else if (strncmp(token, "gmt", 3) == 0 || strncmp(token, "utc", 3) == 0) {
            /*
                Timezone format: GMT|UTC[+-]NN[:]NN
                GMT-08:00, UTC-0800
             */
            token += 3;
            if (token[0] == '-' || token[0] == '+') {
                negate = *token == '-' ? -1 : 1;
                token++;
            } else {
                negate = 1;
            }
            if (strchr(token, ':')) {
                hour = getNum(&token, timeSep);
                min = getNum(&token, timeSep);
            } else {
                hour = atoi(token);
                min = hour % 100;
                hour /= 100;
            }
            zoneOffset = negate * (hour * 60 + min);

        } else if (isalpha((uchar) *token)) {
            if ((tt = (TimeToken*) hashLookupSymbol(timeTokens, token)) != 0) {
                kind = tt->type;
                value = tt->value;
                switch (kind) {

                case TOKEN_DAY:
                    tm.tm_wday = (int) value;
                    break;

                case TOKEN_MONTH:
                    tm.tm_mon = (int) value;
                    break;

                case TOKEN_OFFSET:
                    /* Named timezones or symbolic names like: tomorrow, yesterday, next week ... */
                    /* Units are seconds */
                    offset += (int) value;
                    break;

                case TOKEN_ZONE:
                    zoneOffset = (int) value;
                    break;

                default:
                    /* Just ignore unknown values */
                    break;
                }
            }

        } else if ((cp = strchr(token, timeSep)) != 0 && isdigit((uchar) token[0])) {
            /*
                Time:  10:52[:23]
                Must not parse GMT-07:30
             */
            tm.tm_hour = getNum(&token, timeSep);
            tm.tm_min = getNum(&token, timeSep);
            tm.tm_sec = getNum(&token, timeSep);

        } else {
            dateSep = '/';
            if (strchr(token, dateSep) == 0) {
                dateSep = '-';
                if (strchr(token, dateSep) == 0) {
                    dateSep = '.';
                    if (strchr(token, dateSep) == 0) {
                        dateSep = 0;
                    }
                }
            }
            if (dateSep) {
                /*
                    Date:  07/28/2014, 07/28/08, Jan/28/2014, Jaunuary-28-2014, 28-jan-2014
                    Support order: dd/mm/yy, mm/dd/yy and yyyy/mm/dd
                    Support separators "/", ".", "-"
                 */
                value1 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha);
                value2 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha2);
                value3 = getNumOrSym(&token, dateSep, TOKEN_MONTH, &alpha3);

                if (value1 > 31) {
                    /* yy/mm/dd */
                    tm.tm_year = value1;
                    tm.tm_mon = value2;
                    tm.tm_mday = value3;

                } else if (value1 > 12 || alpha2) {
                    /*
                        dd/mm/yy
                        Cannot detect 01/02/03  This will be evaluated as Jan 2 2003 below.
                     */
                    tm.tm_mday = value1;
                    tm.tm_mon = value2;
                    tm.tm_year = value3;

                } else {
                    /*
                        The default to parse is mm/dd/yy unless the mm value is out of range
                     */
                    tm.tm_mon = value1;
                    tm.tm_mday = value2;
                    tm.tm_year = value3;
                }
            }
        }
        token = stok(NULL, sep, &next);
    }

    /*
        Y2K fix and rebias
     */
    if (0 <= tm.tm_year && tm.tm_year < 100 && !fullYear) {
        if (tm.tm_year < 50) {
            tm.tm_year += 2000;
        } else {
            tm.tm_year += 1900;
        }
    }
    if (tm.tm_year >= 1900) {
        tm.tm_year -= 1900;
    }

    /*
        Convert back to origin 0 for months
     */
    if (tm.tm_mon > 0) {
        tm.tm_mon--;
    }

    /*
        Validate and fill in missing items with defaults
     */
    validateTime(&tm, defaults);
    *time = makeTime(&tm);
    *time += -(zoneOffset * SEC_PER_MIN);
    *time += offset;
    wfree(str);
    return 0;
}


static void validateTime(struct tm *tp, struct tm *defaults)
{
    struct tm   empty;

    /*
        Fix apparent day-mon-year ordering issues. Cannot fix everything!
     */
    if ((12 <= tp->tm_mon && tp->tm_mon <= 31) && 0 <= tp->tm_mday && tp->tm_mday <= 11) {
        /*
            Looks like day month are swapped
         */
        swapDayMonth(tp);
    }

    /*
        Check for overflow. Underflow validated below.
        tm_sec can be 61 for leap seconds.
     */
    if (tp->tm_sec > 61) {
        tp->tm_sec = -1;
    }
    if (tp->tm_min > 59) {
        tp->tm_sec = -1;
    }
    if (tp->tm_hour > 23) {
        tp->tm_sec = -1;
    }
    if (tp->tm_mday > 31) {
        tp->tm_sec = -1;
    }
    if (tp->tm_mon > 11) {
        tp->tm_sec = -1;
    }
    if (tp->tm_wday > 6) {
        tp->tm_sec = -1;
    }
    if (tp->tm_yday > 366) {
        tp->tm_sec = -1;
    }

#if UNUSED
    if (tp->tm_year != -MAXINT && tp->tm_mon >= 0 && tp->tm_mday >= 0 && tp->tm_hour >= 0) {
        /*  Everything defined */
        return;
    }
#endif

    /*
        Use empty time if missing
     */
    if (defaults == NULL) {
        memset(&empty, 0, sizeof(empty));
        defaults = &empty;
        empty.tm_mday = 1;
        empty.tm_year = 70;
    }
    if (tp->tm_hour < 0 && tp->tm_min < 0 && tp->tm_sec < 0) {
        tp->tm_hour = defaults->tm_hour;
        tp->tm_min = defaults->tm_min;
        tp->tm_sec = defaults->tm_sec;
    }

    /*
        Get weekday, if before today then make next week
     */
    if (tp->tm_wday >= 0 && tp->tm_year == -MAXINT && tp->tm_mon < 0 && tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday + (tp->tm_wday - defaults->tm_wday + 7) % 7;
        tp->tm_mon = defaults->tm_mon;
        tp->tm_year = defaults->tm_year;
    }

    /*
        Get month, if before this month then make next year
     */
    if (tp->tm_mon >= 0 && tp->tm_mon <= 11 && tp->tm_mday < 0) {
        if (tp->tm_year == -MAXINT) {
            tp->tm_year = defaults->tm_year + (((tp->tm_mon - defaults->tm_mon) < 0) ? 1 : 0);
        }
        tp->tm_mday = defaults->tm_mday;
    }

    /*
        Get date, if before current time then make tomorrow
     */
    if (tp->tm_hour >= 0 && tp->tm_year == -MAXINT && tp->tm_mon < 0 && tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday + ((tp->tm_hour - defaults->tm_hour) < 0 ? 1 : 0);
        tp->tm_mon = defaults->tm_mon;
        tp->tm_year = defaults->tm_year;
    }
    if (tp->tm_year == -MAXINT) {
        tp->tm_year = defaults->tm_year;
    }
    if (tp->tm_mon < 0) {
        tp->tm_mon = defaults->tm_mon;
    }
    if (tp->tm_mday < 0) {
        tp->tm_mday = defaults->tm_mday;
    }
    if (tp->tm_yday < 0) {
        if (tp->tm_mon <= 11) {
            tp->tm_yday = (leapYear(tp->tm_year + 1900) ?
                leapMonthStart[tp->tm_mon] : normalMonthStart[tp->tm_mon]) + tp->tm_mday - 1;
        } else {
            tp->tm_yday = defaults->tm_yday;
        }
    }
    if (tp->tm_hour < 0) {
        tp->tm_hour = defaults->tm_hour;
    }
    if (tp->tm_min < 0) {
        tp->tm_min = defaults->tm_min;
    }
    if (tp->tm_sec < 0) {
        tp->tm_sec = defaults->tm_sec;
    }
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
