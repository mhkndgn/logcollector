#ifndef CFG_H
#define CFG_H

#include "defines.h"
#include "common.h"

//Global variables
char *cfg[LARGEBUFLEN];
char *plgn[LARGEBUFLEN];
int rows;

/* structures */
struct _dictionary_line {
  char key[SRVBUFLEN];
  int (*func)(char *, char *, char *);
};

struct _plugin_line {
  char key[SRVBUFLEN];
  int (*func)(char *);
};


//Prototypes
int parse_configuration_file(char *filename);
void evaluate_configuration(char *filename, int rows);
void print_configuration(struct configuration c);
void sanitize_cfg(int rows, char *filename);
void print_plugin(struct plugin_t *p);
void print_regex(struct regex_t *r);
int parse_plugin_file(char *filename);
void evaluate_plugin(char *filename, int rows);
void sanitize_plugin(int rows, char *filename);
void trim_spaces(char *buf);
int isblankline(char *line);
int iscomment(char *line);

struct plugin_t *plugin_lookup_with_id(int id);
struct plugin_t *plugin_lookup_with_filename(char *filename);
struct plugin_t *plugin_lookup_with_ip(char * ip, int *index);
struct plugin_t *plugin_lookup_with_ip_dec(uint32_t ip, int *index);
struct regex_t *regex_lookup_with_id(struct plugin_t *p, int regex_id);
struct regex_t *regex_lookup_with_desc(struct plugin_t *p, char *desc);

#endif