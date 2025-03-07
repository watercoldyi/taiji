#include "datetime.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

typedef struct DateTime DateTime;
struct DateTime
{
    int64_t iJD;            /* The julian day number times 86400000 */
    int Y, M, D;            /* Year, month, and day */
    int h, m;               /* Hour and minutes */
    int tz;                 /* Timezone offset in minutes */
    double s;               /* Seconds */
    char validJD;           /* True (1) if iJD is valid */
    char validYMD;          /* True (1) if Y,M,D are valid */
    char validHMS;          /* True (1) if h,m,s are valid */
    char nFloor;            /* Days to implement "floor" */
    unsigned rawS : 1;      /* Raw numeric value stored in s */
    unsigned isError : 1;   /* An overflow has occurred */
    unsigned useSubsec : 1; /* Display subsecond precision */
    unsigned isUtc : 1;     /* Time is known to be UTC */
    unsigned isLocal : 1;   /* Time is known to be localtime */
};

static int currentTimeInt64(int64_t *piNow)
{
    static const int64_t unixEpoch = 24405875 * (int64_t)8640000;
    int rc = 0;
    struct timeval sNow;
    (void)gettimeofday(&sNow, 0); /* Cannot fail given valid arguments */
    *piNow = unixEpoch + 1000 * (int64_t)sNow.tv_sec + sNow.tv_usec / 1000;
    return rc;
}

static void datetimeError(DateTime *p)
{
    memset(p, 0, sizeof(*p));
    p->isError = 1;
}

static void computeJD(DateTime *p)
{
    int Y, M, D, A, B, X1, X2;

    if (p->validJD)
        return;
    if (p->validYMD)
    {
        Y = p->Y;
        M = p->M;
        D = p->D;
    }
    else
    {
        Y = 2000; /* If no YMD specified, assume 2000-Jan-01 */
        M = 1;
        D = 1;
    }
    if (Y < -4713 || Y > 9999 || p->rawS)
    {
        datetimeError(p);
        return;
    }
    if (M <= 2)
    {
        Y--;
        M += 12;
    }
    A = (Y + 4800) / 100;
    B = 38 - A + (A / 4);
    X1 = 36525 * (Y + 4716) / 100;
    X2 = 306001 * (M + 1) / 10000;
    p->iJD = (int64_t)((X1 + X2 + D + B - 1524.5) * 86400000);
    p->validJD = 1;
    if (p->validHMS)
    {
        p->iJD += p->h * 3600000 + p->m * 60000 + (int64_t)(p->s * 1000 + 0.5);
        if (p->tz)
        {
            p->iJD -= p->tz * 60000;
            p->validYMD = 0;
            p->validHMS = 0;
            p->tz = 0;
            p->isUtc = 1;
            p->isLocal = 0;
        }
    }
}

#define INT_464269060799999 ((((int64_t)0x1a640) << 32) | 0x1072fdff)

static int validJulianDay(int64_t iJD)
{
    return iJD >= 0 && iJD <= INT_464269060799999;
}

/*
** Clear the YMD and HMS and the TZ
*/
static void clearYMD_HMS_TZ(DateTime *p)
{
    p->validYMD = 0;
    p->validHMS = 0;
    p->tz = 0;
}

static int setDateTimeToCurrent(DateTime *p)
{
    (void)currentTimeInt64(&p->iJD);
    if (p->iJD > 0)
    {
        p->validJD = 1;
        p->isUtc = 1;
        p->isLocal = 0;
        clearYMD_HMS_TZ(p);
        return 0;
    }
    else
    {
        return 1;
    }
}

/*
** Input "r" is a numeric quantity which might be a julian day number,
** or the number of seconds since 1970.  If the value if r is within
** range of a julian day number, install it as such and set validJD.
** If the value is a valid unix timestamp, put it in p->s and set p->rawS.
*/
static void setRawDateNumber(DateTime *p, double r)
{
    p->s = r;
    p->rawS = 1;
    if (r >= 0.0 && r < 5373484.5)
    {
        p->iJD = (int64_t)(r * 86400000.0 + 0.5);
        p->validJD = 1;
    }
}

/*
** Given the YYYY-MM-DD information current in p, determine if there
** is day-of-month overflow and set nFloor to the number of days that
** would need to be subtracted from the date in order to bring the
** date back to the end of the month.
*/
static void computeFloor(DateTime *p)
{
    assert(p->validYMD || p->isError);
    assert(p->D >= 0 && p->D <= 31);
    assert(p->M >= 0 && p->M <= 12);
    if (p->D <= 28)
    {
        p->nFloor = 0;
    }
    else if ((1 << p->M) & 0x15aa)
    {
        p->nFloor = 0;
    }
    else if (p->M != 2)
    {
        p->nFloor = (p->D == 31);
    }
    else if (p->Y % 4 != 0 || (p->Y % 100 == 0 && p->Y % 400 != 0))
    {
        p->nFloor = p->D - 28;
    }
    else
    {
        p->nFloor = p->D - 29;
    }
}

/*
** Compute the Year, Month, and Day from the julian day number.
*/
static void computeYMD(DateTime *p)
{
    int Z, alpha, A, B, C, D, E, X1;
    if (p->validYMD)
        return;
    if (!p->validJD)
    {
        p->Y = 2000;
        p->M = 1;
        p->D = 1;
    }
    else if (!validJulianDay(p->iJD))
    {
        datetimeError(p);
        return;
    }
    else
    {
        Z = (int)((p->iJD + 43200000) / 86400000);
        alpha = (int)((Z + 32044.75) / 36524.25) - 52;
        A = Z + 1 + alpha - ((alpha + 100) / 4) + 25;
        B = A + 1524;
        C = (int)((B - 122.1) / 365.25);
        D = (36525 * (C & 32767)) / 100;
        E = (int)((B - D) / 30.6001);
        X1 = (int)(30.6001 * E);
        p->D = B - D - X1;
        p->M = E < 14 ? E - 1 : E - 13;
        p->Y = p->M > 2 ? C - 4716 : C - 4715;
    }
    p->validYMD = 1;
}

