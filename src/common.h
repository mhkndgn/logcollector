#ifndef COMMON_H
#define COMMON_H

#include <netinet/in.h>
#include <pcre.h>
#include <stdbool.h>
#include "defines.h"

char *logs[MAX_LOG_CNT];
const char *regex_lines[MAX_REGEX_CNT];
int regex_ids[MAX_REGEX_CNT];

/* net.c */
struct net_addr
{
	int ipver;
	struct sockaddr_in sin4;
	struct sockaddr_in6 sin6;
	struct sockaddr *sockaddr;
	int sockaddr_len;
};

typedef enum core_role{
    RECEIVER,
    MASTER,
    SLAVE
}core_role_t;

typedef struct core {
    core_role_t role;
    int core_id;
} core_t;

typedef enum file_type{
    JSON,
    TEMP
}file_type_t;

struct configuration {
    char *listen_addr_str[MAX_UDP_SOCKET_CNT] ;
    int listen_addr_cnt;
    char *plugin_file[MAX_PLUGIN_CNT];
    int plugin_file_cnt;
    char *log_file;
    char *tmp_folder;
    uint64_t temp_file_size;
    char *json_folder;
    uint64_t json_file_size;
    int thread_num;
    int reuse_port;
    int recv_buf_size;
};

struct field_t{
    char *key;
    char *findby;
    char *func_name;
    bool is_value;
    bool is_integer;
};

struct regex_t{
    int id;
    char *description;
    char *precheck;
    char *regexp;
    struct field_t fields[MAX_REGEX_FIELDS];
    int field_cnt;
    pcre *compiled;
    pcre_extra *extra;
};

struct file_t{
    FILE * fp;
    int ref;
} typedef file_t;

struct json_file_handle_t{
    pthread_mutex_t file_lock;
    FILE *fptr;
    FILE *prev_fptr;
    volatile uint64_t file_size;
    int ref;
    int prev_ref;
};

struct temp_file_handle_t{
    pthread_mutex_t file_lock;
    FILE *fptr;
    FILE *prev_fptr;
    volatile uint64_t file_size;
    int ref;
    int prev_ref;
};

struct write_buffer{
    char *buffer[MAX_LOG_BURST];
    int cnt;
};

struct plugin_t{
    char *filename;
    int plugin_id;
    int core_ids[MAX_PLUGIN_CORE_CNT];
    int core_id_num;
    int all_nondedicated_cores;
    bool optimized;
    const char *ip;
    uint32_t ip_dec;
    const char *port;
    int regex_cnt;
    int empty_rcnt;
    int filled_rcnt;
    int generic_rcnt;
    struct regex_t regex[MAX_REGEX_CNT];
    struct regex_t *empty_p_regex[MAX_REGEX_CNT];
    struct regex_t *filled_p_regex[MAX_REGEX_CNT];
    struct regex_t *generic_p_regex[MAX_REGEX_CNT];
    struct ring *ring_rxs[MAX_CORE_NUM][MAX_CORE_NUM];
    struct ring *ring_txs[MAX_CORE_NUM][MAX_CORE_NUM];
    struct temp_file_handle_t *t_file;
    struct json_file_handle_t *j_file; //json file which depends on time
};

struct thread {
	pthread_t thread_id;
	int thread_index;
	pcre_jit_stack *jit_stack;
	struct plugin_t *plugins[MAX_PLUGIN_CNT];
	int pcnt; // assigned plugin count
	int core_id;
	int dedicated;
};

struct lbuf {
    char log[MAX_MSG];
    struct sockaddr_in client_addr;
};

struct ring *receiver_rings[MAX_CORE_NUM];
int receiver_ring_cnt;

struct configuration config;
struct plugin_t plugin[MAX_PLUGIN_CNT];
int plugin_cnt;

struct shash *hash_map;

void parse_addr(struct net_addr *netaddr, const char *addr);
const char *addr_to_str(struct net_addr *addr);
int net_bind_udp(struct net_addr *addr, int reuseport);
void net_set_buffer_size(int cd, int max, int send);
void net_gethostbyname(struct net_addr *shost, const char *host, int port);
int net_connect_udp(struct net_addr *addr, int src_port);

struct thread;
struct thread *thread_spawn(void (*callback)(void *), void *userdata);

#endif
