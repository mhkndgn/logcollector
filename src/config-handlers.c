#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "config-handlers.h"
#include "common.h"
#include <arpa/inet.h>


static int64_t
parse_decimal(const char *str)
{
    char *end = NULL;
    uint64_t num;

    num = strtoull(str, &end, 10);
    if ((str[0] == '\0') || (end == NULL) || (*end != '\0')
        || num > INT64_MAX)
        return -1;

    return num;
}

int cfg_key_plugin_file(char *filename, char *name, char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"plugin_file is not specified \n");
    	return ERR;
    }
    else{
    	config.plugin_file[config.plugin_file_cnt] = value_ptr;
    	config.plugin_file_cnt++;
    	return SUCCESS;
    }
}

int cfg_key_log_file(char *filename, char *name, char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"log_file is not specified \n");
    	return ERR;
    }
    else{
    	config.log_file = value_ptr;
    	return SUCCESS;
    }
}

int cfg_key_udp_socket(char *filename, char *name, char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"udp_socket is not specified \n");
    	return ERR;
    }
    else{
    	config.listen_addr_str[config.listen_addr_cnt] = value_ptr;
    	config.listen_addr_cnt ++; 
    	return SUCCESS;
    }
}

int cfg_key_recv_buf_size(char *filename, char *name, char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"recv_buf_size is not specified \n");
    	return ERR;
    }
    else{
    	config.recv_buf_size = parse_decimal(value_ptr);
    	return SUCCESS;
    }
}

int cfg_key_tmp_folder(char *filename, char *name, char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"temporary files' folder is not specified \n");
    	return ERR;
    }
    else{
        if(value_ptr[strlen(value_ptr) - 1] == '/'){
            config.tmp_folder = value_ptr;
        }
        else{
            strcat(value_ptr,"/");
            config.tmp_folder = value_ptr;
        }
    	return SUCCESS;
    }
}

int cfg_key_temp_file_size(char *filename, char *name, char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
        fprintf(stderr,"temp file size' is not specified Using default value: 100MB\n");
        config.temp_file_size = HUN_MB_SIZE;
        fprintf(stderr,"temp file size' is specified Using size value: %ld\n", config.temp_file_size);
        return SUCCESS;
    }
    else{
        if(strstr(value_ptr, "MB") != NULL){
            char * token = strtok(value_ptr, "MB");
            config.temp_file_size = parse_decimal(token)*MB_SIZE;
        }
        else if (strstr(value_ptr, "GB") != NULL){
            char * token = strtok(value_ptr, "GB");
            config.temp_file_size = parse_decimal(token)*GB_SIZE;
        }else {
            fprintf(stderr,"temp file size' is not specified wrong! Using default value: 100MB\n");
            config.temp_file_size = HUN_MB_SIZE;
            return SUCCESS;
        }
        return SUCCESS;
    }
}


int cfg_key_json_folder(char *filename, char *name, char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"json files' folder is not specified \n");
    	return ERR;
    }
    else{
        if(value_ptr[strlen(value_ptr) - 1] == '/'){
            config.json_folder = value_ptr;
        }
        else{
            strcat(value_ptr,"/");
            config.json_folder = value_ptr;
        }
    	return SUCCESS;
    }
}

int cfg_key_json_file_size(char *filename, char *name, char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
        fprintf(stderr,"json file size' is not specified Using default value: 100MB\n");
        config.json_file_size = HUN_MB_SIZE;
        return SUCCESS;
    }
    else{
        if(strstr(value_ptr, "MB") != NULL){
            char * token = strtok(value_ptr, "MB");
            config.json_file_size = parse_decimal(token)*MB_SIZE;
        }
        else if (strstr(value_ptr, "GB") != NULL){
            char * token = strtok(value_ptr, "GB");
            config.json_file_size = parse_decimal(token)*GB_SIZE;
        }else {
            fprintf(stderr,"json file size' is not specified wrong! Using default value: 100MB\n");
            config.json_file_size = HUN_MB_SIZE;
            return SUCCESS;
        }
        return SUCCESS;
    }
}

int plugin_key_filename(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"filename is not specified \n");
    	return ERR;
    }
    else{
    	plugin[plugin_cnt].filename = value_ptr;
    	return SUCCESS;
    }
}

int plugin_key_plugin_id(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"plugin_id is not specified \n");
    	return ERR;
    }
    else{
    	plugin[plugin_cnt].plugin_id = parse_decimal(value_ptr);
    	return SUCCESS;
    }
}

int plugin_key_listen_ip(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"listen_ip is not specified \n");
    	return ERR;
    }
    else{
    	plugin[plugin_cnt].ip = value_ptr;
        struct in_addr addr;
        if (inet_aton(value_ptr, &addr) == 1)
            plugin[plugin_cnt].ip_dec = addr.s_addr;
    	return SUCCESS;
    }
}

