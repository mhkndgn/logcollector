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
#include <string.h>

#include <sys/types.h>
#include <sys/sysctl.h>


#define MAX_LOG_CNT 20
#define MAX_LOG_LEN 1024

#define US_PER_S 1000000
#define BURST_TX_DRAIN_US 10000 /* drain every ~1 sec */

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define CYC_PER_10MHZ 1E7

/* net.c */
struct net_addr
{
	int ipver;
	struct sockaddr_in sin4;
	struct sockaddr_in6 sin6;
	struct sockaddr *sockaddr;
	int sockaddr_len;
};

static inline uint64_t get_cycles()
{
    uint64_t t;
    __asm volatile ("rdtsc" : "=A"(t));
    return t;
}

static inline uint64_t
rte_rdtsc(void)
{
	union {
		uint64_t tsc_64;
		struct {
			uint32_t lo_32;
			uint32_t hi_32;
		};
	} tsc;

	asm volatile("rdtsc" :
		     "=a" (tsc.lo_32),
		     "=d" (tsc.hi_32));
	return tsc.tsc_64;
}	

const char *str_quote(const char *s)
{
	static char buf[1024];
	int r = snprintf(buf, sizeof(buf), "\"%.*s\"", (int)sizeof(buf) - 4, s);
	if (r >= (int)sizeof(buf)) {
		buf[sizeof(buf) - 1] = 0;
	}
	return buf;
}

static void net_set_buffer_size(int cd, int max, int send)
{
	int i, flag;

	if (send) {
		flag = SO_SNDBUF;
	} else {
		flag = SO_RCVBUF;
	}

	for (i = 0; i < 10; i++) {
		int bef;
		socklen_t size = sizeof(bef);
		if (getsockopt(cd, SOL_SOCKET, flag, &bef, &size) < 0) {
			fprintf(stderr,"getsockopt(SOL_SOCKET)");
            exit(-1);
			break;
		}
		if (bef >= max) {
			break;
		}

		size = bef * 2;
		if (setsockopt(cd, SOL_SOCKET, flag, &size, sizeof(size)) < 0) {
			// don't log error, just break
			break;
		}
	}
}

static int net_bind_udp(struct net_addr *shost, int reuseport)
{
	int sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {
		fprintf(stderr,"socket()");
        exit(-1);
	}

	int one = 1;
	int r = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
			   sizeof(one));
	if (r < 0) {
		fprintf(stderr,"setsockopt(SO_REUSEADDR)");
        exit(-1);
	}

	if (reuseport) {
		one = 1;
		r = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (char*)&one, sizeof(one));
		if (r < 0) {
			fprintf(stderr,"setsockopt(SO_REUSEPORT)");
            exit(-1);
		}
	}

	if (bind(sd, shost->sockaddr, shost->sockaddr_len) < 0) {
		fprintf(stderr,"bind()");
        exit(-1);
	}

	return sd;
}

static void net_gethostbyname(struct net_addr *shost, const char *host, int port)
{
	memset(shost, 0, sizeof(struct net_addr));

	struct in_addr in_addr;

	if (inet_pton(AF_INET, host, &in_addr) == 1) {
		goto got_ipv4;
	}

	fprintf(stderr,"inet_pton(%s)", str_quote(host));
	return;

got_ipv4:
	shost->ipver = 4;
	shost->sockaddr = (struct sockaddr*)&shost->sin4;
	shost->sockaddr_len = sizeof(shost->sin4);
	shost->sin4.sin_family = AF_INET;
	shost->sin4.sin_port = htons(port);
	shost->sin4.sin_addr = in_addr;
	return;
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
        char *pos;
        if ((pos=strchr(buffer, '\n')) != NULL)
            *pos = '\0';

        logs[line_count] = buffer;
        line_count++;
        if (line_count == max)
            break;
    }

    fclose(fp);
    return line_count;
}

