#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cfg.h"
#include "config-handlers.h"



static const struct _dictionary_line dictionary[] = {
        {"plugin_file", cfg_key_plugin_file},
        {"log_file", cfg_key_log_file},
        {"udp_socket", cfg_key_udp_socket},
        {"recv_buf_size", cfg_key_recv_buf_size},
        {"tmp_folder", cfg_key_tmp_folder},
        {"temp_file_size", cfg_key_temp_file_size},
        {"json_folder", cfg_key_json_folder},
        {"json_file_size", cfg_key_json_file_size},
	    {"", NULL}
};

static const struct _plugin_line plugin_dictionary[] = {
        {"filename", plugin_key_filename},
        {"plugin_id", plugin_key_plugin_id},
        {"listen_ip", plugin_key_listen_ip},
        {"core_id", plugin_key_core_id},
        {"all_nondedicated_cores", plugin_key_all_nondedicated_cores},
        {"optimized", plugin_key_optimized},
        {"end", plugin_key_end},
	{"", NULL}
};

static const struct _plugin_line regex_dictionary[] = {
        {"regex_id", regex_key_regex_id},
        {"description", regex_key_description},
        {"precheck", regex_key_precheck},
        {"regexp", regex_key_regexp},
        {"fields", regex_key_fields},
	{"", NULL}
};

#define REGEX_KEY_CNT 5
static const char *regex_keys[] = {"regex_id","description","precheck","regexp","fields"};

int iscomment(char *line)
{
  int len, j, first_char = TRUE;

  if (!line) return FALSE;

  len = strlen(line);
  for (j = 0; j <= len; j++) {
    if (!isspace(line[j])) first_char--;
    if (!first_char) {
      if (line[j] == '!' || line[j] == '#') return TRUE; 
      else return FALSE;
    }
  }

  return FALSE;
}

int isblankline(char *line)
{
  int len, j, n_spaces = 0;
 
  if (!line) return FALSE;

  len = strlen(line); 
  for (j = 0; j < len; j++) 
    if (isspace(line[j])) n_spaces++;

  if (n_spaces == len) return TRUE;
  else return FALSE;
}

void trim_spaces(char *buf)
{
  char *tmp_buf;
  int i, len;

  len = strlen(buf);

  tmp_buf = (char *)malloc(len + 1);
  if (tmp_buf == NULL) {
    fprintf(stderr, "ERROR: trim_spaces: malloc() failed.\n");
    return;
  }
   
  /* trimming spaces at beginning of the string */
  for (i = 0; i <= len; i++) {
    if (!isspace(buf[i])) {
      if (i != 0) { 
        strncpy(tmp_buf, &buf[i], len+1-i);
        strncpy(buf, tmp_buf, len+1-i);
      }
      break;
    } 
  }

  /* trimming spaces at the end of the string */
  for (i = strlen(buf)-1; i >= 0; i--) { 
    if (isspace(buf[i]))
      buf[i] = '\0';
    else break;
  }

  free(tmp_buf);
}

void print_configuration(struct configuration c)
{
    printf("-------------------------------\n");
    printf("log_file: %s \ntmp_folder: %s \njson_folder: %s \nrecv_buf_size: %d\n",
        c.log_file,c.tmp_folder,c.json_folder,c.recv_buf_size);
    for(int i=0; i <c.plugin_file_cnt; i++){
        printf("plugin_file: %s \n",c.plugin_file[i]);
    }
    for(int i=0; i < c.listen_addr_cnt; i++)
        printf("listen_addr: %s [%d of %d]\n",c.listen_addr_str[i],i,c.listen_addr_cnt);
    printf("-------------------------------\n");
}