/*
** Compute the Hour, Minute, and Seconds from the julian day number.
*/
static void computeHMS(DateTime *p)
{
    int day_ms, day_min; /* milliseconds, minutes into the day */
    if (p->validHMS)
        return;
    computeJD(p);
    day_ms = (int)((p->iJD + 43200000) % 86400000);
    p->s = (day_ms % 60000) / 1000.0;
    day_min = day_ms / 60000;
    p->m = day_min % 60;
    p->h = day_min / 60;
    p->rawS = 0;
    p->validHMS = 1;
}

/*
** Compute both YMD and HMS
*/
static void computeYMD_HMS(DateTime *p)
{
    computeYMD(p);
    computeHMS(p);
}

/*
** Convert zDate into one or more integers according to the conversion
** specifier zFormat.
**
** zFormat[] contains 4 characters for each integer converted, except for
** the last integer which is specified by three characters.  The meaning
** of a four-character format specifiers ABCD is:
**
**    A:   number of digits to convert.  Always "2" or "4".
**    B:   minimum value.  Always "0" or "1".
**    C:   maximum value, decoded as:
**           a:  12
**           b:  14
**           c:  24
**           d:  31
**           e:  59
**           f:  9999
**    D:   the separator character, or \000 to indicate this is the
**         last number to convert.
**
** Example:  To translate an ISO-8601 date YYYY-MM-DD, the format would
** be "40f-21a-20c".  The "40f-" indicates the 4-digit year followed by "-".
** The "21a-" indicates the 2-digit month followed by "-".  The "20c" indicates
** the 2-digit day which is the last integer in the set.
**
** The function returns the number of successful conversions.
*/
static int getDigits(const char *zDate, const char *zFormat, ...)
{
    /* The aMx[] array translates the 3rd character of each format
    ** spec into a max size:    a   b   c   d   e      f */
    static const uint16_t aMx[] = {12, 14, 24, 31, 59, 14712};
    va_list ap;
    int cnt = 0;
    char nextC;
    va_start(ap, zFormat);
    do
    {
        char N = zFormat[0] - '0';
        char min = zFormat[1] - '0';
        int val = 0;
        uint16_t max;

        assert(zFormat[2] >= 'a' && zFormat[2] <= 'f');
        max = aMx[zFormat[2] - 'a'];
        nextC = zFormat[3];
        val = 0;
        while (N--)
        {
            if (!isdigit(*zDate))
            {
                goto end_getDigits;
            }
            val = val * 10 + *zDate - '0';
            zDate++;
        }
        if (val < (int)min || val > (int)max || (nextC != 0 && nextC != *zDate))
        {
            goto end_getDigits;
        }
        *va_arg(ap, int *) = val;
        zDate++;
        cnt++;
        zFormat += 4;
    } while (nextC);
end_getDigits:
    va_end(ap);
    return cnt;
}

static int atoF(const char *z, double *pResult)
{
    char *pend;
    *pResult = strtod(z, &pend);
    return pend == z ? 0 : 1;
}

/*
** If the DateTime p is raw number, try to figure out if it is
** a julian day number of a unix timestamp.  Set the p value
** appropriately.
*/
static void autoAdjustDate(DateTime *p)
{
    if (!p->rawS || p->validJD)
    {
        p->rawS = 0;
    }
    else if (p->s >= -21086676 * (int64_t)10000           /* -4713-11-24 12:00:00 */
             && p->s <= (25340230 * (int64_t)10000) + 799 /*  9999-12-31 23:59:59 */
    )
    {
        double r = p->s * 1000.0 + 210866760000000.0;
        clearYMD_HMS_TZ(p);
        p->iJD = (int64_t)(r + 0.5);
        p->validJD = 1;
        p->rawS = 0;
    }
}

/*
** Parse a timezone extension on the end of a date-time.
** The extension is of the form:
**
**        (+/-)HH:MM
**
** Or the "zulu" notation:
**
**        Z
**
** If the parse is successful, write the number of minutes
** of change in p->tz and return 0.  If a parser error occurs,
** return non-zero.
**
** A missing specifier is not considered an error.
*/
static int parseTimezone(const char *zDate, DateTime *p)
{
    int sgn = 0;
    int nHr, nMn;
    int c;
    while (isspace(*zDate))
    {
        zDate++;
    }
    p->tz = 0;
    c = *zDate;
    if (c == '-')
    {
        sgn = -1;
    }
    else if (c == '+')
    {
        sgn = +1;
    }
    else if (c == 'Z' || c == 'z')
    {
        zDate++;
        p->isLocal = 0;
        p->isUtc = 1;
        goto zulu_time;
    }
    else
    {
        return c != 0;
    }
    zDate++;
    if (getDigits(zDate, "20b:20e", &nHr, &nMn) != 2)
    {
        return 1;
    }
    zDate += 5;
    p->tz = sgn * (nMn + nHr * 60);
zulu_time:
    while (isspace(*zDate))
    {
        zDate++;
    }
    return *zDate != 0;
}

/*
** Parse times of the form HH:MM or HH:MM:SS or HH:MM:SS.FFFF.
** The HH, MM, and SS must each be exactly 2 digits.  The
** fractional seconds FFFF can be one or more digits.
**
** Return 1 if there is a parsing error and 0 on success.
*/
static int parseHhMmSs(const char *zDate, DateTime *p)
{
    int h, m, s;
    double ms = 0.0;
    if (getDigits(zDate, "20c:20e", &h, &m) != 2)
    {
        return 1;
    }
    zDate += 5;
    if (*zDate == ':')
    {
        zDate++;
        if (getDigits(zDate, "20e", &s) != 1)
        {
            return 1;
        }
        zDate += 2;
        if (*zDate == '.' && isdigit(zDate[1]))
        {
            double rScale = 1.0;
            zDate++;
            while (isdigit(*zDate))
            {
                ms = ms * 10.0 + *zDate - '0';
                rScale *= 10.0;
                zDate++;
            }
            ms /= rScale;
        }
    }
    else
    {
        s = 0;
    }
    p->validJD = 0;
    p->rawS = 0;
    p->validHMS = 1;
    p->h = h;
    p->m = m;
    p->s = s + ms;
    if (parseTimezone(zDate, p))
        return 1;
    return 0;
}

