#ifndef CUSTOM_FUNCTIONS_H
#define CUSTOM_FUNCTIONS_H

#include <pcre.h>

#define CF_MAX_PCRE_SUB_STR_NUM 90
#define SRVBUFLEN 256

struct _dictionary_item {
  char key[SRVBUFLEN];
  int (*func)(pcre *, pcre_extra *, pcre_jit_stack *, const char *, char *);
  char regex[SRVBUFLEN];
};

struct regex_handle{
    int (*func)(pcre *, pcre_extra *, pcre_jit_stack *, const char *, char *);
    pcre *compiled;
    pcre_extra *extra;
};

struct citrix_dictionary{
    char key[SRVBUFLEN];
    char value[32];
};

int normalize_date_fortigate(pcre *compiled, pcre_extra *extra, pcre_jit_stack *jit_stack, const char * date, char *ret_str);
int fortigate_sid(pcre *compiled, pcre_extra *extra, pcre_jit_stack *jit_stack, const char * attack_id, char *ret_str);
int normalize_protocol(pcre *compiled, pcre_extra *extra, pcre_jit_stack *jit_stack, const char * protocol, char *ret_str);
int normalize_date_citrix(pcre *compiled, pcre_extra *extra, pcre_jit_stack *jit_stack, const char * date, char *ret_str);
int translate_citrix(pcre *compiled, pcre_extra *extra, pcre_jit_stack *jit_stack, const char * key, char *ret_str);

extern struct _dictionary_item cf_dictionary[];

#endif