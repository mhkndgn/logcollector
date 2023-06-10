#define _GNU_SOURCE // for recvmmsg

#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <uuid/uuid.h>

#include <pcre.h>
#include <string.h>

#include "common.h"
#include "defines.h"
#include "cfg.h"
#include "ring.h"
#include "json.h"
#include "hash.h"

#include "custom_functions.h"

#define TEMP_FILE_OPS 1
#define JSON_FILE_OPS 1

#define STR_CAT(size, str1, str2)                       \
        do {                                                                   \
        	if ((size + strlen(str2)) < MAX_SUB_STR_LEN) {       \
        	    char blank[2] = " ";                                                 \
                    memcpy(&str1[size], str2, strlen(str2));           \
                    size += strlen(str2);                                                \
                    memcpy(&str1[size], blank, strlen(blank));           \
                    size += strlen(blank);                                                \
                }                               \
	} while (0)

FILE *dbg_fptr;
int debug_enabled;
int debug_level = 1;

static uint64_t core_mask = MAX_CORE_NUM;
static uint64_t master_core_mask = MAX_CORE_NUM;
static uint64_t master_receiver_core_mask = MAX_CORE_NUM;
static uuid_t uuid_g;
core_t core_conf[MAX_CORE_NUM];
int nb_cores;
int nb_receiver_cores;
int nb_slaves;
int nb_masters;

bool key_verified = false;
bool kill_signal = false;

struct thread **sthreads;
struct thread **mthreads;
struct thread **rthreads;

static void
signal_handler(int signum) {

    if (signum == SIGINT) {
        kill_signal = true;
    }
}

static int initialize_custom_functions()
{
    const char *pcreErrorStr;
    int pcreErrorOffset;
    int dindex = 0;
    
    hash_map = xmalloc(sizeof *hash_map);
    shash_init(hash_map);
    
    for (dindex = 0; strcmp(cf_dictionary[dindex].key, ""); dindex++) {

        char *name = cf_dictionary[dindex].key;

        struct regex_handle *handle = calloc(1, sizeof(struct regex_handle));
        handle->func = cf_dictionary[dindex].func;
        handle->compiled = pcre_compile(cf_dictionary[dindex].regex, 0, &pcreErrorStr, &pcreErrorOffset, NULL);
        if (handle->compiled == NULL) {
            fprintf(stderr,"Could not compile regex '%s': %s \n", cf_dictionary[dindex].regex, pcreErrorStr);
            return ERR;
        }
        handle->extra = pcre_study(handle->compiled, PCRE_STUDY_JIT_COMPILE, &pcreErrorStr);
        if (pcreErrorStr != NULL) {
            fprintf(stderr,"Could not study '%s': %s\n", cf_dictionary[dindex].regex, pcreErrorStr);
            return ERR;
        }
        
        shash_add_once(hash_map, name, handle);
    }

    return SUCCESS;
}

static int custom_function(pcre_jit_stack *jit_stack, char *func, const char *value, char *ret_str)
{
    struct shash_node *node;
    node = shash_find(hash_map, func);
    if(node != NULL){
        struct regex_handle *handle = node->data; 
        return (*handle->func)(handle->compiled, handle->extra, jit_stack, value, ret_str);
    }
    else
        return ERR;
}

static void destroy_hash_map(struct shash *object){
    struct shash_node *node, *next;

    SHASH_FOR_EACH_SAFE (node, next, object) {
        struct regex_handle *value = node->data;

        free(value);
        shash_delete(object, node);
    }
    shash_destroy(object);
    free(object);
}

static int compile_and_study_regexes(struct plugin_t *p) {
    const char *pcreErrorStr;
    int pcreErrorOffset;

    for (int i = 0; i < p->regex_cnt; i++) {
        pcre **compiled = &p->regex[i].compiled;
        pcre_extra **extra = &p->regex[i].extra;
        char **regexp = &p->regex[i].regexp;

        *compiled = pcre_compile(*regexp, 0, &pcreErrorStr, &pcreErrorOffset, NULL);
        if (unlikely(*compiled == NULL)) {
            ERROR("Could not compile '%s': %s - i: %d \n", *regexp, pcreErrorStr, i);
            return ERR;
        }

        *extra = pcre_study(*compiled, PCRE_STUDY_JIT_COMPILE, &pcreErrorStr);
        if (unlikely(pcreErrorStr != NULL)) {
            ERROR("Could not study '%s': %s\n", *regexp, pcreErrorStr);
            return ERR;
        }
    }

    return SUCCESS;
}

static int match_with_pcre_jit(struct regex_t *rgx, pcre_jit_stack *jit_stack,
                               int ovector[MAX_PCRE_SUB_STR_NUM], char *buf) {
    int pcreErrorOffset;
    int pcreExecRet = 0;
    int ret = 0;

    pcre_assign_jit_stack(rgx->extra, NULL, jit_stack);

#if 1
    pcreExecRet = pcre_jit_exec(rgx->compiled,
                                rgx->extra,
                                buf,
                                strlen(buf),        // length of string
                                0,                      // Start looking at this point
                                0,                      // OPTIONS
                                ovector,
                                MAX_PCRE_SUB_STR_NUM,
                                jit_stack);
#else
    pcreExecRet = pcre_exec(rgx->compiled,
                            rgx->extra,
                            buf,
                            strlen(buf),        // length of string
                            0,                      // Start looking at this point
                            0,                      // OPTIONS
                            ovector,
                            MAX_PCRE_SUB_STR_NUM);
#endif

    if (unlikely(pcreExecRet < 0)) { // Something bad happened..
        switch (pcreExecRet) {
            case PCRE_ERROR_NOMATCH      :
                break;
            case PCRE_ERROR_NULL         :
            case PCRE_ERROR_BADOPTION    :
            case PCRE_ERROR_BADMAGIC     :
            case PCRE_ERROR_UNKNOWN_NODE :
            case PCRE_ERROR_NOMEMORY     :
                break;
            default:
                break;
        }
    } else {
        //printf("Result: We have a match!number of matches: %d \n",pcreExecRet);

        if (pcreExecRet == 0) {
            //printf("But too many substrings were found to fit in subStrVec!\n");
            // Set rc to the max number of substring matches possible.
            pcreExecRet = MAX_PCRE_SUB_STR_NUM / 3;
        }
        
        for (int i = 0; i < pcreExecRet; i++)
        {
            //char *substring_start = buf + ovector[2*i];
            int substring_length = ovector[2*i+1] - ovector[2*i];
            //printf("%d: %.*s", i, substring_length, substring_start);
            //printf(" substring_length %d \n",substring_length);
            if(substring_length > 0) // matches with 0 length does not count
                ret++;
        }
    }

    return ret;
}

static int find_suitable_regexes(struct plugin_t *plgn, int **regex_ids, char *buf) {
    int count = 0;
    for (int i = 0; i < plgn->regex_cnt; i++) {
        struct regex_t *rgx = &plgn->regex[i];
        if (strstr(buf, rgx->precheck) != NULL) {
            *regex_ids[i] = rgx->id;
            count++;
        }
    }
    for (int i = 0; i < count; i++) {
        printf("count: %d regex_ids[%d]: %d \n", count, i, *regex_ids[i]);
    }
    return count;
}