/*
** Parse dates of the form
**
**     YYYY-MM-DD HH:MM:SS.FFF
**     YYYY-MM-DD HH:MM:SS
**     YYYY-MM-DD HH:MM
**     YYYY-MM-DD
**
** Write the result into the DateTime structure and return 0
** on success and 1 if the input string is not a well-formed
** date.
*/
static int parseYyyyMmDd(const char *zDate, DateTime *p)
{
    int Y, M, D, neg;

    if (zDate[0] == '-')
    {
        zDate++;
        neg = 1;
    }
    else
    {
        neg = 0;
    }
    if (getDigits(zDate, "40f-21a-21d", &Y, &M, &D) != 3)
    {
        return 1;
    }
    zDate += 10;
    while (isspace(*zDate) || 'T' == *(uint8_t *)zDate)
    {
        zDate++;
    }
    if (parseHhMmSs(zDate, p) == 0)
    {
        /* We got the time */
    }
    else if (*zDate == 0)
    {
        p->validHMS = 0;
    }
    else
    {
        return 1;
    }
    p->validJD = 0;
    p->validYMD = 1;
    p->Y = neg ? -Y : Y;
    p->M = M;
    p->D = D;
    computeFloor(p);
    if (p->tz)
    {
        computeJD(p);
    }
    return 0;
}

/*
** Attempt to parse the given string into a julian day number.  Return
** the number of errors.
**
** The following are acceptable forms for the input string:
**
**      YYYY-MM-DD HH:MM:SS.FFF  +/-HH:MM
**      DDDD.DD
**      now
**
** In the first form, the +/-HH:MM is always optional.  The fractional
** seconds extension (the ".FFF") is optional.  The seconds portion
** (":SS.FFF") is option.  The year and date can be omitted as long
** as there is a time string.  The time string can be omitted as long
** as there is a year and date.
*/
static int parseDateOrTime(const char *zDate, DateTime *p)
{
    double r;
    if (parseYyyyMmDd(zDate, p) == 0)
    {
        return 0;
    }
    else if (parseHhMmSs(zDate, p) == 0)
    {
        return 0;
    }
    else if (strcmp(zDate, "now") == 0)
    {
        return setDateTimeToCurrent(p);
    }
    else if (atoF(zDate, &r) > 0)
    {
        setRawDateNumber(p, r);
        return 0;
    }
    else if ((strcmp(zDate, "subsec") == 0 || strcmp(zDate, "subsecond") == 0))
    {
        p->useSubsec = 1;
        return setDateTimeToCurrent(p);
    }
    return 1;
}

static int osLocaltime(time_t *t, struct tm *pTm)
{
    struct tm *pX;
    pX = localtime(t);
    if (pX)
    {
        *pTm = *pX;
        return 0;
    }
    return 1;
}

static int toLocaltime(DateTime *p)
{
    time_t t;
    struct tm sLocal;
    int iYearDiff;

    /* Initialize the contents of sLocal to avoid a compiler warning. */
    memset(&sLocal, 0, sizeof(sLocal));

    computeJD(p);
    if (p->iJD < 2108667600 * (int64_t)100000    /* 1970-01-01 */
        || p->iJD > 2130141456 * (int64_t)100000 /* 2038-01-18 */
    )
    {
        /* EVIDENCE-OF: R-55269-29598 The localtime_r() C function normally only
        ** works for years between 1970 and 2037. For dates outside this range,
        ** SQLite attempts to map the year into an equivalent year within this
        ** range, do the calculation, then map the year back.
        */
        DateTime x = *p;
        computeYMD_HMS(&x);
        iYearDiff = (2000 + x.Y % 4) - x.Y;
        x.Y += iYearDiff;
        x.validJD = 0;
        computeJD(&x);
        t = (time_t)(x.iJD / 1000 - 21086676 * (int64_t)10000);
    }
    else
    {
        iYearDiff = 0;
        t = (time_t)(p->iJD / 1000 - 21086676 * (int64_t)10000);
    }
    if (osLocaltime(&t, &sLocal))
    {
        return 1;
    }
    p->Y = sLocal.tm_year + 1900 - iYearDiff;
    p->M = sLocal.tm_mon + 1;
    p->D = sLocal.tm_mday;
    p->h = sLocal.tm_hour;
    p->m = sLocal.tm_min;
    p->s = sLocal.tm_sec + (p->iJD % 1000) * 0.001;
    p->validYMD = 1;
    p->validHMS = 1;
    p->validJD = 0;
    p->rawS = 0;
    p->tz = 0;
    p->isError = 0;
    return 0;
}

static const struct
{
    uint8_t nName; /* Length of the name */
    char zName[7]; /* Name of the transformation */
    float rLimit;  /* Maximum NNN value for this transform */
    float rXform;  /* Constant used for this transform */
} aXformType[] = {
    /* 0 */ {6, "second", 4.6427e+14, 1.0},
    /* 1 */ {6, "minute", 7.7379e+12, 60.0},
    /* 2 */ {4, "hour", 1.2897e+11, 3600.0},
    /* 3 */ {3, "day", 5373485.0, 86400.0},
    /* 4 */ {5, "month", 176546.0, 2592000.0},
    /* 5 */ {4, "year", 14713.0, 31536000.0},
};