void sanitize_cfg(int rows, char *filename)
{
  int rindex = 0, len;
  char localbuf[10240];

  while (rindex < rows) {
    memset(localbuf, 0, 10240);

    /* checking the whole line: if it's a comment starting with
       '!', it will be removed */
    if (iscomment(cfg[rindex])) memset(cfg[rindex], 0, strlen(cfg[rindex]));

    /* checking the whole line: if it's void, it will be removed */
    if (isblankline(cfg[rindex])) memset(cfg[rindex], 0, strlen(cfg[rindex]));

    /* 
       a pair of syntax checks on the whole line:
       - does the line contain at least a ':' verb ?
       - are the square brackets weighted both in key and value ?
    */
    len = strlen(cfg[rindex]);
    if (len) {
      if (!strchr(cfg[rindex], ':')) {
	fprintf(stderr, "ERROR: [%s:%u] Syntax error: missing ':'. Exiting.\n", filename, rindex+1); 
	exit(1);
      }
    }

    /* checking the whole line: erasing unwanted spaces from key;
       trimming start/end spaces from value; symbols will be left
       untouched */
    len = strlen(cfg[rindex]);
    if (len) {
      int symbol = FALSE, value = FALSE, cindex = 0, lbindex = 0;
      char *valueptr = NULL;

      while(cindex <= len) {
	if (!value) {
          if (cfg[rindex][cindex] == '[') symbol++;
          else if (cfg[rindex][cindex] == ']') symbol--;
	  else if (cfg[rindex][cindex] == ':') {
	    value++;
	    valueptr = &localbuf[lbindex+1];
	  }
	}
        if ((!symbol) && (!value)) {
	  if (!isspace(cfg[rindex][cindex])) {
	    localbuf[lbindex] = cfg[rindex][cindex]; 
	    lbindex++;
	  }
        }
        else {
	  localbuf[lbindex] = cfg[rindex][cindex];
	  lbindex++;
        }
        cindex++;
      }
      localbuf[lbindex] = '\0';
      trim_spaces(valueptr);
      strcpy(cfg[rindex], localbuf);
    }

    /* checking key field: does a key still exist ? */
    len = strlen(cfg[rindex]);
    if (len) {
      if (cfg[rindex][0] == ':') {
	fprintf(stderr, "ERROR: [%s:%u] Syntax error: missing key. Exiting.\n", filename, rindex+1);
	exit(1);
      }
    }

    /* checking key field: converting key to lower chars */ 
    len = strlen(cfg[rindex]);
    if (len) {
      int symbol = FALSE, cindex = 0;

      while(cindex <= len) {
        if (cfg[rindex][cindex] == '[') symbol++;
	else if (cfg[rindex][cindex] == ']') symbol--;

	if (cfg[rindex][cindex] == ':') break;
	if (!symbol) {
	  if (isalpha(cfg[rindex][cindex]))
	    cfg[rindex][cindex] = tolower(cfg[rindex][cindex]);
	}
	cindex++;
      }
    }

    rindex++;
  }
}

void evaluate_configuration(char *filename, int rows)
{
  char *key, *value, *name, *delim;
  int index = 0, dindex, valid_line, key_found = 0, res;

  while (index < rows) {
    if (*cfg[index] == '\0') valid_line = FALSE;
    else valid_line = TRUE; 

    if (valid_line) {

      /* splitting key, value and name */
      delim = strchr(cfg[index], ':');
      *delim = '\0';
      key = cfg[index];
      value = delim+1;

      delim = strchr(key, '[');
      if (delim) {
        *delim = '\0';
        name = delim+1;
        delim = strchr(name, ']');
        *delim = '\0';
      }
      else name = NULL;

      /* parsing keys */
      for (dindex = 0; strcmp(dictionary[dindex].key, ""); dindex++) {
        if (!strcmp(dictionary[dindex].key, key)) {
	  res = FALSE;
          if ((*dictionary[dindex].func)) {
	    res = (*dictionary[dindex].func)(filename, name, value);
	    if (res < 0) fprintf(stdout,  "WARN: [%s:%u] Invalid value. Ignored.\n", filename, index+1);
	  }
	  else fprintf(stdout, "WARN: [%s:%u] Unable to handle key: %s. Ignored.\n", filename, index+1, key);
	  key_found = TRUE;
	  break;
        }
	else key_found = FALSE;
      }

      if (!key_found) fprintf(stdout, "WARN: [%s:%u] Unknown key: %s. Ignored.\n", filename, index+1, key);
    }

    index++;
  }
}