static int set_log_files_directory(struct plugin_t *plgn, struct tm *tm, char *folder, char **fullpath)
{
    char directory[1024];
    if(folder != NULL){
        memcpy (directory, folder, strlen(folder)+1 );
        strcat(directory, plgn->ip);
    }
    else{
        memcpy (directory, plgn->ip, strlen(plgn->ip)+1 );
    }

    struct stat st = {0};
    if (stat(directory, &st) == -1) {
        int ret = mkdir(directory, 0777);
        if (ret < 0) {
            ERROR("Folder creation failed with name : %s", directory);
            return ERR;
        }
    }

    char year[10];
    sprintf(year, "%d", (tm->tm_year + 1900));
    strcat(directory, "/");
    strcat(directory, year);

    if (stat(directory, &st) == -1) {
        int ret = mkdir(directory, 0777);
        if (ret < 0) {
            ERROR("Folder creation failed with name : %s", directory);
            return ERR;
        }
    }

    char month[10];
    sprintf(month, "%d", (tm->tm_mon + 1));
    strcat(directory, "/");
    strcat(directory, month);

    if (stat(directory, &st) == -1) {
        int ret = mkdir(directory, 0777);
        if (ret < 0) {
            ERROR("Folder creation failed with name : %s", directory);
            return ERR;
        }
    }

    char day[10];
    sprintf(day, "%d", (tm->tm_mday));
    strcat(directory, "/");
    strcat(directory, day);

    if (stat(directory, &st) == -1) {
        int ret = mkdir(directory, 0777);
        if (ret < 0) {
            ERROR("Folder creation failed with name : %s", directory);
            return ERR;
        }
    }

    *fullpath = strdup(directory);

    return SUCCESS;
}
static int set_temp_file_ptr(struct plugin_t *plgn, struct tm *tm, char *new_file_name) {
    char *log_folder_path;
    char full_path[1280];
    char *folder = config.tmp_folder;

    pthread_mutex_lock(&plgn->t_file->file_lock);
    if(set_log_files_directory(plgn, tm, folder, &log_folder_path) < 0){
        return ERR;
    }
    sprintf(full_path, "%s/%s", log_folder_path, new_file_name);

    if (plgn->t_file->prev_fptr == NULL && plgn->t_file->prev_ref == 0) { // if new file doesn't exists
        plgn->t_file->prev_fptr = plgn->t_file->fptr;
        plgn->t_file->fptr = fopen(full_path, "a");
        if(plgn->t_file->fptr == NULL)
            ERROR_EXIT("Cannot open new temp file with name: %s \n",full_path);
        __sync_and_and_fetch(&plgn->t_file->file_size, 0);
        plgn->t_file->prev_ref = plgn->t_file->ref -1;
        plgn->t_file->ref = 1;
        //printf("coreid: %d plgn->t_file->prev_ref: %d new_ref: %d\n",coreid, plgn->t_file->prev_ref, plgn->t_file->ref);
    }
    else{
        //printf("coreid: %d plgn->t_file->prev_ref: %d new_ref: %d\n",coreid, plgn->t_file->prev_ref, plgn->t_file->ref);
        plgn->t_file->ref ++;
        plgn->t_file->prev_ref--;
    }

    if(plgn->t_file->prev_ref == 0){ // if there is only one thread 
        //printf("coreid: %d close file: %p \n",coreid, plgn->t_file->prev_fptr);
        fflush(plgn->t_file->prev_fptr);
        fclose(plgn->t_file->prev_fptr);
        plgn->t_file->prev_fptr = NULL;
    }
    pthread_mutex_unlock(&plgn->t_file->file_lock);
    
    free(log_folder_path);
    return SUCCESS;
}

static int create_temp_log_file(struct plugin_t *plgn, struct tm *tm, char *new_file_name) {
    char *log_folder_path;
    char full_path[1280];
    char *folder = config.tmp_folder;

    pthread_mutex_lock(&plgn->t_file->file_lock);
    if(set_log_files_directory(plgn, tm, folder, &log_folder_path) < 0){
        return ERR;
    }
    sprintf(full_path, "%s/%s", log_folder_path, new_file_name);

    if (likely(plgn->t_file->fptr == NULL)) {
        plgn->t_file->fptr = fopen(full_path, "a");
        if(plgn->t_file->fptr == NULL)
            return ERR;
            
        plgn->t_file->ref = 1;
        plgn->t_file->prev_ref = 0;
    }
    else{
        // somebody opened the file before so do nothing fptr is not NULL
        plgn->t_file->ref++;
    }
    pthread_mutex_unlock(&plgn->t_file->file_lock);
    
    free(log_folder_path);
    return SUCCESS;
}


static int set_json_file_ptr(struct plugin_t *plgn, struct tm *tm, char *new_file_name, int coreid) {
    char *log_folder_path;
    char full_path[1280];
    char *folder = config.json_folder;

    pthread_mutex_lock(&plgn->j_file->file_lock);
    if(set_log_files_directory(plgn, tm, folder, &log_folder_path) < 0){
        return ERR;
    }
    sprintf(full_path, "%s/%s", log_folder_path, new_file_name);

    /*printf("coreid: %d plgn->j_file: %p fptr: %p prev_fptr: %p file_lock: %p new_ref: %d prev_ref: %d file_size: %lu \n",
        coreid, plgn->j_file,plgn->j_file->fptr,plgn->j_file->prev_fptr, &plgn->j_file->file_lock,
        plgn->j_file->ref, plgn->j_file->prev_ref,__sync_fetch_and_add(&plgn->j_file->file_size,0));
    */
    if (plgn->j_file->prev_fptr == NULL && plgn->j_file->prev_ref == 0) { // if new file doesn't exists
        plgn->j_file->prev_fptr = plgn->j_file->fptr;
        plgn->j_file->fptr = fopen(full_path, "a");
        if(plgn->j_file->fptr == NULL)
            ERROR_EXIT("Cannot open new json file with name: %s \n",full_path);
        __sync_and_and_fetch(&plgn->j_file->file_size, 0);
        plgn->j_file->prev_ref = plgn->j_file->ref -1;
        plgn->j_file->ref = 1;
        //printf("coreid: %d plgn->j_file->prev_ref: %d new_ref: %d\n",coreid, plgn->j_file->prev_ref, plgn->j_file->ref);
    }
    else{
        //printf("coreid: %d plgn->j_file->prev_ref: %d new_ref: %d\n",coreid, plgn->j_file->prev_ref, plgn->j_file->ref);
        plgn->j_file->ref ++;
        plgn->j_file->prev_ref--;
    }

    if(plgn->j_file->prev_ref == 0){ // if there is only one thread 
        //printf("coreid: %d close file: %p \n",coreid, plgn->j_file->prev_fptr);
        fflush(plgn->j_file->prev_fptr);
        fclose(plgn->j_file->prev_fptr);
        plgn->j_file->prev_fptr = NULL;
    }

    /*printf("coreid: %d plgn->j_file: %p fptr: %p prev_fptr: %p file_lock: %p new_ref: %p prev_ref: %p file_size: %p \n",
        coreid, plgn->j_file,plgn->j_file->fptr,plgn->j_file->prev_fptr, &plgn->j_file->file_lock,
        &plgn->j_file->ref, &plgn->j_file->prev_ref,&plgn->j_file->file_size);
    */
    pthread_mutex_unlock(&plgn->j_file->file_lock);
    
    free(log_folder_path);
    return SUCCESS;
}

static int create_json_log_file(struct plugin_t *plgn, struct tm *tm, char *new_file_name, int coreid) {
    char *log_folder_path;
    char full_path[1280];
    char *folder = config.json_folder;

    pthread_mutex_lock(&plgn->j_file->file_lock);
    if(set_log_files_directory(plgn, tm, folder, &log_folder_path) < 0){
        return ERR;
    }
    sprintf(full_path, "%s/%s", log_folder_path, new_file_name);

    if(plgn->j_file->fptr == NULL){ // first time to open the file
        plgn->j_file->fptr = fopen(full_path, "a");
        if(plgn->j_file->fptr == NULL)
            return ERR;
            
        plgn->j_file->ref = 1;
        plgn->j_file->prev_ref = 0;
    }
    else{ // somebody opened file before
        plgn->j_file->ref++;
    }
    /*printf("coreid: %d plgn->j_file: %p fptr: %p prev_fptr: %p file_lock: %p new_ref: %p prev_ref: %p file_size: %p \n",
        coreid, plgn->j_file,plgn->j_file->fptr,plgn->j_file->prev_fptr, &plgn->j_file->file_lock,
        &plgn->j_file->ref, &plgn->j_file->prev_ref,&plgn->j_file->file_size);
    */
    pthread_mutex_unlock(&plgn->j_file->file_lock);
    
    free(log_folder_path);
    return SUCCESS;
}

static int create_debug_file(char *filename){
    dbg_fptr = fopen(filename, "w");
    if(dbg_fptr == NULL)
        return ERR;
    else
        return SUCCESS;
}

static inline uint64_t get_cycles()
{
    uint64_t t;
    __asm volatile ("rdtsc" : "=A"(t));
    return t;
}

static uint64_t drain_write_buffer(struct write_buffer *wb, FILE *fptr, int free_buffers){
    uint64_t len = 0;

    for(int i=0; i <wb->cnt; i++){
        int tmplen = strlen(wb->buffer[i]);
        len +=tmplen;
        //fprintf(fptr, "%s", wb->buffer[i]);
        fwrite(wb->buffer[i],1,tmplen,fptr);
        if(free_buffers)
            free(wb->buffer[i]);
    }
    wb->cnt = 0;
    return len;
}

static uint64_t drain_write_buffer_json(struct write_buffer *wb, FILE *fptr, int free_buffers){
    uint64_t len = 0;

    for(int i=0; i <wb->cnt; i++){
        int tmplen = strlen(wb->buffer[i]);
        len +=tmplen;
        fprintf(fptr, "%s\n", wb->buffer[i]);
        //fwrite(wb->buffer[i],1,tmplen,fptr);
        if(free_buffers)
            free(wb->buffer[i]);
    }
    wb->cnt = 0;
    return len;
}

