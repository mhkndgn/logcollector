#ifndef CONFIG_HANDLERS_H
#define CONFIG_HANDLERS_H

int cfg_key_plugin_file(char *filename, char *name, char *value_ptr);
int cfg_key_log_file(char *filename, char *name, char *value_ptr);
int cfg_key_udp_socket(char *filename, char *name, char *value_ptr);
int cfg_key_recv_buf_size(char *filename, char *name, char *value_ptr);
int cfg_key_tmp_folder(char *filename, char *name, char *value_ptr);
int cfg_key_temp_file_size(char *filename, char *name, char *value_ptr);
int cfg_key_json_folder(char *filename, char *name, char *value_ptr);
int cfg_key_json_file_size(char *filename, char *name, char *value_ptr);

int plugin_key_filename(char *value_ptr);
int plugin_key_plugin_id(char *value_ptr);
int plugin_key_listen_ip(char *value_ptr);
int plugin_key_core_id(char *value_ptr);
int plugin_key_all_nondedicated_cores(char *value_ptr);
int plugin_key_optimized(char *value_ptr);
int plugin_key_end(char *value_ptr);
int regex_key_regex_id(char *value_ptr);
int regex_key_description(char *value_ptr);
int regex_key_precheck(char *value_ptr);
int regex_key_regexp(char *value_ptr);
int regex_key_fields(char *value_ptr);

#endif