static void parse_addr(struct net_addr *netaddr, const char *addr) {
	char *colon = strrchr(addr, ':');
	if (colon == NULL) {
		fprintf(stderr,"You forgot to specify port");
	}
	int port = atoi(colon+1);
	if (port < 0 || port > 65535) {
		fprintf(stderr,"Invalid port number %d", port);
	}
	char host[255];
	int addr_len = colon-addr > 254 ? 254 : colon-addr;
	strncpy(host, addr, addr_len);
	host[addr_len] = '\0';
	net_gethostbyname(netaddr, host, port);
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

int main(int argc, char *argv[]) {
    struct net_addr send_addr, src_address;
    struct timeval t1, t2, d1, d2;
    struct tm *tm;
    time_t current_time;    
    int main_fd = -1;
    double diff, diff_d;
    double sec_tot_pkt_len = 0.0;
    uint64_t sec_pkt_cnt = 0;
    uint64_t tot_pkt_cnt = 0;
    uint64_t tot_free_pkt_cnt = 0;
    char *prgname = argv[0];
    char *log_file = NULL;
    int opt;
    int count = 0;
    char *address = NULL;
    int dst_port;
    int sample = 1;
    int recv_buf_size = 4096;

    char *logs[MAX_LOG_CNT];
    int line_count = 0;
    uint64_t total_send = 0;
    uint64_t total_drop = 0;
    char src_addr[] = "0.0.0.0:5140";
    parse_addr(&src_address,src_addr);


    while ((opt = getopt(argc, argv, "f:c:a:s:")) != -1) {
        switch (opt) {
            case 'f':
                log_file = optarg;
                break;
            case 'c':
                count = parse_decimal(optarg);
                break;
            case 'a':
                address = optarg;
                parse_addr(&send_addr,address);
                break;
            case 's':
                sample = parse_decimal(optarg);
                break;
            default:
                //usage(prgname);
                exit(1);
                break;
        }
    }

    if(log_file != NULL) {
        line_count = read_logs_from_file(log_file,logs,MAX_LOG_CNT);
    }

    if (main_fd == -1) {
        main_fd = net_bind_udp(&src_address, 1);
        net_set_buffer_size(main_fd, recv_buf_size, 1);
    }

    const double drain_tsc = 1.0;
    double diff_tsc = 0.0;
    clock_t start, end;
    double total_second = 1.0;

    printf("drain_tsc: %f \n", drain_tsc);


    uint64_t loop =0;
    gettimeofday(&t1, NULL);
    //gettimeofday(&d1, NULL);
    start = clock();
    while(1) {
        
        if(loop % sample == 0) {
            int i = total_send % line_count;
            int len = sendto(main_fd, logs[i], strlen(logs[i]), 0, &send_addr.sin4, send_addr.sockaddr_len); 

            if (len <= 0) {
                total_drop++;
            } 
            else {
                total_send++;
            }

            if(total_drop + total_send == count)
                break;
            
        }
        
        end = clock();
        diff_tsc = ((double) (end - start)) / CLOCKS_PER_SEC;

        if (unlikely(diff_tsc > drain_tsc)) {
            total_second += 1.0;
            printf("%lu packet has been sent. total drop: %lu time_passed: %.2f EPS: %.0f \n", total_send, total_drop, total_second, total_send/total_second);
            start = clock();
            
        }
        
        /*
        gettimeofday(&d2, NULL);
        diff_d = (d2.tv_sec - d1.tv_sec) * 1000.0;
        diff_d += (d2.tv_usec - d1.tv_usec) / 1000.0;

        if(diff_d > 1000.0) {
            gettimeofday(&t2, NULL);
            diff = (t2.tv_sec - t1.tv_sec) * 1000.0;
            diff += (t2.tv_usec - t1.tv_usec) / 1000.0;
            diff = diff / 1000; //milisecond to second
            printf("%lu packet has been sent. total drop: %lu time_passed: %.2f EPS: %.0f \n", total_send, total_drop, diff, total_send/diff);
            gettimeofday(&d1, NULL);
        }
        */
        loop++;
    }
    gettimeofday(&t2, NULL);
    diff = (t2.tv_sec - t1.tv_sec) * 1000.0;
    diff += (t2.tv_usec - t1.tv_usec) / 1000.0;
    diff = diff / 1000; //milisecond to second

    printf("%lu packet has been sent. total drop: %lu time_passed: %.2f EPS: %.0f \n", total_send, total_drop, diff, total_send/diff);


    

}