/*
** Process a modifier to a date-time stamp.  The modifiers are
** as follows:
**
**     NNN days
**     NNN hours
**     NNN minutes
**     NNN.NNNN seconds
**     NNN months
**     NNN years
**     +/-YYYY-MM-DD HH:MM:SS.SSS
**     ceiling
**     floor
**     start of month
**     start of year
**     start of week
**     start of day
**     weekday N
**     unixepoch
**     auto
**     localtime
**     utc
**     subsec
**     subsecond
**
** Return 0 on success and 1 if there is any kind of error. If the error
** is in a system call (i.e. localtime()), then an error message is written
** to context pCtx. If the error is an unrecognized modifier, no error is
** written to pCtx.
*/
static int parseModifier(
    const char *z, /* The text of the modifier */
    int n,         /* Length of zMod in bytes */
    DateTime *p,   /* The date/time value to be modified */
    int idx        /* Parameter index of the modifier */
)
{
    int rc = 1;
    double r;
    switch (tolower(z[0]))
    {
    case 'a':
    {
        /*
        **    auto
        **
        ** If rawS is available, then interpret as a julian day number, or
        ** a unix timestamp, depending on its magnitude.
        */
        if (strcmp(z, "auto") == 0)
        {
            if (idx > 1)
                return 1; /* IMP: R-33611-57934 */
            autoAdjustDate(p);
            rc = 0;
        }
        break;
    }
    case 'c':
    {
        /*
        **    ceiling
        **
        ** Resolve day-of-month overflow by rolling forward into the next
        ** month.  As this is the default action, this modifier is really
        ** a no-op that is only included for symmetry.  See "floor".
        */
        if (strcmp(z, "ceiling") == 0)
        {
            computeJD(p);
            clearYMD_HMS_TZ(p);
            rc = 0;
            p->nFloor = 0;
        }
        break;
    }
    case 'f':
    {
        /*
        **    floor
        **
        ** Resolve day-of-month overflow by rolling back to the end of the
        ** previous month.
        */
        if (strcmp(z, "floor") == 0)
        {
            computeJD(p);
            p->iJD -= p->nFloor * 86400000;
            clearYMD_HMS_TZ(p);
            rc = 0;
        }
        break;
    }
    case 'j':
    {
        /*
        **    julianday
        **
        ** Always interpret the prior number as a julian-day value.  If this
        ** is not the first modifier, or if the prior argument is not a numeric
        ** value in the allowed range of julian day numbers understood by
        ** SQLite (0..5373484.5) then the result will be NULL.
        */
        if (strcmp(z, "julianday") == 0)
        {
            if (idx > 1)
                return 1; /* IMP: R-31176-64601 */
            if (p->validJD && p->rawS)
            {
                rc = 0;
                p->rawS = 0;
            }
        }
        break;
    }
    case 'l':
    {
        /*    localtime
        **
        ** Assuming the current time value is UTC (a.k.a. GMT), shift it to
        ** show local time.
        */
        if (strcmp(z, "localtime") == 0)
        {
            rc = p->isLocal ? 0 : toLocaltime(p);
            p->isUtc = 0;
            p->isLocal = 1;
        }
        break;
    }
    case 'u':
    {
        /*
        **    unixepoch
        **
        ** Treat the current value of p->s as the number of
        ** seconds since 1970.  Convert to a real julian day number.
        */
        if (strcmp(z, "unixepoch") == 0 && p->rawS)
        {
            if (idx > 1)
                return 1; /* IMP: R-49255-55373 */
            r = p->s * 1000.0 + 210866760000000.0;
            if (r >= 0.0 && r < 464269060800000.0)
            {
                clearYMD_HMS_TZ(p);
                p->iJD = (int64_t)(r + 0.5);
                p->validJD = 1;
                p->rawS = 0;
                rc = 0;
            }
        }
        else if (strcmp(z, "utc") == 0)
        {
            if (p->isUtc == 0)
            {
                int64_t iOrigJD; /* Original localtime */
                int64_t iGuess;  /* Guess at the corresponding utc time */
                int cnt = 0;     /* Safety to prevent infinite loop */
                int64_t iErr;    /* Guess is off by this much */

                computeJD(p);
                iGuess = iOrigJD = p->iJD;
                iErr = 0;
                do
                {
                    DateTime new;
                    memset(&new, 0, sizeof(new));
                    iGuess -= iErr;
                    new.iJD = iGuess;
                    new.validJD = 1;
                    rc = toLocaltime(&new);
                    if (rc)
                        return rc;
                    computeJD(&new);
                    iErr = new.iJD - iOrigJD;
                } while (iErr && cnt++ < 3);
                memset(p, 0, sizeof(*p));
                p->iJD = iGuess;
                p->validJD = 1;
                p->isUtc = 1;
                p->isLocal = 0;
            }
            rc = 0;
        }
        break;
    }
    case 'w':
    {
        /*
        **    weekday N
        **
        ** Move the date to the same time on the next occurrence of
        ** weekday N where 0==Sunday, 1==Monday, and so forth.  If the
        ** date is already on the appropriate weekday, this is a no-op.
        */
        if (strncmp(z, "weekday ", 8) == 0 && atoF(&z[8], &r) > 0 && r >= 0.0 && r < 7.0 && (n = (int)r) == r)
        {
            int64_t Z;
            computeYMD_HMS(p);
            p->tz = 0;
            p->validJD = 0;
            computeJD(p);
            Z = ((p->iJD + 129600000) / 86400000) % 7;
            if (Z > n)
                Z -= 7;
            p->iJD += (n - Z) * 86400000;
            clearYMD_HMS_TZ(p);
            rc = 0;
        }
        break;
    }
    case 's':
    {
        /*
        **    start of TTTTT
        **
        ** Move the date backwards to the beginning of the current day,
        ** or month or year.
        **
        **    subsecond
        **    subsec
        **
        ** Show subsecond precision in the output of datetime() and
        ** unixepoch() and strftime('%s').
        */
        if (strncmp(z, "start of ", 9) != 0)
        {
            if (strcmp(z, "subsec") == 0 || strcmp(z, "subsecond") == 0)
            {
                p->useSubsec = 1;
                rc = 0;
            }
            break;
        }
        if (!p->validJD && !p->validYMD && !p->validHMS)
            break;
        z += 9;
        computeYMD(p);
        p->validHMS = 1;
        p->h = p->m = 0;
        p->s = 0.0;
        p->rawS = 0;
        p->tz = 0;
        p->validJD = 0;
        if (strcmp(z, "month") == 0)
        {
            p->D = 1;
            rc = 0;
        }
        else if (strcmp(z, "year") == 0)
        {
            p->M = 1;
            p->D = 1;
            rc = 0;
        }
        else if (strcmp(z, "day") == 0)
        {
            rc = 0;
        }
        break;
    }
    case '+':
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    {
        double rRounder;
        int i;
        int Y, M, D, h, m, x;
        const char *z2 = z;
        char z0 = z[0];
        for (n = 1; z[n]; n++)
        {
            if (z[n] == ':')
                break;
            if (isspace(z[n]))
                break;
            if (z[n] == '-')
            {
                if (n == 5 && getDigits(&z[1], "40f", &Y) == 1)
                    break;
                if (n == 6 && getDigits(&z[1], "50f", &Y) == 1)
                    break;
            }
        }
        if (atoF(z, &r) <= 0)
        {
            assert(rc == 1);
            break;
        }
        if (z[n] == '-')
        {
            /* A modifier of the form (+|-)YYYY-MM-DD adds or subtracts the
            ** specified number of years, months, and days.  MM is limited to
            ** the range 0-11 and DD is limited to 0-30.
            */
            if (z0 != '+' && z0 != '-')
                break; /* Must start with +/- */
            if (n == 5)
            {
                if (getDigits(&z[1], "40f-20a-20d", &Y, &M, &D) != 3)
                    break;
            }
            else
            {
                assert(n == 6);
                if (getDigits(&z[1], "50f-20a-20d", &Y, &M, &D) != 3)
                    break;
                z++;
            }
            if (M >= 12)
                break; /* M range 0..11 */
            if (D >= 31)
                break; /* D range 0..30 */
            computeYMD_HMS(p);
            p->validJD = 0;
            if (z0 == '-')
            {
                p->Y -= Y;
                p->M -= M;
                D = -D;
            }
            else
            {
                p->Y += Y;
                p->M += M;
            }
            x = p->M > 0 ? (p->M - 1) / 12 : (p->M - 12) / 12;
            p->Y += x;
            p->M -= x * 12;
            computeFloor(p);
            computeJD(p);
            p->validHMS = 0;
            p->validYMD = 0;
            p->iJD += (int64_t)D * 86400000;
            if (z[11] == 0)
            {
                rc = 0;
                break;
            }
            if (isspace(z[11]) && getDigits(&z[12], "20c:20e", &h, &m) == 2)
            {
                z2 = &z[12];
                n = 2;
            }
            else
            {
                break;
            }
        }
        if (z2[n] == ':')
        {
            /* A modifier of the form (+|-)HH:MM:SS.FFF adds (or subtracts) the
            ** specified number of hours, minutes, seconds, and fractional seconds
            ** to the time.  The ".FFF" may be omitted.  The ":SS.FFF" may be
            ** omitted.
            */

            DateTime tx;
            int64_t day;
            if (!isdigit(*z2))
                z2++;
            memset(&tx, 0, sizeof(tx));
            if (parseHhMmSs(z2, &tx))
                break;
            computeJD(&tx);
            tx.iJD -= 43200000;
            day = tx.iJD / 86400000;
            tx.iJD -= day * 86400000;
            if (z0 == '-')
                tx.iJD = -tx.iJD;
            computeJD(p);
            clearYMD_HMS_TZ(p);
            p->iJD += tx.iJD;
            rc = 0;
            break;
        }

        /* If control reaches this point, it means the transformation is
        ** one of the forms like "+NNN days".  */
        z += n;
        while (isspace(*z))
            z++;
        n = strlen(z);
        if (n < 3 || n > 10)
            break;
        if (tolower(z[n - 1]) == 's')
            n--;
        computeJD(p);
        assert(rc == 1);
        rRounder = r < 0 ? -0.5 : +0.5;
        p->nFloor = 0;
        for (i = 0; i < sizeof(aXformType) / sizeof(aXformType[0]); i++)
        {
            if (aXformType[i].nName == n && strncmp(aXformType[i].zName, z, n) == 0 && r > -aXformType[i].rLimit && r < aXformType[i].rLimit)
            {
                switch (i)
                {
                case 4:
                { /* Special processing to add months */
                    assert(strcmp(aXformType[4].zName, "month") == 0);
                    computeYMD_HMS(p);
                    p->M += (int)r;
                    x = p->M > 0 ? (p->M - 1) / 12 : (p->M - 12) / 12;
                    p->Y += x;
                    p->M -= x * 12;
                    computeFloor(p);
                    p->validJD = 0;
                    r -= (int)r;
                    break;
                }
                case 5:
                { /* Special processing to add years */
                    int y = (int)r;
                    assert(strcmp(aXformType[5].zName, "year") == 0);
                    computeYMD_HMS(p);
                    assert(p->M >= 0 && p->M <= 12);
                    p->Y += y;
                    computeFloor(p);
                    p->validJD = 0;
                    r -= (int)r;
                    break;
                }
                }
                computeJD(p);
                p->iJD += (int64_t)(r * 1000.0 * aXformType[i].rXform + rRounder);
                rc = 0;
                break;
            }
        }
        clearYMD_HMS_TZ(p);
        break;
    }
    default:
    {
        break;
    }
    }
    return rc;
}