static void *worker_loop(void *thread_data) {
    struct thread *th = thread_data;
    printf("\nworker_loop with core_id: %d\n", th->core_id);
    struct timeval t1, t2, t3;
    struct timeval tf1, tf2;
    struct timeval timestamp;
    time_t current_time;
    double diff, diff_file, diff_av;
    double second_pkt_len = 0.0;
    double total_len = 0.0;
    double av_total_speed = 0.0;
    int pcount = 1;

    printf("assigned plugin ids for this core with core_id: %d : ", th->core_id);
    for (int k = 0; k < th->pcnt; k++) {
        printf("%d ", th->plugins[k]->plugin_id);
    }
    printf("\n");

    struct write_buffer **wb = calloc(th->pcnt, sizeof(struct write_buffer)); 
    for (int t = 0; t < th->pcnt; t++) {
        *(wb + t) = calloc(1, sizeof(struct write_buffer));
    }

    current_time = time(NULL);
    struct tm *tm = localtime(&current_time);

    for (int i = 0; i < th->pcnt; i++) {
        struct plugin_t *plgn = th->plugins[i]; 
        if (plgn == NULL) {
            ERROR_EXIT("plugin with ID: %d not found!", th->plugins[i]->plugin_id);
        }
        gettimeofday(&timestamp, NULL);
        char new_file_name[100];
        memset(new_file_name, 0, sizeof(new_file_name));
        sprintf(new_file_name, "%s_%02d:%02d:%02d_%ld.json", plgn->ip, tm->tm_hour, tm->tm_min, tm->tm_sec,timestamp.tv_usec%1000);

        if (create_json_log_file(plgn, tm, new_file_name,th->core_id) < 0)
            ERROR_EXIT("Cannot create json log file.");

        for (int i = 0; i < plgn->regex_cnt; i++) {
            struct regex_t *rgx = &plgn->regex[i];
            if(!strcmp(rgx->precheck, "")){ 
                plgn->empty_p_regex[plgn->empty_rcnt] = rgx;
                plgn->empty_rcnt++;
            }
            else if(!strcmp(rgx->precheck, GENERIC_LOG_PRECHECK)) {
                plgn->generic_p_regex[plgn->generic_rcnt] = rgx;
                plgn->generic_rcnt++;
            }
            else {
                plgn->filled_p_regex[plgn->filled_rcnt] = rgx;
                plgn->filled_rcnt++;
            }
        }
        /*
        for(int i=0; i<plgn->empty_rcnt; i++)
            printf("empty rgx id: %d \n",plgn->empty_p_regex[i]->id);
        for(int i=0; i<plgn->generic_rcnt; i++)
            printf("generic rgx id: %d \n",plgn->generic_p_regex[i]->id);
        for(int i=0; i<plgn->filled_rcnt; i++)
            printf("filled rgx id: %d \n",plgn->filled_p_regex[i]->id);
        */
    }
    

    gettimeofday(&t1, NULL);
    gettimeofday(&t3, NULL);
    gettimeofday(&tf1, NULL);
    while (th->pcnt > 0) {
        for (int i = 0; i < th->pcnt; i++) {
            char *buf;
            struct lbuf *pkt;
            int rgx_ids[MAX_REGEX_CNT];
            int rgx_sub_vectors[MAX_REGEX_CNT][MAX_PCRE_SUB_STR_NUM];
            //int suit_cnt = 0;
            int best_rgx_index = 0;
            int best_match = 0;
            int generic_rgx_ids[MAX_REGEX_CNT];
            int generic_rgx_cnt = 0;

            int suit_cnt = 0;
            struct regex_t *suit_rgxs[MAX_REGEX_CNT];
            struct regex_t *best_rgx;

            struct plugin_t *plgn = th->plugins[i]; //plugin_lookup_with_id(th->plugin_ids[i]);
            if (plgn == NULL) {
                ERROR_EXIT("plugin with ID: %d not found!", th->plugins[i]->plugin_id);
            }
            for (int master_index=0; master_index<nb_masters; master_index++) {
                if (ring_dequeue(plgn->ring_rxs[th->core_id][master_index], (void **) &pkt) == 0) { //msg received
                    buf = pkt->log;
                    
                    if(plgn->optimized){
                        for(int i=0; i<plgn->filled_rcnt; i++) {
                            struct regex_t *rgx = plgn->filled_p_regex[i];
                            if (strstr(buf, rgx->precheck) != NULL) { // non-empty precheck
                                if(suit_cnt < MAX_REGEX_CNT){
                                    suit_rgxs[suit_cnt] = rgx;
                                    suit_cnt++;
                                }
                            }
                        }
                    }
                    else{
                        for (int i = 0; i < plgn->filled_rcnt; i++) {
                            struct regex_t *rgx = plgn->filled_p_regex[i];
                            if (strstr(buf, rgx->precheck) != NULL) { //add both empty and non-empty suitable precheck regexes
                                if(suit_cnt < MAX_REGEX_CNT){
                                    suit_rgxs[suit_cnt] = rgx;
                                    suit_cnt++;
                                }
                            }
                        }
                        for(int i = 0; i < plgn->empty_rcnt; i++) {
                            suit_rgxs[suit_cnt] = plgn->empty_p_regex[i];
                            suit_cnt++;
                        }
                    }

                    // match suitable regexes
                    for (int r_index = 0; r_index < suit_cnt; r_index++) {
                        struct regex_t *rgx = suit_rgxs[r_index];
                        int ret = match_with_pcre_jit(rgx, th->jit_stack, rgx_sub_vectors[r_index], buf);
                        if (ret > best_match) {
                            best_match = ret;
                            best_rgx = rgx;
                            best_rgx_index = r_index;
                        }
                    }
                    // if no rgx is matched try generic regexes
                    if(best_match == 0){ 
                        suit_cnt = 0;
                        for (int r_index = 0; r_index < plgn->generic_rcnt; r_index++) {
                            struct regex_t *rgx = plgn->generic_p_regex[i];
                            suit_rgxs[suit_cnt] = plgn->generic_p_regex[i];
                            suit_cnt++;
                            int ret = match_with_pcre_jit(rgx, th->jit_stack, rgx_sub_vectors[r_index], buf);
                            if (ret > best_match) {
                                best_match = ret;
                                best_rgx = rgx;
                                best_rgx_index = r_index;
                            }
                        }
                    }

                    struct json *jason = json_object_create();
                    char sub_strings[MAX_SUB_STR_LEN] = {0};
                    size_t size = 0;

                    if (likely(best_match > 0)) {
                        struct regex_t *rgx = best_rgx;
                        
                        for (int i = 0; i < rgx->field_cnt; i++) {
                            if (!rgx->fields[i].is_value) {
                                const char *psubStrMatchStr;

                                int len = pcre_get_named_substring(rgx->compiled, buf, rgx_sub_vectors[best_rgx_index],
                                                                   best_match, rgx->fields[i].findby,
                                                                   &(psubStrMatchStr));
                                if (likely(len >= 0)) {
                                    if (rgx->fields[i].func_name != NULL) {
                                        char *func_ret_str = calloc(1, MAX_CUST_FUNC_RET_STR);
                                        if(custom_function(th->jit_stack, rgx->fields[i].func_name,psubStrMatchStr,func_ret_str) < 0)
                                            strcpy(func_ret_str, psubStrMatchStr);

                                        STR_CAT(size,sub_strings,func_ret_str);
                                        
                                        if(rgx->fields[i].is_integer)
                                            json_object_put(jason, rgx->fields[i].key, json_integer_create(strtol(func_ret_str,NULL,10)));
                                        else
                                            json_object_put_string(jason, rgx->fields[i].key, func_ret_str);
                                            
                                        pcre_free_substring(psubStrMatchStr);
                                        free(func_ret_str);
                                    } else {
                                        STR_CAT(size,sub_strings,psubStrMatchStr);
                                        
                                        if(rgx->fields[i].is_integer)
                                            json_object_put(jason, rgx->fields[i].key, json_integer_create(strtol(psubStrMatchStr,NULL,10)));
                                        else
                                            json_object_put_string(jason, rgx->fields[i].key, psubStrMatchStr);
                                        pcre_free_substring(psubStrMatchStr);
                                    }
                                } else {
                                    //printf("len: %d \n",len);
                                }
                            }else{
                                if(rgx->fields[i].is_integer)
                                    json_object_put(jason, rgx->fields[i].key, json_integer_create(strtol(rgx->fields[i].findby,NULL,10)));
                                else
                                    json_object_put_string(jason, rgx->fields[i].key, rgx->fields[i].findby);

                                STR_CAT(size,sub_strings,rgx->fields[i].findby);
                            }
                        }
                        json_object_put(jason, "regex_matched", json_boolean_create(true)); // finally add "regex_matched": "true"
                        STR_CAT(size,sub_strings,"true");

                        json_object_put_string(jason, "matched_regex_name", rgx->description);
                        STR_CAT(size,sub_strings,rgx->description);
                    }
                    else{
                        json_object_put(jason, "regex_matched", json_boolean_create(false));// no match for any regex for this log
                        STR_CAT(size,sub_strings,"false");
                    }
                    
                    json_object_put_string(jason, "raw_log", buf);
                    STR_CAT(size,sub_strings,buf);

                    json_object_put(jason, "plugin_id", json_integer_create(plgn->plugin_id));
                    char plgn_id_str[20] = {0};
                    sprintf(plgn_id_str,"%d",plgn->plugin_id);
                    STR_CAT(size,sub_strings,plgn_id_str);
                    
                    uint32_t hash = hash_string(sub_strings, 0);
                    char hash_str[20];
                    sprintf(hash_str, "%u", hash);
                    json_object_put_string(jason, "siemplus_hash", hash_str);

                    if(debug_enabled && debug_level == 2){
                        char suit_rgx_indxs[1024] = {0};
                        for (int r_index = 0; r_index < suit_cnt; r_index++) {
                            char id[5] = {0};    
                            snprintf(id, 5, "%d ", rgx_ids[r_index]);
                            strcat(suit_rgx_indxs,id);
                        }
                        json_object_put_format(jason, "debug", 
                            "BRI: %d, BM: %d, SC: %d, SRI: %s",
                            rgx_ids[best_rgx_index],best_match,suit_cnt,suit_rgx_indxs);
                    }

#if JSON_FILE_OPS == 1
                    char *json_str = json_to_string(jason, 0); // string returned from json_to_string must be freed
                    if (wb[i]->cnt >= MAX_LOG_BURST) {
                        uint64_t size = drain_write_buffer_json(wb[i], plgn->j_file->fptr, TRUE);
                        __sync_add_and_fetch(&plgn->j_file->file_size, size);
                    }
                    
                    wb[i]->buffer[wb[i]->cnt] = json_str;
                    wb[i]->cnt++;
#endif
                    json_destroy(jason);

                    if ((__sync_fetch_and_add(&plgn->j_file->file_size, 0) > config.json_file_size)) {
                        gettimeofday(&timestamp, NULL);
                        current_time = time(NULL);
                        tm = localtime(&current_time);
                        char new_file_name[100];
                        sprintf(new_file_name, "%s_%02d:%02d:%02d_%ld.json", plgn->ip, tm->tm_hour, tm->tm_min,
                                tm->tm_sec, timestamp.tv_usec % 1000);
                        set_json_file_ptr(plgn, tm, new_file_name, th->core_id);
                    }

                    pcount++;
                    second_pkt_len += strlen(buf);
                    total_len += strlen(buf);
                    free(pkt);
                }
            }

            gettimeofday(&t2, NULL);
            diff = (t2.tv_sec - t1.tv_sec) * 1000.0;
            diff += (t2.tv_usec - t1.tv_usec) / 1000.0;

            if (diff > ONE_SEC_TO_USEC) {
                current_time = time(NULL);
                tm = localtime(&current_time);
                diff_av = (t2.tv_sec - t3.tv_sec) * 1000.0;
                diff_av += (t2.tv_usec - t3.tv_usec) / 1000.0;
                av_total_speed = BYTE2GBIT(total_len) / (diff_av / ONE_SEC_TO_USEC);

                if(debug_enabled){
                    LOGF(dbg_fptr,"|%d-%02d-%02d %02d:%02d:%02d| -> ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                            tm->tm_hour,
                            tm->tm_min, tm->tm_sec);
                    LOGF(dbg_fptr,"worker_loop [tid: %d][plgn_id: %d][speed: %.2f Gbit/s][%d pkts/s][buffers:",
                            th->core_id, plgn->plugin_id, BYTE2GBIT(second_pkt_len) * (diff / 1000.0),
                            pcount - 1);  
                    
                    for (int master_index=0; master_index<nb_masters; master_index++) {
                        LOGF(dbg_fptr,"(%d %u)",master_index, ring_count(plgn->ring_rxs[th->core_id][master_index]));
                        if (unlikely(kill_signal == true)) {
                            //printf("Signal term received from CPU (%d)\n", th->core_id);
                            if (ring_count(plgn->ring_rxs[th->core_id][master_index]) == 0)
                                goto exit;
                        }
                    }
                    LOGF(dbg_fptr,"]\n");
                    
                    fflush(dbg_fptr);
                }
                else{
                    LOG("|%d-%02d-%02d %02d:%02d:%02d| -> ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                            tm->tm_hour,
                            tm->tm_min, tm->tm_sec);
                    LOG("worker_loop [tid: %d][plgn_id: %d][speed: %.2f Gbit/s][%d pkts/s][buffers:",
                            th->core_id, plgn->plugin_id, BYTE2GBIT(second_pkt_len) * (diff / 1000.0),
                            pcount - 1);  
                    
                    for (int master_index=0; master_index<nb_masters; master_index++) {
                        LOG("(%d %u)",master_index, ring_count(plgn->ring_rxs[th->core_id][master_index]));
                        if (unlikely(kill_signal == true)) {
                            //printf("Signal term received from CPU (%d)\n", th->core_id);
                            if (ring_count(plgn->ring_rxs[th->core_id][master_index]) == 0)
                                goto exit;
                        }
                    }
                    LOG("]\n");
                }
           
                pcount = 1;
                second_pkt_len = 0.0;
                gettimeofday(&t1, NULL);
            }

            gettimeofday(&tf2, NULL);
            diff_file = (tf2.tv_sec - tf1.tv_sec) * 1000.0;
            diff_file += (tf2.tv_usec - tf1.tv_usec) / 1000.0;

            if (diff_file > ONE_SEC_TO_USEC) { // check if it is necessary to create a new json file every second.

                if(wb[i]->cnt > 0){
                    uint64_t size = drain_write_buffer_json(wb[i], plgn->j_file->fptr, TRUE);
                    __sync_add_and_fetch(&plgn->j_file->file_size, size);
                }
                current_time = time(NULL);
                tm = localtime(&current_time);
                
                if (unlikely(tm->tm_min == 00 && tm->tm_sec == 00)) {
                    gettimeofday(&timestamp, NULL);
                    char new_file_name[100];
                    sprintf(new_file_name, "%s_%02d:%02d:%02d_%ld.json", plgn->ip, tm->tm_hour, tm->tm_min, tm->tm_sec,timestamp.tv_usec%1000);
                    set_json_file_ptr(plgn, tm, new_file_name,th->core_id);
                }
                gettimeofday(&tf1, NULL);
            }
        }
    }
    LOG_GREEN("thread with id %d is finished! You didnt set any plugin to this core.\n", th->core_id);
    exit:
    if (kill_signal == true)
        LOG_GREEN("thread with id %d is killed! Waited until all rings were emptied!\n", th->core_id);

    // all freeing operations in here 
    for (int t = 0; t < plugin_cnt; t++) {
        free(*(wb + t));
    }
    free(wb);
    
    pthread_cancel(pthread_self());
}

