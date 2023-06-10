#ifndef BULL_UTIL_H
#define BULL_UTIL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

/* This is a void expression that issues a compiler error if POINTER cannot be
 * compared for equality with the given pointer TYPE.  This generally means
 * that POINTER is a qualified or unqualified TYPE.  However,
 * BUILD_ASSERT_TYPE(POINTER, void *) will accept any pointer to object type,
 * because any pointer to object can be compared for equality with "void *".
 *
 * POINTER can be any expression.  The use of "sizeof" ensures that the
 * expression is not actually evaluated, so that any side effects of the
 * expression do not occur.
 *
 * The cast to int is present only to suppress an "expression using sizeof
 * bool" warning from "sparse" (see
 * http://permalink.gmane.org/gmane.comp.parsers.sparse/2967). */
#define BUILD_ASSERT_TYPE(POINTER, TYPE) \
    ((void) sizeof ((int) ((POINTER) == (TYPE) (POINTER))))

/* Casts 'pointer' to 'type' and issues a compiler warning if the cast changes
 * anything other than an outermost "const" or "volatile" qualifier. */
#define CONST_CAST(TYPE, POINTER)                               \
    (BUILD_ASSERT_TYPE(POINTER, TYPE),                          \
     (TYPE) (POINTER))

#define BULL_TYPEOF(OBJECT) __typeof__(OBJECT)
#define OBJECT_OFFSETOF(OBJECT, MEMBER) offsetof(__typeof__(*(OBJECT)), MEMBER)

/* Given POINTER, the address of the given MEMBER within an object of the type
 * that that OBJECT points to, returns OBJECT as an assignment-compatible
 * pointer type (either the correct pointer type or "void *").  OBJECT must be
 * an lvalue.
 *
 * This is the same as CONTAINER_OF except that it infers the structure type
 * from the type of '*OBJECT'. */
#define OBJECT_CONTAINING(POINTER, OBJECT, MEMBER)                      \
    ((BULL_TYPEOF(OBJECT)) (void *)                                      \
     ((char *) (POINTER) - OBJECT_OFFSETOF(OBJECT, MEMBER)))

/* Given POINTER, the address of the given MEMBER within an object of the type
 * that that OBJECT points to, assigns the address of the outer object to
 * OBJECT, which must be an lvalue.
 *
 * Evaluates to (void) 0 as the result is not to be used. */
#define ASSIGN_CONTAINER(OBJECT, POINTER, MEMBER) \
    ((OBJECT) = OBJECT_CONTAINING(POINTER, OBJECT, MEMBER), (void) 0)

/* As explained in the comment above OBJECT_OFFSETOF(), non-GNUC compilers
 * like MSVC will complain about un-initialized variables if OBJECT
 * hasn't already been initialized. To prevent such warnings, INIT_CONTAINER()
 * can be used as a wrapper around ASSIGN_CONTAINER. */
#define INIT_CONTAINER(OBJECT, POINTER, MEMBER) \
    ((OBJECT) = NULL, ASSIGN_CONTAINER(OBJECT, POINTER, MEMBER))

#define BUILD_ASSERT__(EXPR) \
        sizeof(struct { unsigned int build_assert_failed : (EXPR) ? 1 : -1; })
#define BUILD_ASSERT(EXPR) (void) BUILD_ASSERT__(EXPR)
#define BUILD_ASSERT_DECL(EXPR) extern int (*build_assert(void))[BUILD_ASSERT__(EXPR)]

#ifndef MIN
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#endif

#define ROUND_DOWN(X, Y) ((X) / (Y) * (Y))

/* Given POINTER, the address of the given MEMBER in a STRUCT object, returns
   the STRUCT object. */
#define CONTAINER_OF(POINTER, STRUCT, MEMBER)                           \
        ((STRUCT *) (void *) ((char *) (POINTER) - offsetof (STRUCT, MEMBER)))

#define BULL_SOURCE_LOCATOR __FILE__ ":" BULL_STRINGIZE(__LINE__)
#define BULL_STRINGIZE(ARG) BULL_STRINGIZE2(ARG)
#define BULL_STRINGIZE2(ARG) #ARG

void *xmalloc(size_t);
void *xcalloc(size_t, size_t);
void *xzalloc(size_t);
void *xrealloc(void *, size_t);
void *x2nrealloc(void *p, size_t *n, size_t s);
void *xmemdup(const void *, size_t);
char *xmemdup0(const char *, size_t);
char *xstrdup(const char *);
char *nullable_xstrdup(const char *);
bool nullable_string_is_equal(const char *a, const char *b);
char *xvasprintf(const char *format, va_list args);

bool str_to_double(const char *s, double *d);
uintmax_t hexits_value(const char *s, size_t n, bool *ok);
char *xasprintf(const char *format, ...);
void out_of_memory(void);

uint32_t get_unaligned_u32(const uint32_t *p_);

/* The C standards say that neither the 'dst' nor 'src' argument to
 * memcpy() may be null, even if 'n' is zero.  This wrapper tolerates
 * the null case. */
static inline void nullable_memcpy(void *dst, const void *src, size_t n)
{
    if (n) {
        memcpy(dst, src, n);
    }
}

/* Returns true if X is a power of 2, otherwise false. */
#define IS_POW2(X) ((X) && !((X) & ((X) - 1)))

static inline bool
is_pow2(uintmax_t x)
{
    return IS_POW2(x);
}

int utf16_decode_surrogate_pair(int leading, int trailing);

#ifdef __cplusplus
}
#endif

#endif