static int valueIsNumber(const char *v)
{
    const char *p = v;
    int dotCount = 0;
    if (!isdigit(*p++))
        return 0;
    while (p[0])
    {
        if (*p == '.')
        {
            dotCount++;
            if (dotCount > 1)
                return 0;
        }
        else if (!isdigit(*p))
            return 0;

        p++;
    }
    return 1;
}

static int isDate(int argc, char *argv[], DateTime *p)
{
    int n;
    const char *z;
    if (argc == 0)
    {
        return setDateTimeToCurrent(p);
    }
    if (valueIsNumber(argv[0]))
    {
        setRawDateNumber(p, strtod(argv[0], NULL));
    }
    else if (!argv[0] || parseDateOrTime(argv[0], p))
    {
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        z = argv[i];
        n = strlen(z);
        if (argv[i] == NULL || parseModifier(z, n, p, i))
            return 1;
    }
    computeJD(p);
    if (p->isError || !validJulianDay(p->iJD))
        return 1;
    if (argc == 1 && p->validYMD && p->D > 28)
    {
        /* Make sure a YYYY-MM-DD is normalized.
        ** Example: 2023-02-31 -> 2023-03-03 */
        assert(p->validJD);
        p->validYMD = 0;
    }
    return 0;
}

double dt_julianday(int argc, ...)
{
    va_list ap;
    char *argv[32];
    DateTime x;
    va_start(ap, argc);
    for (int i = 0; i < argc; i++)
    {
        argv[i] = va_arg(ap, char *);
    }
    va_end(ap);
    if (isDate(argc, argv, &x) == 0)
    {
        computeJD(&x);
        printf("jd %llu\n", x.iJD);
        return (double)x.iJD / 86400000.0;
    }
    return 0.0;
}