static void *receiver_loop(void *thread_data) {
    struct thread *thread = thread_data;
    struct net_addr listen_addr;
    struct timeval t1, t2;
    struct tm *tm;
    time_t current_time;    
    int main_fd = -1;
    double diff;
    double sec_tot_pkt_len = 0.0;
    uint64_t sec_pkt_cnt = 0;
    uint64_t tot_pkt_cnt = 0;
    uint64_t tot_free_pkt_cnt = 0;

    printf("receiver_loop core_id: %d\n",thread->core_id);

    parse_addr(&listen_addr, config.listen_addr_str[0]);
    if (main_fd == -1) {
        main_fd = net_bind_udp(&listen_addr, 1);
        net_set_buffer_size(main_fd, config.recv_buf_size, 0);
    }

    struct lbuf *pkt = (struct lbuf *)calloc(1,sizeof(struct lbuf));
    if(pkt == NULL)
        ERROR_EXIT("unable to allocate memory");

    gettimeofday(&t1, NULL);
    while (1) {
        struct sockaddr_in server_addr, client_addr;
        int client_struct_length = sizeof(client_addr);

        if (unlikely(kill_signal == true)) {
            printf("Signal term received from CPU(receiver_loop) (%d)\n", thread->core_id);
            break;
        }

        /* Blocking recv. no blocking: replace 0 with MSG_DONTWAIT*/
        int len = recvfrom(main_fd, pkt->log, MAX_MSG, MSG_DONTWAIT,
                           (struct sockaddr *) &pkt->client_addr, &client_struct_length);

        if (len <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                //continue;
            }
            //LOG_RED("error occurred while recvfrom() for core : %d \n", thread->thread_index);
        } 
        else {
            //enqueue pkt lbuf to ring
            int ring_index = rand() % receiver_ring_cnt;
            int rslt = ring_enqueue(receiver_rings[ring_index], pkt);
            if(rslt < 0) {
                free(pkt);
                tot_free_pkt_cnt++;
            }

            sec_tot_pkt_len += len;
            tot_pkt_cnt++;
            sec_pkt_cnt++;

            pkt = (struct lbuf *)calloc(1,sizeof(struct lbuf));
            if(pkt == NULL)
                ERROR_EXIT("unable to allocate memory");
        }

        gettimeofday(&t2, NULL);
        diff = (t2.tv_sec - t1.tv_sec) * 1000.0;
        diff += (t2.tv_usec - t1.tv_usec) / 1000.0;
        if (diff > ONE_SEC_TO_USEC) {
            current_time = time(NULL);
            tm = localtime(&current_time);

            if(debug_enabled){
                LOGF(dbg_fptr,"|%d-%02d-%02d %02d:%02d:%02d| -> ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
                    tm->tm_min, tm->tm_sec);

                LOGF(dbg_fptr,"recvr_loop   [tid: %d][speed: %.2f Gbit/s][%lu pkts/s][tot_pkt: %lu][tot_free: %lu]\n",
                        thread->core_id, BYTE2GBIT(sec_tot_pkt_len), sec_pkt_cnt, tot_pkt_cnt, tot_free_pkt_cnt);
                fflush(dbg_fptr);
            }
            else{
                LOG("|%d-%02d-%02d %02d:%02d:%02d| -> ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
                    tm->tm_min, tm->tm_sec);
                if(sec_pkt_cnt > 1)
                    LOG_BLUE("recvr_loop  [tid: %d][speed: %.2f Gbit/s][%lu pkts/s][tot_pkt: %lu][tot_free: %lu]\n",
                        thread->core_id, BYTE2GBIT(sec_tot_pkt_len), sec_pkt_cnt, tot_pkt_cnt, tot_free_pkt_cnt);
                else
                    LOG_RED("recvr_loop  [tid: %d][speed: %.2f Gbit/s][%lu pkts/s][tot_pkt: %lu][tot_free: %lu]\n",
                        thread->core_id, BYTE2GBIT(sec_tot_pkt_len), sec_pkt_cnt, tot_pkt_cnt, tot_free_pkt_cnt);
            }

            sec_tot_pkt_len = 0.0;
            sec_pkt_cnt = 0;
            gettimeofday(&t1, NULL);
        }
    }

    LOG_GREEN("thread receiver with id %d is killed!\n", thread->core_id);
    pthread_cancel(pthread_self());
}