void set_default_config_values() {
    config.json_file_size = 100 * MB_SIZE;
    config.temp_file_size = 100 * MB_SIZE;
    config.tmp_folder = "";
    config.json_folder = "";
}

int parse_configuration_file(char *filename)
{
  char localbuf[10240];
  FILE *file;
  int num = 0, idx;
  rows = 0;
  
  /* NULL filename means we don't have a configuration file; 1st stage: read from
     file and store lines into a first char* array; merge commandline options, if
     required, placing them at the tail - in order to override directives placed
     in the configuration file */
  if (filename) { 
    if ((file = fopen(filename,"r")) == NULL) {
      fprintf(stderr, "ERROR: [%s] file not found.\n", filename);
      return ERR;
    }
    else {
      while (!feof(file)) {
        if (rows == LARGEBUFLEN) {
	  fprintf(stderr, "ERROR: [%s] maximum number of %d lines reached.\n", filename, LARGEBUFLEN);
	  break;
        }
	memset(localbuf, 0, sizeof(localbuf));
        if (fgets(localbuf, sizeof(localbuf), file) == NULL) break;	
        else {
	  localbuf[sizeof(localbuf)-1] = '\0';
          cfg[rows] = malloc(strlen(localbuf)+2);
	  if (!cfg[rows]) {
	    fprintf(stderr, "ERROR: [%s] malloc() failed (parse_configuration_file). Exiting.\n", filename);
	    exit(1);
	  }
          strcpy(cfg[rows], localbuf);
          cfg[rows][strlen(localbuf)+1] = '\0';
          rows++;
        } 
      }
    }
    fclose(file);
  }


  /* 2nd stage: sanitize lines */
  sanitize_cfg(rows, filename);

  set_default_config_values();
  
  /* 5th stage: parsing keys and building configurations */ 
  evaluate_configuration(filename, rows);

  return SUCCESS;
}

void print_plugin(struct plugin_t *p)
{
    printf("-------------------------------\n");
    printf("filename: %s \nplugin_id: %d \nlisten_ip: %s \n",p->filename,p->plugin_id,p->ip);
    printf("core_ids: ");
    for(int i=0; i < p->core_id_num; i++){
        printf("%d ",p->core_ids[i]);
    }
    printf("\nregexes: \n\n");
    for(int i=0; i < p->regex_cnt; i++){
        print_regex(&p->regex[i]);
    }
    printf("-------------------------------\n");
}

void print_regex(struct regex_t *r)
{
    printf("id: %d \ndescription: %s \nprecheck: %s \nregexp: %s \nfields: ",
            r->id,r->description,r->precheck,r->regexp);
    for(int k=0; k < r->field_cnt; k++){
        printf("key: %s findby: %s\n",r->fields[k].key,r->fields[k].findby);
    }
    printf("\n\n");
}