const char *dt_datetime(int argc, ...)
{
    DateTime x;
    va_list ap;
    char *argv[32];
    va_start(ap, argc);
    for (int i = 0; i < argc; i++)
    {
        argv[i] = va_arg(ap, char *);
    }
    va_end(ap);
    if (isDate(argc, argv, &x) == 0)
    {
        int Y, s;
        static __thread char zBuf[32];
        computeYMD_HMS(&x);
        Y = x.Y;
        if (Y < 0)
            Y = -Y;
        zBuf[1] = '0' + (Y / 1000) % 10;
        zBuf[2] = '0' + (Y / 100) % 10;
        zBuf[3] = '0' + (Y / 10) % 10;
        zBuf[4] = '0' + (Y) % 10;
        zBuf[5] = '-';
        zBuf[6] = '0' + (x.M / 10) % 10;
        zBuf[7] = '0' + (x.M) % 10;
        zBuf[8] = '-';
        zBuf[9] = '0' + (x.D / 10) % 10;
        zBuf[10] = '0' + (x.D) % 10;
        zBuf[11] = ' ';
        zBuf[12] = '0' + (x.h / 10) % 10;
        zBuf[13] = '0' + (x.h) % 10;
        zBuf[14] = ':';
        zBuf[15] = '0' + (x.m / 10) % 10;
        zBuf[16] = '0' + (x.m) % 10;
        zBuf[17] = ':';
        if (x.useSubsec)
        {
            s = (int)(1000.0 * x.s + 0.5);
            zBuf[18] = '0' + (s / 10000) % 10;
            zBuf[19] = '0' + (s / 1000) % 10;
            zBuf[20] = '.';
            zBuf[21] = '0' + (s / 100) % 10;
            zBuf[22] = '0' + (s / 10) % 10;
            zBuf[23] = '0' + (s) % 10;
            zBuf[24] = 0;
        }
        else
        {
            s = (int)x.s;
            zBuf[18] = '0' + (s / 10) % 10;
            zBuf[19] = '0' + (s) % 10;
            zBuf[20] = 0;
        }
        if (x.Y < 0)
        {
            zBuf[0] = '-';
            return zBuf;
        }
        else
        {
            return &zBuf[1];
        }
    }
    return NULL;
}

int64_t dt_unixepoch(int argc, ...)
{
    va_list ap;
    char *argv[32];
    DateTime x;
    va_start(ap, argc);
    for (int i = 0; i < argc; i++)
    {
        argv[i] = va_arg(ap, char *);
    }
    va_end(ap);
    if (isDate(argc, argv, &x) == 0)
    {
        computeJD(&x);
        return x.iJD / 1000 - 21086676 * (int64_t)10000;
    }
    return 0;
}

const char *dt_date(int argc, ...)
{
    DateTime x;
    va_list ap;
    char *argv[32];
    va_start(ap, argc);
    for (int i = 0; i < argc; i++)
    {
        argv[i] = va_arg(ap, char *);
    }
    va_end(ap);
    if (isDate(argc, argv, &x) == 0)
    {
        int Y;
        static __thread char zBuf[16];
        computeYMD(&x);
        Y = x.Y;
        if (Y < 0)
            Y = -Y;
        zBuf[1] = '0' + (Y / 1000) % 10;
        zBuf[2] = '0' + (Y / 100) % 10;
        zBuf[3] = '0' + (Y / 10) % 10;
        zBuf[4] = '0' + (Y) % 10;
        zBuf[5] = '-';
        zBuf[6] = '0' + (x.M / 10) % 10;
        zBuf[7] = '0' + (x.M) % 10;
        zBuf[8] = '-';
        zBuf[9] = '0' + (x.D / 10) % 10;
        zBuf[10] = '0' + (x.D) % 10;
        zBuf[11] = 0;
        if (x.Y < 0)
        {
            zBuf[0] = '-';
            return zBuf;
        }
        else
        {
            return &zBuf[1];
        }
    }
    return NULL;
}

const char *dt_time(int argc, ...)
{
    DateTime x;
    va_list ap;
    char *argv[32];
    va_start(ap, argc);
    for (int i = 0; i < argc; i++)
    {
        argv[i] = va_arg(ap, char *);
    }
    va_end(ap);
    if (isDate(argc, argv, &x) == 0)
    {
        int s, n;
        static __thread char zBuf[16];
        computeHMS(&x);
        zBuf[0] = '0' + (x.h / 10) % 10;
        zBuf[1] = '0' + (x.h) % 10;
        zBuf[2] = ':';
        zBuf[3] = '0' + (x.m / 10) % 10;
        zBuf[4] = '0' + (x.m) % 10;
        zBuf[5] = ':';
        if (x.useSubsec)
        {
            s = (int)(1000.0 * x.s + 0.5);
            zBuf[6] = '0' + (s / 10000) % 10;
            zBuf[7] = '0' + (s / 1000) % 10;
            zBuf[8] = '.';
            zBuf[9] = '0' + (s / 100) % 10;
            zBuf[10] = '0' + (s / 10) % 10;
            zBuf[11] = '0' + (s) % 10;
            zBuf[12] = 0;
            n = 12;
        }
        else
        {
            s = (int)x.s;
            zBuf[6] = '0' + (s / 10) % 10;
            zBuf[7] = '0' + (s) % 10;
            zBuf[8] = 0;
            n = 8;
        }
        return zBuf;
    }
    return NULL;
}