static void *main_loop(void *thread_data) {

    struct thread *thread = thread_data;
    struct plugin_t *plgn;
    struct ring *ring = NULL;
    struct tm *tm;
    struct timeval t1, t2;
    struct timeval tf1, tf2;
    struct timeval timestamp;
    time_t current_time;
    double diff, diff_file;
    uint64_t sec_pkt_cnt = 0;
    uint64_t tot_pkt_cnt = 0;
    uint64_t tot_free_pkt_cnt = 0;

    printf("main_loop core_id: %d \n",thread->core_id);

    struct write_buffer **wb = calloc(plugin_cnt, sizeof(struct write_buffer)); 
    for (int t = 0; t < plugin_cnt; t++) {
        *(wb + t) = calloc(1, sizeof(struct write_buffer));
    }

    current_time = time(NULL);
    tm = localtime(&current_time);
    for (int i = 0; i < plugin_cnt; ++i) {
        plgn = &plugin[i];
        char new_file_name[100];
        gettimeofday(&timestamp, NULL);
        snprintf(new_file_name, sizeof(new_file_name), "%s_%02d:%02d:%02d_%ld.temp", plgn->ip, tm->tm_hour, tm->tm_min, tm->tm_sec,timestamp.tv_usec%1000);
        
        if (create_temp_log_file(plgn, tm, new_file_name) < 0)
            ERROR_EXIT("New temp file assignment failed!");

        LOG("Temporal file: %s is opened for plugin: %s \n", new_file_name, plgn->filename);
    }

    printf("receiver_ring_cnt: %d \n",receiver_ring_cnt);
    for(int i=0; i < receiver_ring_cnt; i++){
        char ring_name[32] = {0};
        sprintf(ring_name, "ring_rec_%d", thread->core_id);
        printf("ring_name: %s receiver_rings[i]->name: %s \n",ring_name,receiver_rings[i]->name);
        if(!strcmp(receiver_rings[i]->name,ring_name)){
            ring = receiver_rings[i];
            printf("main_loop ring_name: %s \n", ring->name);
            break;
        }
    }

    if(ring == NULL){
        printf("ring is NULL \n");
        goto exit;
    }
        

    gettimeofday(&t1, NULL);
    gettimeofday(&tf1, NULL);
    while (1) {

        struct lbuf *pkt;
        // dequeue from ring
        if(ring_dequeue(ring,(void **)&pkt) == 0){
            if(pkt == NULL)
                continue;

            int p_index = 0;
            plgn = plugin_lookup_with_ip_dec(pkt->client_addr.sin_addr.s_addr, &p_index);
            if (plgn == NULL) {
                //LOG("Plugin %s is not found\n", inet_ntoa(client_addr.sin_addr));
                continue;
            }

            sec_pkt_cnt++;
            tot_pkt_cnt++;

#if TEMP_FILE_OPS == 1

            if(wb[p_index]->cnt >= MAX_LOG_BURST){
                uint64_t size = drain_write_buffer(wb[p_index], plgn->t_file->fptr, TRUE);
                __sync_add_and_fetch(&plgn->t_file->file_size, size);
            }

            size_t log_len = strlen(pkt->log);
            char *tmp_log = malloc(log_len+2);
            memcpy(tmp_log, &pkt->log, log_len+2);
            tmp_log[log_len] = '\n';

            wb[p_index]->buffer[wb[p_index]->cnt] = tmp_log;
            wb[p_index]->cnt++;


#endif

            int core_index = 0;
            int ring_index = 0;

            if (plgn->core_id_num != 0) {
                core_index = rand() % plgn->core_id_num;
                ring_index = plgn->core_ids[core_index];
            } else {
                core_index = rand() % nb_slaves;
                ring_index = plgn->core_ids[core_index];
            }
            
            int rslt = ring_enqueue(plgn->ring_rxs[ring_index][thread->thread_index], pkt);
            if (rslt < 0) {
                free(pkt);
                tot_free_pkt_cnt++;
            }
            
            if ((__sync_fetch_and_add(&plgn->t_file->file_size,0) > config.temp_file_size)) { // file size check
                current_time = time(NULL);
                tm = localtime(&current_time);
                char new_file_name[100];
                gettimeofday(&timestamp, NULL);
                snprintf(new_file_name, sizeof(new_file_name), "%s_%02d:%02d:%02d_%ld.temp", plgn->ip, tm->tm_hour, tm->tm_min, tm->tm_sec,timestamp.tv_usec%1000);
                set_temp_file_ptr(plgn, tm, new_file_name);
            }
            
        }
        
        gettimeofday(&t2, NULL);
        diff = (t2.tv_sec - t1.tv_sec) * 1000.0;
        diff += (t2.tv_usec - t1.tv_usec) / 1000.0;

        if (diff > ONE_SEC_TO_USEC) {
            current_time = time(NULL);
            tm = localtime(&current_time);

            if(debug_enabled){
                LOGF(dbg_fptr,"|%d-%02d-%02d %02d:%02d:%02d| -> ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
                    tm->tm_min, tm->tm_sec);
                LOGF(dbg_fptr,"main_loop   [tid: %d][plgn_id: -][%lu pkts/s][tot_pkt: %lu][tot_free: %lu][buffer: %u]\n",
                        thread->core_id, sec_pkt_cnt, tot_pkt_cnt, tot_free_pkt_cnt,ring_count(ring));
                 fflush(dbg_fptr);
            }
            else{
                LOG("|%d-%02d-%02d %02d:%02d:%02d| -> ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
                    tm->tm_min, tm->tm_sec);
                if(plgn != NULL && sec_pkt_cnt > 1)
                    LOG_GREEN("main_loop   [tid: %d][plgn_id: %d][%lu pkts/s][tot_pkt: %lu][tot_free: %lu][buffer: %u]\n",
                          thread->core_id, plgn->plugin_id, sec_pkt_cnt, tot_pkt_cnt, tot_free_pkt_cnt,ring_count(ring));
                else
                    LOG_RED("main_loop   [tid: %d][plgn_id: -][%lu pkts/s][tot_pkt: %lu][tot_free: %lu][buffer: %u]\n",
                          thread->core_id, sec_pkt_cnt, tot_pkt_cnt, tot_free_pkt_cnt,ring_count(ring));
            }
            
            sec_pkt_cnt = 0;
            gettimeofday(&t1, NULL);
        }
        
        gettimeofday(&tf2, NULL);
        diff_file = (tf2.tv_sec - tf1.tv_sec) * 1000.0;
        diff_file += (tf2.tv_usec - tf1.tv_usec) / 1000.0;

        if (diff_file > ONE_SEC_TO_USEC) {

            current_time = time(NULL);
            tm = localtime(&current_time);

            struct plugin_t *plgn;
            for (int i = 0; i < plugin_cnt; i++) {
                plgn = &plugin[i];
                    
                if(wb[i]->cnt > 0){
                    uint64_t size = drain_write_buffer(wb[i], plgn->t_file->fptr, FALSE);
                    fflush(plgn->t_file->fptr);
                    __sync_add_and_fetch(&plgn->t_file->file_size, size);
                }
                
                if (unlikely(tm->tm_min == 00 && tm->tm_sec == 00)) {
                    char new_file_name[100];
                    gettimeofday(&timestamp, NULL);
                    snprintf(new_file_name, sizeof(new_file_name), "%s_%02d:%02d:%02d_%ld.temp", plgn->ip, tm->tm_hour, tm->tm_min, tm->tm_sec,timestamp.tv_usec%1000);
                    set_temp_file_ptr(plgn, tm, new_file_name);
                }

                if (unlikely(kill_signal == true)) {
                    if (ring_count(ring) == 0)
                        goto exit;
                }
            }
            gettimeofday(&tf1, NULL);
        }
    }

exit:
    // all freeing operations in here 
    
    for (int t = 0; t < plugin_cnt; t++) {
        free(*(wb + t));
    }
    free(wb);

    LOG_GREEN("thread main with id %d is killed!\n", thread->core_id);
    pthread_cancel(pthread_self());
}