void sanitize_plugin(int rows, char *filename)
{
  int rindex = 0, len;
  char localbuf[10240];

  while (rindex < rows) {
    memset(localbuf, 0, 10240);

    /* checking the whole line: if it's a comment starting with
       '!', it will be removed */
    if (iscomment(plgn[rindex])) memset(plgn[rindex], 0, strlen(plgn[rindex]));

    /* checking the whole line: if it's void, it will be removed */
    if (isblankline(plgn[rindex])) memset(plgn[rindex], 0, strlen(plgn[rindex]));

    /* 
       a pair of syntax checks on the whole line:
       - does the line contain at least a ':' verb ?
       - are the square brackets weighted both in key and value ?
    */
    len = strlen(plgn[rindex]);
    if (len) {
      if (!strchr(plgn[rindex], ':')) {
	fprintf(stderr, "ERROR: [%s:%u] Syntax error: missing ':'. Exiting.\n", filename, rindex+1); 
	exit(1);
      }
    }

    /* checking the whole line: erasing unwanted spaces from key;
       trimming start/end spaces from value; symbols will be left
       untouched */
    len = strlen(plgn[rindex]);
    if (len) {
      int symbol = FALSE, value = FALSE, cindex = 0, lbindex = 0;
      char *valueptr = NULL;

      while(cindex <= len) {
	if (!value) {
          if (plgn[rindex][cindex] == '[') symbol++;
          else if (plgn[rindex][cindex] == ']') symbol--;
	  else if (plgn[rindex][cindex] == ':') {
	    value++;
	    valueptr = &localbuf[lbindex+1];
	  }
	}
        if ((!symbol) && (!value)) {
	  if (!isspace(plgn[rindex][cindex])) {
	    localbuf[lbindex] = plgn[rindex][cindex]; 
	    lbindex++;
	  }
        }
        else {
	  localbuf[lbindex] = plgn[rindex][cindex];
	  lbindex++;
        }
        cindex++;
      }
      localbuf[lbindex] = '\0';
      trim_spaces(valueptr);
      strcpy(plgn[rindex], localbuf);
    }

    /* checking key field: does a key still exist ? */
    len = strlen(plgn[rindex]);
    if (len) {
      if (plgn[rindex][0] == ':') {
	fprintf(stderr, "ERROR: [%s:%u] Syntax error: missing key. Exiting.\n", filename, rindex+1);
	exit(1);
      }
    }

    /* checking key field: converting key to lower chars */ 
    len = strlen(plgn[rindex]);
    if (len) {
      int symbol = FALSE, cindex = 0;

      while(cindex <= len) {
        if (plgn[rindex][cindex] == '[') symbol++;
	else if (plgn[rindex][cindex] == ']') symbol--;

	if (plgn[rindex][cindex] == ':') break;
	if (!symbol) {
	  if (isalpha(plgn[rindex][cindex]))
	    plgn[rindex][cindex] = tolower(plgn[rindex][cindex]);
	}
	cindex++;
      }
    }

    rindex++;
  }
}

void evaluate_plugin(char *filename, int rows)
{
    char *key, *value, *name, *delim;
    int index = 0, pd_index, rd_index, valid_line, plugin_key_found = 0, res;
    int rgx_index = 0, regex_key_found = 0;

    while (index < rows) {
        if (*plgn[index] == '\0') valid_line = FALSE;
        else valid_line = TRUE; 

        if (valid_line) {

            /* splitting key, value and name */
            delim = strchr(plgn[index], ':');
            *delim = '\0';
            key = plgn[index];
            value = delim+1; 

        
      // parsing keys 
            for (pd_index = 0; strcmp(plugin_dictionary[pd_index].key, ""); pd_index++) {
                if (!strcmp(plugin_dictionary[pd_index].key, key)) {
                    res = FALSE;
                    if ((*plugin_dictionary[pd_index].func)) {
                        res = (*plugin_dictionary[pd_index].func)(value);
                        if (res < 0){
                            fprintf(stdout,  "WARN: [%s:%u] Invalid value. Ignored.\n", filename, index+1);
                        }
                    }
                    else{
                        fprintf(stdout, "WARN: [%s:%u] Unable to handle key: %s  Ignored.\n", filename, index+1, key);
                    }
                    plugin_key_found = TRUE;
                    break;
                }
                else{
                    plugin_key_found = FALSE;
                }
            }

            for (rd_index = 0; strcmp(regex_dictionary[rd_index].key, ""); rd_index++) {
                if (!strcmp(regex_dictionary[rd_index].key, key)) { // check if key is in regex dictionary
                    
                    if (!strcmp(regex_keys[rgx_index], key)) { // check regex keys with order
                        rgx_index++;
                    }

                    res = FALSE;
                    if ((*regex_dictionary[rd_index].func)) {
                        res = (*regex_dictionary[rd_index].func)(value);
                        if (res < 0){
                            fprintf(stdout,  "WARN: [%s:%u] Invalid value. Ignored.\n", filename, index+1);
                            exit(1);
                        }
                    }
                    else{
                        fprintf(stdout, "WARN: [%s:%u] Unable to handle key: %s. Ignored.\n", filename, index+1, key);
                        exit(1);
                    }
                    if(rgx_index == REGEX_KEY_CNT)
                        rgx_index = 0;
                        
                    regex_key_found = TRUE;
                    break;
                }
                else{
                    regex_key_found = FALSE;
                }
            }

            if ((!plugin_key_found) && (!regex_key_found)) fprintf(stdout, "WARN: [%s:%u] Unknown key: %s  Ignored.\n", filename, index+1, key);
            
    }

    index++;
  }
}


