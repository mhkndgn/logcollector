#ifndef DEFINES_H
#define DEFINES_H

#define LOG(x...) printf(x)
#define LOGF(x,y...) fprintf(x,y)

#define LOG_GREEN(x...)                                           \
        do {                                                                   \
		printf("\033[0;32m");                                    \
                printf(x);                                                       \
                printf("\033[0m");                                          \
	} while (0)

#define LOG_RED(x...)                                           \
        do {                                                                   \
		printf("\033[0;31m");                                    \
                printf(x);                                                       \
                printf("\033[0m");                                          \
	} while (0)

#define LOG_BLUE(x...)                                           \
        do {                                                                   \
		printf("\033[0;36m");                                    \
                printf(x);                                                       \
                printf("\033[0m");                                          \
	} while (0)

#define LOG_YELLOW(x...)                                           \
        do {                                                                   \
		printf("\033[0;33m");                                    \
                printf(x);                                                       \
                printf("\033[0m");                                          \
	} while (0)


#define ERROR_DEFAULT(x...) fprintf(stderr, x)

#define ERROR(x...)                     \
         do {                                                                   \
		ERROR_DEFAULT("[-] ERROR : " x);                              \
		ERROR_DEFAULT("\n\tLocation : %s(), %s:%u\n\n", __PRETTY_FUNCTION__,         \
		       __FILE__, __LINE__);                                         \
	} while (0)

#define ERROR_EXIT(x...)                                                 \
        do {                                                                   \
		ERROR_DEFAULT("[-] ERROR : " x);                              \
		ERROR_DEFAULT("\n\tLocation : %s(), %s:%u\n\n", __FUNCTION__,         \
		       __FILE__, __LINE__);                                    \
		exit(EXIT_FAILURE);                                             \
	} while (0)

#define ERROR_EXIT_RED(x...)                                                 \
        do {                                                                   \
		fprintf(stderr,"\033[0;31m");                                    \
                ERROR_DEFAULT("[-] ERROR : " x);                              \
                ERROR_DEFAULT("\n");                                    \
                fprintf(stderr,"\033[0m");                                          \
                exit(EXIT_FAILURE);                                             \
	} while (0)

#define FATAL(x...)                                                            \
	do {                                                                   \
		ERROR_DEFAULT("[-] PROGRAM ABORT : " x);                              \
		ERROR_DEFAULT("\n\tLocation : %s(), %s:%u\n\n", __FUNCTION__,         \
		       __FILE__, __LINE__);                                    \
		exit(EXIT_FAILURE);                                            \
	} while (0)

#define PFATAL(x...)                                                           \
	do {                                                                   \
		ERROR_DEFAULT("[-] SYSTEM ERROR : " x);                               \
		ERROR_DEFAULT("\n\tLocation : %s(), %s:%u\n", __FUNCTION__, __FILE__, \
		       __LINE__);                                              \
		perror("      OS message ");                                   \
		ERROR_DEFAULT("\n");                                                  \
		exit(EXIT_FAILURE);                                            \
	} while (0)

#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
#define TIMEVAL_NSEC(ts)                                                       \
	((ts)->tv_sec * 1000000000ULL + (ts)->tv_usec * 1000ULL)
#define NSEC_TIMESPEC(ns)                                                      \
	(struct timespec) { (ns) / 1000000000ULL, (ns) % 1000000000ULL }
#define NSEC_TIMEVAL(ns)                                                       \
	(struct timeval)                                                       \
	{                                                                      \
		(ns) / 1000000000ULL, ((ns) % 1000000000ULL) / 1000ULL         \
	}
#define MSEC_NSEC(ms) ((ms)*1000000ULL)


#define bull_fatal(str, ...) do {                                                      \
    fprintf(stderr, "%s:%d " str "\n", __FILE__, __LINE__, ##__VA_ARGS__);                  \
    exit(EXIT_FAILURE);                                                                \
} while(0)

#define bull_assert(expr) if (!(expr)) abort();
#define BULL_NOT_REACHED() abort()
#define BULL_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

/* true if x is a power of 2 */
#define POWEROF2(x) ((((x)-1) & (x)) == 0)

#define BYTE2GBIT(x) (x/(1024*1024*1024/8))

#define STR_CAT(size, str1, str2)                       \
        do {                                                                   \
            size_t size_str2 = strlen(str2);                            \
        	if ((size + size_str2) < MAX_SUB_STR_LEN) {       \
                memcpy(&str1[size], str2, size_str2);           \
                size += size_str2;                                                \
            }                               \
	} while (0)

#ifndef ERR
#define ERR -1
#endif
#ifndef SUCCESS
#define SUCCESS 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAX_CORE_NUM 48
#define MAIN_CORE 0

#define MTU_SIZE (2048-64*2)
#define MAX_MSG 1024
#define MAX_LOG_CNT 262144 //4096
#define MAX_REGEX_CNT 64
#define MAX_UDP_SOCKET_CNT 32
#define MAX_REGEX_FIELDS 32
#define MAX_PLUGIN_CNT 32
#define MAX_PLUGIN_CORE_CNT MAX_CORE_NUM
#define MAX_LOG_SIZE 4096
#define MAX_LOG_SIZE_TEMP 1024 + 128
#define MAX_LOG_BURST 64
#define GENERIC_LOG_PRECHECK "GENERIC_LOG"

#define REGEX_CFG_LINE_CNT 5

#define SRVBUFLEN 256
#define LARGEBUFLEN 8192
#define VERYSHORTBUFLEN 32
#define MAX_CMD_OUTPUT_LINE_NUM 1024
#define MAX_CMD_OUTPUT_LINE_LEN 1024
#define MAX_CMD_FIELD_NUM 32
#define MAX_CMD_FIELD_LEN 256
#define MAX_PCRE_SUB_STR_NUM 210
#define MAX_CUST_FUNC_RET_STR 256
#define MAX_SUB_STR_LEN 4096

#define ONE_HOUR_TO_MIN 60
#define ONE_MIN_TO_SEC 60
#define ONE_SEC_TO_USEC 1000.0
#define ONE_UHOUR ONE_HOUR_TO_MIN*ONE_MIN_TO_SEC*ONE_SEC_TO_USEC

#define MB_SIZE 1024*1024
#define GB_SIZE MB_SIZE*1024
#define HUN_MB_SIZE (uint64_t)MB_SIZE*99

#endif
