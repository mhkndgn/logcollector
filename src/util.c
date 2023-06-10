#include "util.h"
#include "timeval.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>

void *xcalloc(size_t count, size_t size)
{
    void *p = count && size ? calloc(count, size) : malloc(1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

void *
xzalloc(size_t size)
{
    return xcalloc(1, size);
}

void *
xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

void *
xrealloc(void *p, size_t size)
{
    p = realloc(p, size ? size : 1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

void *
x2nrealloc(void *p, size_t *n, size_t s)
{
    *n = *n == 0 ? 1 : 2 * *n;
    return xrealloc(p, *n * s);
}

void *
xmemdup(const void *p_, size_t size)
{
    void *p = xmalloc(size);
    nullable_memcpy(p, p_, size);
    return p;
}

char *
xmemdup0(const char *p_, size_t length)
{
    char *p = xmalloc(length + 1);
    memcpy(p, p_, length);
    p[length] = '\0';
    return p;
}

char *
xstrdup(const char *s)
{
    return xmemdup0(s, strlen(s));
}

char * nullable_xstrdup(const char *s)
{
    return s ? xstrdup(s) : NULL;
}

bool nullable_string_is_equal(const char *a, const char *b)
{
    return a ? b && !strcmp(a, b) : !b;
}

char *
xvasprintf(const char *format, va_list args)
{
    va_list args2;
    size_t needed;
    char *s;

    va_copy(args2, args);
    needed = vsnprintf(NULL, 0, format, args);

    s = xmalloc(needed + 1);

    vsnprintf(s, needed + 1, format, args2);
    va_end(args2);

    return s;
}

/* Converts floating-point string 's' into a double.  If successful, stores
 * the double in '*d' and returns true; on failure, stores 0 in '*d' and
 * returns false.
 *
 * Underflow (e.g. "1e-9999") is not considered an error, but overflow
 * (e.g. "1e9999)" is. */
bool
str_to_double(const char *s, double *d)
{
    int save_errno = errno;
    char *tail;
    errno = 0;
    *d = strtod(s, &tail);
    if (errno == EINVAL || (errno == ERANGE && *d != 0)
        || tail == s || *tail != '\0') {
        errno = save_errno;
        *d = 0;
        return false;
    } else {
        errno = save_errno;
        return true;
    }
}

/* Returns the value of 'c' as a hexadecimal digit. */
int
hexit_value(unsigned char c)
{
    static const signed char tbl[UCHAR_MAX + 1] = {
#define TBL(x)                                  \
        (  x >= '0' && x <= '9' ? x - '0'       \
         : x >= 'a' && x <= 'f' ? x - 'a' + 0xa \
         : x >= 'A' && x <= 'F' ? x - 'A' + 0xa \
         : -1)
#define TBL0(x)  TBL(x),  TBL((x) + 1),   TBL((x) + 2),   TBL((x) + 3)
#define TBL1(x) TBL0(x), TBL0((x) + 4),  TBL0((x) + 8),  TBL0((x) + 12)
#define TBL2(x) TBL1(x), TBL1((x) + 16), TBL1((x) + 32), TBL1((x) + 48)
        TBL2(0), TBL2(64), TBL2(128), TBL2(192)
    };

    return tbl[c];
}

/* Returns the integer value of the 'n' hexadecimal digits starting at 's', or
 * UINTMAX_MAX if one of those "digits" is not really a hex digit.  Sets '*ok'
 * to true if the conversion succeeds or to false if a non-hex digit is
 * detected. */
uintmax_t
hexits_value(const char *s, size_t n, bool *ok)
{
    uintmax_t value;
    size_t i;

    value = 0;
    for (i = 0; i < n; i++) {
        int hexit = hexit_value(s[i]);
        if (hexit < 0) {
            *ok = false;
            return UINTMAX_MAX;
        }
        value = (value << 4) + hexit;
    }
    *ok = true;
    return value;
}

char *
xasprintf(const char *format, ...)
{
    va_list args;
    char *s;

    va_start(args, format);
    s = xvasprintf(format, args);
    va_end(args);

    return s;
}

void out_of_memory(void)
{
    abort();
}

/* strftime() with an extension for high-resolution timestamps.  Any '#'s in
 * 'format' will be replaced by subseconds, e.g. use "%S.###" to obtain results
 * like "01.123".  */
size_t
strftime_msec(char *s, size_t max, const char *format,
              const struct tm_msec *tm)
{
    size_t n;

    /* Visual Studio 2013's behavior is to crash when 0 is passed as second
     * argument to strftime. */
    n = max ? strftime(s, max, format, &tm->tm) : 0;
    if (n) {
        char decimals[4];
        char *p;

        sprintf(decimals, "%03d", tm->msec);
        for (p = strchr(s, '#'); p; p = strchr(p, '#')) {
            char *d = decimals;
            while (*p == '#')  {
                *p++ = *d ? *d++ : '0';
            }
        }
    }

    return n;
}

struct tm_msec *gmtime_msec(long long int now, struct tm_msec *result)
{
    time_t now_sec = now / 1000;
    gmtime_r(&now_sec, &result->tm);
    result->msec = now % 1000;
    return result;
}

struct tm_msec *
localtime_msec(long long int now, struct tm_msec *result)
{
    time_t now_sec = now / 1000;
    localtime_r(&now_sec, &result->tm);
    result->msec = now % 1000;
    return result;
}


uint32_t get_unaligned_u32(const uint32_t *p_)
{
    const uint8_t *p = (const uint8_t *) p_;
    return ntohl((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

/* Returns the unicode code point corresponding to leading surrogate 'leading'
 * and trailing surrogate 'trailing'.  The return value will not make any
 * sense if 'leading' or 'trailing' are not in the correct ranges for leading
 * or trailing surrogates. */
int
utf16_decode_surrogate_pair(int leading, int trailing)
{
    /*
     *  Leading surrogate:         110110wwwwxxxxxx
     * Trailing surrogate:         110111xxxxxxxxxx
     *         Code point: 000uuuuuxxxxxxxxxxxxxxxx
     */
    int w = (leading >> 6) & 0xf;
    int u = w + 1;
    int x0 = leading & 0x3f;
    int x1 = trailing & 0x3ff;
    return (u << 16) | (x0 << 10) | x1;
}