static int read_regexes_from_file(const char *filename, const char **expressions, int max) {
    FILE *fp;
    char *regex_line = NULL;
    size_t len = 0;
    ssize_t read;
    int line_count = 0;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("cannot open file regex_file: %s \n", filename);
        return -1;
    }
    while ((read = getline(&regex_line, &len, fp)) != -1) {
        char *buffer = (char *) malloc(read * sizeof(char));
        strncpy(buffer, regex_line, read);
        expressions[line_count] = buffer;
        line_count++;
        if (line_count == max)
            break;
    }

    fclose(fp);

    return line_count;
}

static int read_regexes_from_plugin(struct plugin_t *p, const char **expressions) {
    for (int i = 0; i < p->regex_cnt; i++) {
        expressions[i] = p->regex[i].regexp;
    }

    return p->regex_cnt;
}

static void initialize_regex_ids(int *regex_ids, int size) {
    for (int i = 0; i < size; i++) {
        regex_ids[i] = i;
    }
}

static int read_logs_from_file(const char *filename, char **logs, int max) {
    FILE *fp;
    size_t len = 0;
    ssize_t read;
    int line_count = 0;
    char *log_line = NULL;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("cannot open log file: %s \n", filename);
        return -1;
    }
    while ((read = getline(&log_line, &len, fp)) != -1) {
        //printf("Retrieved line of length %zu:\n", read);
        //printf("log_byte_count: %u line_count: %d \n",log_byte_count,line_count);
        //printf("log_line: %s \n", log_line);
        char *buffer = (char *) malloc(read * sizeof(char));
        strncpy(buffer, log_line, read);
        logs[line_count] = buffer;
        line_count++;
        if (line_count == max)
            break;
    }

    fclose(fp);
    return line_count;
}

static void usage(const char *prgname) {
    printf("%s [EAL options] -- "
           "  -f FILENAME: config file to be parsed\n"
           "  -c COREMASK: core mask in decimal eg 1,2,3\n"
           "  -k KEY: key to run program\n",
           prgname);
}

static void get_system_uuid(uuid_t *in_uuid) {
    FILE *fp;
    char path[100];
    char *uuid_string;

    fp = popen("dmidecode | grep UUID", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        ERROR_EXIT("Error while getting system ınfo");
    }

    while (fgets(path, sizeof(path), fp) != NULL) {
        if (strstr(path, "UUID") != NULL) {
            uuid_string = strtok(path, ": ");
            uuid_string = strtok(NULL, ": ");
            uuid_string = strtok(uuid_string, "\n");
        }
    }

    uuid_parse(uuid_string, *in_uuid);
    pclose(fp);

}

static int compare_uuid(char *key, uuid_t uuid) {

    unsigned char uid[] = "abababab-cdcd-ffff-1234-567890abcdf0";
    unsigned char xored_uuid[36];
    unsigned char hex_to_char[3];
    unsigned char key_to_char[3];
    char uuid_to_string[36];
    uint8_t temp;

    uuid_unparse_lower(uuid, uuid_to_string);

    for (int i = 0; i < 36; ++i) {
        xored_uuid[i] = uuid_to_string[i] ^ uid[i];
        temp=xored_uuid[i];

    }

    for (int i = 0; i < 36; ++i) {
        snprintf(hex_to_char, 3, "%02X", xored_uuid[i]);
        memcpy(key_to_char, key+(2*i), 2);
        key_to_char[2] = '\0';
        int ret = strcmp(key_to_char, hex_to_char);
        if (ret != 0)
            ERROR_EXIT_RED("Key is wrong!");
    }
    key_verified = true;
    return SUCCESS;
}

static int parse_uuid(char *uuid) {
    int ret = uuid_parse(uuid, uuid_g);
    if ( ret == 0)
        return SUCCESS;
    else {
        ERROR_EXIT("UUID parse failed: %s \n", uuid);
        return ERR;
    }
}

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

static int parse_core_mask(char *core_mask, enum core_role core_role) {

    char *comma_delim = ",";
    if (!strcmp(core_mask, "all")) {
        memset(core_mask, 0, sizeof(core_mask));
        int core_num = get_nprocs();
        for (int i = 0; i < core_num; i++) {
            if (i != core_num - 1)
                sprintf(core_mask, "%s%d,", core_mask, i);
            else
                sprintf(core_mask, "%s%d", core_mask, i);
        }
    }

    char *token = strtok(core_mask, comma_delim);
    // Extract the first token
    printf("=======================================\n");

    while (token != NULL) {
        if (core_role == RECEIVER) {
            core_conf[nb_cores].core_id = atoi(token);
            nb_receiver_cores++;
            core_conf[nb_cores].role = RECEIVER;
            printf("%d. thread on -> core %d -> ROLE: RECEIVER\n", nb_cores, core_conf[nb_cores].core_id);
        }else if (core_role == MASTER) {
            core_conf[nb_cores].core_id = atoi(token);
            nb_masters++;
            core_conf[nb_cores].role = MASTER;
            printf("%d. thread on -> core %d -> ROLE: MASTER\n", nb_cores, core_conf[nb_cores].core_id);
        } else {
            core_conf[nb_cores].core_id = atoi(token);
            nb_slaves++;
            core_conf[nb_cores].role = SLAVE;
            printf("%d. thread on -> core %d -> ROLE: SLAVE\n", nb_cores, core_conf[nb_cores].core_id);
        }
        nb_cores++;
        token = strtok(NULL, comma_delim);
    }

    if (nb_cores > 0)
        return 1;
    else
        return 0;
}

static int core_count() {
    return nb_cores;
}

static int receiver_core_count() {
    return nb_receiver_cores;
}

static int master_core_count() {
    return nb_masters;
}

static int slave_core_count() {
    return nb_slaves;
}

static void initialize_json_file_handles(struct plugin_t *p){
    p->j_file = calloc(1,sizeof(struct json_file_handle_t));
    if (pthread_mutex_init(&p->j_file->file_lock, NULL) != 0) {
        ERROR_EXIT("\n mutex init has failed\n");
    }
}

static void delete_json_file_handles(struct plugin_t *p){
    if (p->j_file->fptr != NULL)
        fclose(p->j_file->fptr);
    if (p->j_file->prev_fptr != NULL)
        fclose(p->j_file->prev_fptr);
    free(p->j_file);
    pthread_mutex_destroy(&p->j_file->file_lock);
}

static void initialize_temp_file_handles(struct plugin_t *p){
    p->t_file = calloc(1,sizeof(struct temp_file_handle_t));
    if (pthread_mutex_init(&p->t_file->file_lock, NULL) != 0) {
        ERROR_EXIT("\n mutex init has failed\n");
    }
}

static void delete_temp_file_handles(struct plugin_t *p){
    if (p->t_file->fptr != NULL)
        fclose(p->t_file->fptr);
    if (p->t_file->prev_fptr != NULL)
        fclose(p->t_file->prev_fptr);
    free(p->t_file);
    pthread_mutex_destroy(&p->t_file->file_lock);
}

static void create_rings_receiver_main(struct thread **rec_threads, struct thread **man_threads){
    int ring_num = 0;
    char ring_name[32];
    for (int b = 0; b < master_core_count(); b++) {
        sprintf(ring_name, "ring_rec_%d", man_threads[b]->core_id);

        struct ring *ring_rx;
        if (receiver_core_count() < 2 )
            ring_rx = ring_create(ring_name, RING_SIZE, RING_F_SP_ENQ);
        else
            ring_rx = ring_create(ring_name, RING_SIZE, 0);

        receiver_rings[ring_num] = ring_rx;
        ring_num++;
        printf("Receiver ring %s is created and attach to receiver_rings[%d]\n", ring_name, ring_num);
    }
    receiver_ring_cnt = ring_num;
}