int parse_plugin_file(char *filename)
{
  char localbuf[10240];
  FILE *file;
  int num = 0, idx;
  rows = 0;
  
  /* NULL filename means we don't have a configuration file; 1st stage: read from
     file and store lines into a first char* array; merge commandline options, if
     required, placing them at the tail - in order to override directives placed
     in the configuration file */
  if (filename) { 
    if ((file = fopen(filename,"r")) == NULL) {
      fprintf(stderr, "ERROR: [%s] file not found.\n", filename);
      return ERR;
    }
    else {
      while (!feof(file)) {
        if (rows == LARGEBUFLEN) {
	  fprintf(stderr, "ERROR: [%s] maximum number of %d lines reached.\n", filename, LARGEBUFLEN);
	  break;
        }
	memset(localbuf, 0, sizeof(localbuf));
        if (fgets(localbuf, sizeof(localbuf), file) == NULL) break;	
        else {
	  localbuf[sizeof(localbuf)-1] = '\0';
          plgn[rows] = malloc(strlen(localbuf)+2);
	  if (!plgn[rows]) {
	    fprintf(stderr, "ERROR: [%s] malloc() failed (parse_plugin_file). Exiting.\n", filename);
	    exit(1);
	  }
          strcpy(plgn[rows], localbuf);
          plgn[rows][strlen(localbuf)+1] = '\0';
          rows++;
        } 
      }
    }
    fclose(file);
  }

  /* 2nd stage: sanitize lines */
  sanitize_plugin(rows, filename);

  /* 5th stage: parsing keys and building configurations */ 
  evaluate_plugin(filename, rows);

  return SUCCESS;
}

struct plugin_t *plugin_lookup_with_id(int id)
{
    struct plugin_t *p = NULL;
    for(int i=0; i < plugin_cnt; i++){
        if(plugin[i].plugin_id == id)
        {
            p = & plugin[i];
        }
    }
    return p;
}

struct plugin_t *plugin_lookup_with_ip(char * ip, int *index)
{
    struct plugin_t *p = NULL;
    for(int i=0; i < plugin_cnt; i++){
        if(!strcmp(plugin[i].ip, ip))
        {
            p = & plugin[i];
            *index = i;
            break;
        }
    }
    return p;
}

struct plugin_t *plugin_lookup_with_ip_dec(uint32_t ip, int *index)
{
    struct plugin_t *p = NULL;
    for(int i=0; i < plugin_cnt; i++){
        if(plugin[i].ip_dec == ip)
        {
            p = &plugin[i];
            *index = i;
            break;
        }
    }
    return p;
}

struct plugin_t *plugin_lookup_with_filename(char *filename)
{
    struct plugin_t *p = NULL;
    for(int i=0; i < plugin_cnt; i++){
        if(!strcmp(plugin[i].filename, filename))
        {
            p = & plugin[i];
            break;
        }
    }
    return p;
}

struct regex_t *regex_lookup_with_id(struct plugin_t *p, int regex_id)
{
    struct regex_t *r = NULL;
    for(int k=0; k < p->regex_cnt; k++){
        if(p->regex[k].id == regex_id)
        {
            r = &p->regex[k];
        }
    }
    return r;
}

struct regex_t *regex_lookup_with_desc(struct plugin_t *p, char *desc)
{
    struct regex_t *r = NULL;
    for(int k=0; k < p->regex_cnt; k++){
        if(!strcmp(p->regex[k].description, desc))
        {
            r = &p->regex[k];
        }
    }
    return r;
}

