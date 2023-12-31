#ifndef BULL_DYNAMIC_STRING_H
#define BULL_DYNAMIC_STRING_H 1

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A "dynamic string", that is, a buffer that can be used to construct a
 * string across a series of operations that extend or modify it.
 *
 * The 'string' member does not always point to a null-terminated string.
 * Initially it is NULL, and even when it is nonnull, some operations do not
 * ensure that it is null-terminated.  Use ds_cstr() to ensure that memory is
 * allocated for the string and that it is null-terminated. */
struct ds {
    char *string;       /* Null-terminated string. */
    size_t length;      /* Bytes used, not including null terminator. */
    size_t allocated;   /* Bytes allocated, not including null terminator. */
};

#define DS_EMPTY_INITIALIZER { NULL, 0, 0 }

void ds_init(struct ds *);
void ds_clear(struct ds *);
void ds_truncate(struct ds *, size_t new_length);
void ds_reserve(struct ds *, size_t min_length);
char *ds_put_uninit(struct ds *, size_t n);
static inline void ds_put_char(struct ds *, char);
void ds_put_utf8(struct ds *, int uc);
void ds_put_char_multiple(struct ds *, char, size_t n);
void ds_put_buffer(struct ds *, const char *, size_t n);
void ds_put_cstr(struct ds *, const char *);
void ds_put_and_free_cstr(struct ds *, char *);
void ds_put_format(struct ds *, const char *, ...);
void ds_put_format_valist(struct ds *, const char *, va_list);

void ds_put_printable(struct ds *, const char *, size_t);
void ds_put_hex(struct ds *ds, const void *buf, size_t size);
void ds_put_hex_dump(struct ds *ds, const void *buf_, size_t size,
                     uintptr_t ofs, bool ascii);
int ds_get_line(struct ds *, FILE *);
int ds_get_preprocessed_line(struct ds *, FILE *, int *line_number);
int ds_get_test_line(struct ds *, FILE *);

void ds_put_strftime_msec(struct ds *, const char *format, long long int when,
                          bool utc);
char *xastrftime_msec(const char *format, long long int when, bool utc);

char *ds_cstr(struct ds *);
const char *ds_cstr_ro(const struct ds *);
char *ds_steal_cstr(struct ds *);
void ds_destroy(struct ds *);
void ds_swap(struct ds *, struct ds *);

int ds_last(const struct ds *);
bool ds_chomp(struct ds *, int c);
void ds_clone(struct ds *, struct ds *);

/* Inline functions. */

void ds_put_char__(struct ds *, char);

static inline void
ds_put_char(struct ds *ds, char c)
{
    if (ds->length < ds->allocated) {
        ds->string[ds->length++] = c;
        ds->string[ds->length] = '\0';
    } else {
        ds_put_char__(ds, c);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* dynamic-string.h */