static void create_rings_dedicated_threads(struct plugin_t *p, struct thread **sthreads) {
    char ring_name[32];
    for (int a=0; a<master_core_count(); a++) {
        if (p->all_nondedicated_cores < 1) {
            for (int i = 0; i < p->core_id_num; i++) {
                sprintf(ring_name, "ring_rx_%d_%s_%d", a, p->filename, p->core_ids[i]);

                struct ring *ring_rx;
                if (master_core_count() < 2)
                    ring_rx = ring_create(ring_name, RING_SIZE, RING_F_SP_ENQ);
                else
                    ring_rx = ring_create(ring_name, RING_SIZE, 0);

                p->ring_rxs[p->core_ids[i]][a] = ring_rx;
                printf("Ring %s is created and attach to plugin %s in index [%d][%d] \n", ring_name, p->filename,
                       p->core_ids[i], a);
                sprintf(ring_name, "ring_tx_%d_%s_%d", a, p->filename, p->core_ids[i]);
                struct ring *ring_tx = ring_create(ring_name, RING_SIZE, RING_F_SP_ENQ);
                p->ring_txs[p->core_ids[i]][a] = ring_tx;
                printf("Ring %s is created and attach to plugin %s in index [%d][%d] \n", ring_name, p->filename,
                       p->core_ids[i], a);
            }
        }
    }
}

static void create_rings_nondedicated_threads(struct plugin_t *p, struct thread **sthreads) {
    char ring_name[32];
    for(int a=0; a<master_core_count(); a++) {
        for (int i = 0; i < slave_core_count(); i++) {
            if (!sthreads[i]->dedicated) {
                if (p->ring_rxs[sthreads[i]->core_id] == NULL) {
                    sprintf(ring_name, "ring_rx_%d_%s_%d", a, p->filename, sthreads[i]->core_id);
                    struct ring *ring_rx = ring_create(ring_name, RING_SIZE, (RING_F_SP_ENQ));
                    p->ring_rxs[sthreads[i]->core_id][a] = ring_rx;
                    printf("Ring %s is created and attach to plugin %s in index [%d][%d] \n", ring_name, p->filename,
                           sthreads[i]->core_id, a);
                }
                if (p->ring_txs[sthreads[i]->core_id] == NULL) {
                    sprintf(ring_name, "ring_tx_%d_%s_%d", a, p->filename, sthreads[i]->core_id);
                    struct ring *ring_tx = ring_create(ring_name, RING_SIZE, (RING_F_SP_ENQ));
                    p->ring_txs[sthreads[i]->core_id][a] = ring_tx;
                    printf("Ring %s is created and attach to plugin %s in index [%d][%d] \n", ring_name, p->filename,
                           sthreads[i]->core_id, a);
                }
            }

        }
    }

}

static void copy_regex(const struct regex_t *from, struct regex_t *to) {
    to->id = from->id;
    to->description = strdup(from->description);
    to->precheck = strdup(from->precheck);
    to->regexp = strdup(from->regexp);
    for (int i = 0; i < from->field_cnt; i++)
        to->fields[i] = from->fields[i];
    to->field_cnt = from->field_cnt;
    // pcre related fields are not copied.
    // they are assigned when compile_and_study_regexes func is called.
}

static void copy_plugin(const struct plugin_t *from, struct plugin_t *to) {
    to->filename = strdup(from->filename);
    to->plugin_id = from->plugin_id;
    for (int i = 0; i < from->core_id_num; i++)
        to->core_ids[i] = from->core_ids[i];
    to->core_id_num = from->core_id_num;

    if (from->all_nondedicated_cores != 0)
        to->all_nondedicated_cores = from->all_nondedicated_cores;
    else
        to->all_nondedicated_cores = 0;
        
    to->optimized = from->optimized;
    to->ip = strdup(from->ip);
    to->regex_cnt = from->regex_cnt;
    for (int i = 0; i < from->regex_cnt; i++)
        copy_regex(&from->regex[i], &to->regex[i]);
    for(int a = 0; a < MAX_CORE_NUM; a++) {
        for (int i = 0; i < MAX_CORE_NUM; i++) { //shallow copy
            to->ring_rxs[i][a] = from->ring_rxs[i][a];
            to->ring_txs[i][a] = from->ring_txs[i][a];
        }
    }
    to->t_file = from->t_file; //shallow copy
    to->j_file = from->j_file; //shallow copy
}

static void assign_plugins_to_workers(struct thread **r, struct thread **m, struct thread **t) {
    for (int s_cnt = 0; s_cnt < slave_core_count(); s_cnt++) {
        int plgn_indx = 0;
        for (int i = 0; i < plugin_cnt; i++) {
            if (plugin[i].all_nondedicated_cores == 0) {
                for (int j = 0; j < plugin[i].core_id_num; j++) {
                    if (t[s_cnt]->core_id == plugin[i].core_ids[j]) {
                        t[s_cnt]->dedicated = TRUE;
                        t[s_cnt]->plugins[plgn_indx] = calloc(1, sizeof(struct plugin_t));

                        copy_plugin(&plugin[i], t[s_cnt]->plugins[plgn_indx]);
                        if (compile_and_study_regexes(t[s_cnt]->plugins[plgn_indx]) < 0)
                            ERROR_EXIT("cannot compile and study plugin: %s ", t[s_cnt]->plugins[plgn_indx]->filename);
                        t[s_cnt]->pcnt++;
                        plgn_indx++;
                        LOG("plugin[%d] %s, plugin_core_id %d, th[%d]coreid %d dedicated %d\n", plugin[i].plugin_id,
                            plugin[i].filename, plugin[i].core_ids[j], s_cnt, t[s_cnt]->core_id, t[s_cnt]->dedicated);
                    }
                }
            }
        }
    }
    for (int s_cnt = 0; s_cnt < slave_core_count(); s_cnt++) {
        int plgn_indx = 0;
        for (int i = 0; i < plugin_cnt; i++) {
            if (plugin[i].all_nondedicated_cores != 0) {
                if (!t[s_cnt]->dedicated) {

                    t[s_cnt]->plugins[plgn_indx] = calloc(1, sizeof(struct plugin_t));

                    copy_plugin(&plugin[i], t[s_cnt]->plugins[plgn_indx]);
                    if (compile_and_study_regexes(t[s_cnt]->plugins[plgn_indx]) < 0)
                        ERROR_EXIT("cannot compile and study plugin: %s ", t[s_cnt]->plugins[plgn_indx]->filename);
                    plgn_indx++;
                    t[s_cnt]->pcnt++;
                    LOG("plugin[%d] %s, plugin_core_id %d, th[%d]coreid %d dedicated %d\n", plugin[i].plugin_id,
                        plugin[i].filename, t[s_cnt]->core_id, s_cnt, t[s_cnt]->core_id, t[s_cnt]->dedicated);
                }
            }
        }
    }

    printf("===========================\n");
    LOG_BLUE("receiver core map: \n");
    for (int i = 0; i < receiver_core_count(); i++) {
        printf("thread %d core_id: %d -> receiver_thread_id %d", i, r[i]->core_id, r[i]->thread_index);
        printf("\n");
    }
    LOG_BLUE("main core map: \n");
    for (int i = 0; i < master_core_count(); i++) {
        printf("thread %d core_id: %d -> master_thread_id %d", i, m[i]->core_id, m[i]->thread_index);
        printf("\n");
    }
    LOG_BLUE("plugin core map: \n");
    for (int i = 0; i < slave_core_count(); i++) {
        printf("thread %d core_id: %d : ", i, t[i]->core_id);
        if (t[i]->dedicated) {
            for (int k = 0; k < t[i]->pcnt; k++) {
                LOG_YELLOW("%s -> dedicated: %d ", t[i]->plugins[k]->filename, t[i]->dedicated);
            }
            printf("\n");
        } else {
            for (int k = 0; k < t[i]->pcnt; k++) {
                LOG_BLUE("%s %d ", t[i]->plugins[k]->filename, t[i]->dedicated);
            }
            printf("\n");
        }
    }
    printf("===========================\n");
}