int plugin_key_core_id(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"core_id is not specified \n");
    	return ERR;
    }
    else{
        char  *delim, *temp, *temp2;
        delim = strchr(value_ptr,'[');
        int field_num = 0;
        while(delim){

        	*delim = '\0';
        	temp = delim + 1;
        	
        	delim = strchr(temp, ']');
        	
        	*delim = '\0';
        	temp2 = delim+1;

                plugin[plugin_cnt].core_ids[plugin[plugin_cnt].core_id_num] = parse_decimal(temp);
                plugin[plugin_cnt].core_id_num++;
        	
        	temp = temp2;
        	field_num++;
        	delim = strchr(temp,'[');
        }
    	return SUCCESS;
    }
}

int plugin_key_all_nondedicated_cores(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
        fprintf(stderr,"all_nondedicated_cores is not specified \n");
        return ERR;
    }else if(!strcmp(value_ptr, "true")) {
        plugin[plugin_cnt].all_nondedicated_cores = 1;
        return SUCCESS;
    }else if (!strcmp(value_ptr, "false")){
        plugin[plugin_cnt].all_nondedicated_cores = 0;
        return SUCCESS;
    }else {
        fprintf(stderr,"all_nondedicated_cores is not set correctly. Please type \"true\" or \"false\" \n");
        return ERR;
    }
}

int plugin_key_optimized(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
        fprintf(stderr,"optimized is not specified \n");
        return ERR;
    }else if(!strcmp(value_ptr, "true")) {
        plugin[plugin_cnt].optimized = true;
        return SUCCESS;
    }else if (!strcmp(value_ptr, "false")){
        plugin[plugin_cnt].optimized = false;
        return SUCCESS;
    }else {
        fprintf(stderr,"optimized is not set correctly. Please type \"true\" or \"false\" \n");
        return ERR;
    }
}

int plugin_key_end(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"end is not specified \n");
    	return ERR;
    }
    else{
        plugin_cnt++;
    	return SUCCESS;
    }
}


int regex_key_regex_id(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"regex_id is not specified \n");
    	return ERR;
    }
    else{
    	plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].id= parse_decimal(value_ptr);
    	return SUCCESS;
    }
}

int regex_key_description(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"description is not specified \n");
    	return ERR;
    }
    else{
        plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].description = value_ptr;
    	return SUCCESS;
    }
}

int regex_key_precheck(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"precheck is not specified in plugin with id: %d and regex with id: %d\n",
    	    plugin[plugin_cnt].plugin_id,
    	    plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].id);
    	plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].precheck = value_ptr;
    	return SUCCESS;
    }
    else{
        plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].precheck = value_ptr;
    	return SUCCESS;
    }
}

int regex_key_regexp(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"regexp is not specified \n");
    	return ERR;
    }
    else{
        plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].regexp = value_ptr;
    	return SUCCESS;
    }
}

int regex_key_fields(char *value_ptr)
{
    if(!strcmp(value_ptr, "")){
    	fprintf(stderr,"fields is not specified \n");
    	return ERR;
    }//-f /home/ahmet-genc/CLion-B-ull/logcollector/src/config.conf -m 3 -c 5,6,7 -k 545055525903075400565D575400025F5E56000156020D00050602015A00505751555F06
    else{
        char  *delim, *temp, *temp2, *temp3, *temp4, *temp5, *temp6, *temp7;
        delim = strchr(value_ptr,'[');
        int field_num = 0;
        while(delim){
            //plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].func_name == NULL;

        	*delim = '\0';
        	temp = delim + 1;
        	
        	delim = strchr(temp, ':');
        	
        	*delim = '\0';
        	temp2 = delim+1;
            plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].key = temp;

            delim = strchr(temp2, ']');
            *delim = '\0';
            temp3 = delim + 1;
            plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].findby = temp2;
            plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].is_value = false;

            delim = strchr(temp2, ':');
            if (delim != NULL) {
                *delim = '\0';
                temp6 = delim + 1;
                delim = strchr(temp6, ':');
                if (delim != NULL) {
                    *delim = '\0';
                    temp7 = delim + 1;
                    if (!strcmp("integer", temp7))
                        plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].is_integer = true;

                    if (*temp6 != 0)
                        plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].func_name= temp6;

                } else {
                    if (*temp6 != 0)
                        plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].func_name= temp6;
                    delim = strchr(temp7, '"');
                }
            } else
                plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].is_integer = false;

            delim = strchr(temp2, '"');
            if (delim != NULL) {
                *delim = '\0';
                temp5 = delim+1;
                delim = strchr(temp5, '"');
                *delim = '\0';
                plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].findby = temp5;
                plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].is_value = true;
                delim = strchr(temp3, ']');
            }

            delim = strchr(temp2, ':');
            if (delim != NULL) {
                *delim = '\0';
                temp4 = delim+1;
                plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].findby = temp2;
                plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].func_name = temp4;
                plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].is_value = false;
                temp4 = NULL;
            } else{
                //plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].fields[field_num].func_name = NULL;
                delim = strchr(temp3, ']');
            }

            temp = temp3;
            field_num++;
        	delim = strchr(temp3,'[');
        }
        plugin[plugin_cnt].regex[plugin[plugin_cnt].regex_cnt].field_cnt = field_num;
        
        plugin[plugin_cnt].regex_cnt++;
    	return SUCCESS;
    }
}