/*
** timediff(DATE1, DATE2)
**
** Return the amount of time that must be added to DATE2 in order to
** convert it into DATE2.  The time difference format is:
**
**     +YYYY-MM-DD HH:MM:SS.SSS
**
** The initial "+" becomes "-" if DATE1 occurs before DATE2.  For
** date/time values A and B, the following invariant should hold:
**
**     datetime(A) == (datetime(B, timediff(A,B))
**
** Both DATE arguments must be either a julian day number, or an
** ISO-8601 string.  The unix timestamps are not supported by this
** routine.
*/
const char *dt_timediff(int argc, ...)
{
    char sign;
    int Y, M;
    DateTime d1, d2;
    va_list ap;
    char *argv[32];
    static __thread char sres[32];
    sres[0] = 0;
    if (argc < 2)
        return NULL;
    va_start(ap, argc);
    for (int i = 0; i < argc; i++)
    {
        argv[i] = va_arg(ap, char *);
    }
    va_end(ap);
    if (isDate(1, &argv[0], &d1))
        return NULL;
    if (isDate(1, &argv[1], &d2))
        return NULL;
    computeYMD_HMS(&d1);
    computeYMD_HMS(&d2);
    if (d1.iJD >= d2.iJD)
    {
        sign = '+';
        Y = d1.Y - d2.Y;
        if (Y)
        {
            d2.Y = d1.Y;
            d2.validJD = 0;
            computeJD(&d2);
        }
        M = d1.M - d2.M;
        if (M < 0)
        {
            Y--;
            M += 12;
        }
        if (M != 0)
        {
            d2.M = d1.M;
            d2.validJD = 0;
            computeJD(&d2);
        }
        while (d1.iJD < d2.iJD)
        {
            M--;
            if (M < 0)
            {
                M = 11;
                Y--;
            }
            d2.M--;
            if (d2.M < 1)
            {
                d2.M = 12;
                d2.Y--;
            }
            d2.validJD = 0;
            computeJD(&d2);
        }
        d1.iJD -= d2.iJD;
        d1.iJD += (uint64_t)1486995408 * (uint64_t)100000;
    }
    else /* d1<d2 */
    {
        sign = '-';
        Y = d2.Y - d1.Y;
        if (Y)
        {
            d2.Y = d1.Y;
            d2.validJD = 0;
            computeJD(&d2);
        }
        M = d2.M - d1.M;
        if (M < 0)
        {
            Y--;
            M += 12;
        }
        if (M != 0)
        {
            d2.M = d1.M;
            d2.validJD = 0;
            computeJD(&d2);
        }
        while (d1.iJD > d2.iJD)
        {
            M--;
            if (M < 0)
            {
                M = 11;
                Y--;
            }
            d2.M++;
            if (d2.M > 12)
            {
                d2.M = 1;
                d2.Y++;
            }
            d2.validJD = 0;
            computeJD(&d2);
        }
        d1.iJD = d2.iJD - d1.iJD;
        d1.iJD += (uint64_t)1486995408 * (uint64_t)100000;
    }
    clearYMD_HMS_TZ(&d1);
    computeYMD_HMS(&d1);
    sprintf(sres, "%c%04d-%02d-%02d %02d:%02d:%06.3f", sign, Y, M, d1.D - 1, d1.h, d1.m, d1.s);
    return sres;
}

typedef struct dt_str dt_str;
struct dt_str
{
    int m;
    int l;
    char d[128];
};

static void strInit(dt_str *str)
{
    str->m = 128;
    str->l = 0;
}

static void strAppend(dt_str *str, const char *ss, int n)
{
    int free = str->m - str->l - 1;
    if (n > free)
        n = free;
    memcpy(str->d, ss, n);
    str->l += n;
    str->d[str->l] = 0;
}

static void strAppendF(dt_str *str, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(&str->d[str->l], str->m - str->l - 1, fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        str->l += n;
    }
    str->d[str->l] = 0;
}

static void strAppendChar(dt_str *str, int n, char c)
{
    if (str->l + n + 1 <= str->m)
    {
        while (n--)
        {
            str->d[str->l++] = c;
        }
        str->d[str->l] = 0;
    }
}

/*
** Compute the number of days after the most recent January 1.
**
** In other words, compute the zero-based day number for the
** current year:
**
**   Jan01 = 0,  Jan02 = 1, ..., Jan31 = 30, Feb01 = 31, ...
**   Dec31 = 364 or 365.
*/
static int daysAfterJan01(DateTime *pDate)
{
    DateTime jan01 = *pDate;
    assert(jan01.validYMD);
    assert(jan01.validHMS);
    assert(pDate->validJD);
    jan01.validJD = 0;
    jan01.M = 1;
    jan01.D = 1;
    computeJD(&jan01);
    return (int)((pDate->iJD - jan01.iJD + 43200000) / 86400000);
}

/*
** Return the number of days after the most recent Monday.
**
** In other words, return the day of the week according
** to this code:
**
**   0=Monday, 1=Tuesday, 2=Wednesday, ..., 6=Sunday.
*/
static int daysAfterMonday(DateTime *pDate)
{
    assert(pDate->validJD);
    return (int)((pDate->iJD + 43200000) / 86400000) % 7;
}

/*
** Return the number of days after the most recent Sunday.
**
** In other words, return the day of the week according
** to this code:
**
**   0=Sunday, 1=Monday, 2=Tues, ..., 6=Saturday
*/
static int daysAfterSunday(DateTime *pDate)
{
    assert(pDate->validJD);
    return (int)((pDate->iJD + 129600000) / 86400000) % 7;
}