int main(int argc, char *argv[]) {
    int opt;
    char *config_file;
    char *debug_file;
    char *prgname = argv[0];

    struct timeval t1, t2;
    double elapsedTime;
    uuid_t system_uuid;

    signal(SIGINT, signal_handler);

    while ((opt = getopt(argc, argv, "f:c:m:s:k:d:l:")) != -1) {
        switch (opt) {
            case 'f':
                config_file = optarg;
                break;
            case 'd':
                debug_file = optarg;
                debug_enabled = TRUE;
                break;
            case 'l':
                debug_level = parse_decimal(optarg);
                break;
            case 'm':
                master_core_mask = parse_core_mask(optarg, MASTER);
                if (master_core_mask == 0) {
                    printf("invalid master cores\n");
                    usage(prgname);
                    return -1;
                }
                break;
            case 's':
                master_receiver_core_mask = parse_core_mask(optarg, RECEIVER);
                if (master_receiver_core_mask == 0) {
                    printf("invalid master receiver cores\n");
                    usage(prgname);
                    return -1;
                }
                break;
            case 'c':
                core_mask = parse_core_mask(optarg, SLAVE);
                if (core_mask == 0) {
                    printf("invalid cores\n");
                    usage(prgname);
                    return -1;
                }
                printf("=======================================\n");
                break;
            case 'k':
                //parse_uuid(optarg);
                get_system_uuid(&system_uuid);
                if (compare_uuid(optarg, system_uuid) == ERR) {
                    printf("invalid key");
                    usage(prgname);
                    return -1;
                }
                break;
            default:
                usage(prgname);
                exit(1);
                break;
        }
    }

    if(debug_enabled){
        if(create_debug_file(debug_file) != SUCCESS){
            ERROR("Debug file cannot be opened! disabling file debug. ");
            debug_enabled = FALSE;
        }    
    }

    if (receiver_core_count()<1)
        ERROR_EXIT_RED("invalid receiver core num, check -s parameter");
    if (master_core_count()<1)
        ERROR_EXIT_RED("invalid master core num, check -m parameter");
    if (slave_core_count()<1)
        ERROR_EXIT_RED("invalid slave core num, check -c parameter");

    if (key_verified == false) {
        ERROR_EXIT_RED("You must enter the key to run the program! Exiting...\n");
    }

    if (parse_configuration_file(config_file) != SUCCESS) {
        ERROR_EXIT("cannot parse config file!! exiting... \n");
    }

    for (int i = 0; i < config.plugin_file_cnt; i++) {
        if (parse_plugin_file(config.plugin_file[i]) < 0)
            ERROR_EXIT("cannot parse plugin file %s ", config.plugin_file[i]);
    }

    for (int i = 0; i < plugin_cnt; i++) {
        struct plugin_t *p = &plugin[i];
        create_rings_dedicated_threads(p, NULL);
        initialize_json_file_handles(p);
        initialize_temp_file_handles(p);
    }

    if(initialize_custom_functions() < 0)
        ERROR_EXIT("Cannot initialize custom functions!");
    
    if (core_mask == MAX_CORE_NUM)
        LOG("All cores will be work!");
    else
        LOG("core mask is : %d\n", core_count());

    LOG("receiver_core_count %d, master_core_count: %d slave_core_count: %d plugin_cnt: %d \n",
        receiver_core_count(), master_core_count(), slave_core_count(), plugin_cnt);

    unsigned long *tids = calloc(core_count(), sizeof(unsigned long));

    cpu_set_t cpuset_master;
    cpu_set_t cpuset_receiver;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset_master);
    CPU_ZERO(&cpuset_receiver);
    CPU_ZERO(&cpuset);
    int set_result;

    rthreads = calloc(receiver_core_count(), sizeof(struct thread));
    for (int t = 0; t < receiver_core_count(); t++) {
        *(rthreads + t) = calloc(1, sizeof(struct thread));
        rthreads[t]->core_id = core_conf[t].core_id;
        rthreads[t]->thread_index = t;
    }

    mthreads = calloc(master_core_count(), sizeof(struct thread));
    for (int t = 0; t < master_core_count(); t++) {
        *(mthreads + t) = calloc(1, sizeof(struct thread));
        mthreads[t]->core_id = core_conf[t + receiver_core_count()].core_id;
        mthreads[t]->thread_index = t;
    }

    create_rings_receiver_main(rthreads, mthreads);

    for (int t = 0; t < receiver_core_count(); t++) {
        CPU_ZERO(&cpuset_receiver);
        CPU_SET(core_conf[t].core_id, &cpuset_receiver);

        if (pthread_create(&(rthreads[t]->thread_id), NULL, receiver_loop, rthreads[t]) != 0) {
            ERROR_EXIT("pthread_create error");
        }

        if (pthread_setaffinity_np(rthreads[t]->thread_id, sizeof(cpu_set_t), &cpuset_receiver) != 0) {
            ERROR_EXIT("pthread_setaffinity_np error");
        }

        tids[t] = rthreads[t]->thread_id;
    }
    
    for (int t = 0; t < master_core_count(); t++) {
        CPU_ZERO(&cpuset_master);
        CPU_SET(core_conf[t + receiver_core_count()].core_id, &cpuset_master);

        if (pthread_create(&(mthreads[t]->thread_id), NULL, main_loop, mthreads[t]) != 0) {
            ERROR_EXIT("pthread_create error");
        }

        if (pthread_setaffinity_np(mthreads[t]->thread_id, sizeof(cpu_set_t), &cpuset_master) != 0) {
            ERROR_EXIT("pthread_setaffinity_np error");
        }

        tids[t + receiver_core_count()] = mthreads[t]->thread_id;
    }

    sthreads = calloc(slave_core_count(), sizeof(struct thread));
    for (int t = 0; t < slave_core_count(); t++) {
        *(sthreads + t) = calloc(1, sizeof(struct thread));
        sthreads[t]->core_id = core_conf[t + receiver_core_count() + master_core_count()].core_id;
        sthreads[t]->jit_stack = pcre_jit_stack_alloc(32 * 1024, 1024 * 1024);
    }
    assign_plugins_to_workers(rthreads, mthreads, sthreads);
    for (int i = 0; i < slave_core_count(); i++) {
        for (int j = 0; j < sthreads[i]->pcnt; ++j) {
            struct plugin_t *p = sthreads[i]->plugins[j];
            if (!sthreads[i]->dedicated)
                create_rings_nondedicated_threads(p, sthreads);
        }

    }

    for (int t = 0; t < slave_core_count(); t++) {
        CPU_ZERO(&cpuset);
        CPU_SET(core_conf[t + receiver_core_count() + master_core_count()].core_id, &cpuset);

        if (pthread_create(&(sthreads[t]->thread_id), NULL, worker_loop, sthreads[t]) != 0) {
            ERROR_EXIT("pthread_create error");
        }

        if (pthread_setaffinity_np(sthreads[t]->thread_id, sizeof(cpu_set_t), &cpuset) != 0) {
            ERROR_EXIT("pthread_setaffinity_np error");
        }

        tids[t + receiver_core_count() + master_core_count()] = sthreads[t]->thread_id;
    }

    

    for (int t = 0; t < receiver_core_count(); t++) {
        int core_id = core_conf[t].core_id;

        if(pthread_getaffinity_np(rthreads[t]->thread_id, sizeof(cpu_set_t), &cpuset_receiver) != 0)
            ERROR_EXIT("pthread_getaffinity_np error");

        if (CPU_ISSET(core_id, &cpuset_receiver)) {
            LOG("Receiver thread successfully set thread %lu with affinity to CPU %d\n", tids[t], core_id);
        } else {
            LOG("Failed to set thread %lu with affinity to CPU %d\n", tids[t], core_id);
        }
    }

    for (int t = 0; t < master_core_count(); t++) {
        int core_id = core_conf[t + receiver_core_count()].core_id;

        if(pthread_getaffinity_np(mthreads[t]->thread_id, sizeof(cpu_set_t), &cpuset_master) != 0)
                ERROR_EXIT("pthread_getaffinity_np error");
                
        if (CPU_ISSET(core_id, &cpuset_master)) {
            LOG("Main thread successfully set thread %lu with affinity to CPU %d\n", tids[t + receiver_core_count()], core_id);
        } else {
            LOG("Failed to set thread %lu with affinity to CPU %d\n", tids[t], core_id);
        }
    }

    for (int t = 0; t < slave_core_count(); t++) {
        int core_id = core_conf[t + receiver_core_count() + master_core_count()].core_id;
        if(pthread_getaffinity_np(sthreads[t]->thread_id, sizeof(cpu_set_t), &cpuset) != 0)
                ERROR_EXIT("pthread_getaffinity_np error");
                
        if (CPU_ISSET(core_id, &cpuset)) {
            LOG("Slave thread successfully set thread %lu with affinity to CPU %d\n", tids[t + receiver_core_count() +
            master_core_count()], core_id);
        } else {
            LOG("Failed to set thread %lu with affinity to CPU %d\n", tids[t + receiver_core_count() +
            master_core_count()], core_id);
        }
    }

    for (int i = 0; i < core_count(); i++) {
        pthread_join(tids[i], NULL);
    }

    for (int t = 0; t < slave_core_count(); t++) {
        for (int i = 0; i < MAX_PLUGIN_CNT; i++)
            free(sthreads[t]->plugins[i]);

        free(sthreads[t]);
    }
    free(sthreads);

    for (int t = 0; t < master_core_count(); t++) {
        free(mthreads[t]);
    }

    for (int t = 0; t < receiver_core_count(); t++) {
        free(rthreads[t]);
    }

    free(mthreads);
    free(rthreads);
    free(tids);

    for (int i = 0; i < plugin_cnt; i++) {
        struct plugin_t *p = &plugin[i];
        delete_json_file_handles(p);
        delete_temp_file_handles(p);
    }

    for(int master_index=0; master_index<master_core_count();master_index++) {
        for (int a = 0; a < plugin_cnt; a++) {
            struct plugin_t *p = &plugin[a];
            for (int i = 0; i < p->core_id_num; i++) {
                ring_free(p->ring_rxs[p->core_ids[i]][master_index]);
                ring_free(p->ring_txs[p->core_ids[i]][master_index]);
            }
        }
    }

    destroy_hash_map(hash_map);

    return 0;
}