/*
**    strftime( FORMAT, TIMESTRING, MOD, MOD, ...)
**
** Return a string described by FORMAT.  Conversions as follows:
**
**   %d  day of month  01-31
**   %e  day of month  1-31
**   %f  ** fractional seconds  SS.SSS
**   %F  ISO date.  YYYY-MM-DD
**   %G  ISO year corresponding to %V 0000-9999.
**   %g  2-digit ISO year corresponding to %V 00-99
**   %H  hour 00-24
**   %k  hour  0-24  (leading zero converted to space)
**   %I  hour 01-12
**   %j  day of year 001-366
**   %J  ** julian day number
**   %l  hour  1-12  (leading zero converted to space)
**   %m  month 01-12
**   %M  minute 00-59
**   %p  "am" or "pm"
**   %P  "AM" or "PM"
**   %R  time as HH:MM
**   %s  seconds since 1970-01-01
**   %S  seconds 00-59
**   %T  time as HH:MM:SS
**   %u  day of week 1-7  Monday==1, Sunday==7
**   %w  day of week 0-6  Sunday==0, Monday==1
**   %U  week of year 00-53  (First Sunday is start of week 01)
**   %V  week of year 01-53  (First week containing Thursday is week 01)
**   %W  week of year 00-53  (First Monday is start of week 01)
**   %Y  year 0000-9999
**   %%  %
*/
const char *dt_strftime(const char *zFmt, int argc, ...)
{
    DateTime x;
    size_t i, j;
    static __thread dt_str sRes;
    va_list ap;
    char *argv[32];
    va_start(ap, argc);
    for (int i = 0; i < argc; i++)
    {
        argv[i] = va_arg(ap, char *);
    }
    va_end(ap);
    if (argc == 0)
        return NULL;
    if (zFmt == 0 || isDate(argc, argv, &x))
        return NULL;
    strInit(&sRes);
    computeJD(&x);
    computeYMD_HMS(&x);
    for (i = j = 0; zFmt[i]; i++)
    {
        char cf;
        if (zFmt[i] != '%')
            continue;
        if (j < i)
            strAppend(&sRes, zFmt + j, (int)(i - j));
        i++;
        j = i + 1;
        cf = zFmt[i];
        switch (cf)
        {
        case 'd': /* Fall thru */
        case 'e':
        {
            strAppendF(&sRes, cf == 'd' ? "%02d" : "%2d", x.D);
            break;
        }
        case 'f':
        { /* Fractional seconds.  (Non-standard) */
            double s = x.s;
            if (s > 59.999)
                s = 59.999;
            strAppendF(&sRes, "%06.3f", s);
            break;
        }
        case 'F':
        {
            strAppendF(&sRes, "%04d-%02d-%02d", x.Y, x.M, x.D);
            break;
        }
        case 'G': /* Fall thru */
        case 'g':
        {
            DateTime y = x;
            assert(y.validJD);
            /* Move y so that it is the Thursday in the same week as x */
            y.iJD += (3 - daysAfterMonday(&x)) * 86400000;
            y.validYMD = 0;
            computeYMD(&y);
            if (cf == 'g')
            {
                strAppendF(&sRes, "%02d", y.Y % 100);
            }
            else
            {
                strAppendF(&sRes, "%04d", y.Y);
            }
            break;
        }
        case 'H':
        case 'k':
        {
            strAppendF(&sRes, cf == 'H' ? "%02d" : "%2d", x.h);
            break;
        }
        case 'I': /* Fall thru */
        case 'l':
        {
            int h = x.h;
            if (h > 12)
                h -= 12;
            if (h == 0)
                h = 12;
            strAppendF(&sRes, cf == 'I' ? "%02d" : "%2d", h);
            break;
        }
        case 'j':
        { /* Day of year.  Jan01==1, Jan02==2, and so forth */
            strAppendF(&sRes, "%03d", daysAfterJan01(&x) + 1);
            break;
        }
        case 'J':
        { /* Julian day number.  (Non-standard) */
            strAppendF(&sRes, "%.16g", x.iJD / 86400000.0);
            break;
        }
        case 'm':
        {
            strAppendF(&sRes, "%02d", x.M);
            break;
        }
        case 'M':
        {
            strAppendF(&sRes, "%02d", x.m);
            break;
        }
        case 'p': /* Fall thru */
        case 'P':
        {
            if (x.h >= 12)
            {
                strAppend(&sRes, cf == 'p' ? "PM" : "pm", 2);
            }
            else
            {
                strAppend(&sRes, cf == 'p' ? "AM" : "am", 2);
            }
            break;
        }
        case 'R':
        {
            strAppendF(&sRes, "%02d:%02d", x.h, x.m);
            break;
        }
        case 's':
        {
            if (x.useSubsec)
            {
                strAppendF(&sRes, "%.3f",
                           (x.iJD - 21086676 * (int64_t)10000000) / 1000.0);
            }
            else
            {
                int64_t iS = (int64_t)(x.iJD / 1000 - 21086676 * (int64_t)10000);
                strAppendF(&sRes, "%lld", iS);
            }
            break;
        }
        case 'S':
        {
            strAppendF(&sRes, "%02d", (int)x.s);
            break;
        }
        case 'T':
        {
            strAppendF(&sRes, "%02d:%02d:%02d", x.h, x.m, (int)x.s);
            break;
        }
        case 'u': /* Day of week.  1 to 7.  Monday==1, Sunday==7 */
        case 'w':
        { /* Day of week.  0 to 6.  Sunday==0, Monday==1 */
            char c = (char)daysAfterSunday(&x) + '0';
            if (c == '0' && cf == 'u')
                c = '7';
            strAppendChar(&sRes, 1, c);
            break;
        }
        case 'U':
        { /* Week num. 00-53. First Sun of the year is week 01 */
            strAppendF(&sRes, "%02d",
                       (daysAfterJan01(&x) - daysAfterSunday(&x) + 7) / 7);
            break;
        }
        case 'V':
        { /* Week num. 01-53. First week with a Thur is week 01 */
            DateTime y = x;
            /* Adjust y so that is the Thursday in the same week as x */
            assert(y.validJD);
            y.iJD += (3 - daysAfterMonday(&x)) * 86400000;
            y.validYMD = 0;
            computeYMD(&y);
            strAppendF(&sRes, "%02d", daysAfterJan01(&y) / 7 + 1);
            break;
        }
        case 'W':
        { /* Week num. 00-53. First Mon of the year is week 01 */
            strAppendF(&sRes, "%02d",
                       (daysAfterJan01(&x) - daysAfterMonday(&x) + 7) / 7);
            break;
        }
        case 'Y':
        {
            strAppendF(&sRes, "%04d", x.Y);
            break;
        }
        case '%':
        {
            strAppendChar(&sRes, 1, '%');
            break;
        }
        default:
        {
            strInit(&sRes);
            return NULL;
        }
        }
    }
    if (j < i)
        strAppend(&sRes, zFmt + j, (int)(i - j));

    return sRes.d;
}