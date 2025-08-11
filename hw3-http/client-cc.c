#include "http.h"

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define  BUFF_SZ 1024

char recv_buff[BUFF_SZ];

char *generate_cc_request(const char *host, int port, const char *path){
	static char req[512] = {0};
	int offset = 0;
	
    //note that all paths should start with "/" when passed in
	offset += sprintf((char *)(req + offset),"GET %s HTTP/1.1\r\n", path);
	offset += sprintf((char *)(req + offset),"Host: %s\r\n", host);
	offset += sprintf((char *)(req + offset),"Connection: Close\r\n");
	offset += sprintf((char *)(req + offset),"\r\n");

	printf("DEBUG: %s", req);
	return req;
}


void print_usage(char *exe_name){
    fprintf(stderr, "Usage: %s <hostname> <port> <path...>\n", exe_name);
    fprintf(stderr, "Using default host %s, port %d  and path [\\]\n", DEFAULT_HOST, DEFAULT_PORT); 
}

int process_request(const char *host, uint16_t port, char *resource){
    int sock;
    int total_bytes;
    char snd_buf[BUFF_SZ];

    memset(snd_buf, 0, BUFF_SZ);

    sock = socket_connect(host, port);
    if(sock < 0) return sock;

    strncpy(snd_buf, generate_cc_request(host, port, resource), 512);
    if(send(sock, snd_buf, BUFF_SZ, 0) == -1) {
        perror("Send HTTP request failed.\n");
        exit(1);
    }
    printf("Send request success!\n");

    int bytesRecvd = 0;
    do{
        bytesRecvd = recv(sock, recv_buff, BUFF_SZ, 0);
        total_bytes += bytesRecvd;
        printf("%.*s", bytesRecvd, recv_buff);
    } while(bytesRecvd == BUFF_SZ);

    close(sock);
    return total_bytes;
}

int main(int argc, char *argv[]){
    int sock;
    struct timespec start, end, elapsed;
    const char *host = DEFAULT_HOST;
    uint16_t   port = DEFAULT_PORT;
    char       *resource = DEFAULT_PATH;
    int        remaining_args = 0;

    // Command line argument processing should be all setup, you should not need
    // to modify this code

    if(clock_gettime(CLOCK_REALTIME, &start) == -1) {
        perror("Get start time error\n");
    }
    if(argc < 4){
        print_usage(argv[0]);
        //process the default request
        process_request(host, port, resource);
	} else {
        host = argv[1];
        port = atoi(argv[2]);
        resource = argv[3];
        if (port == 0) {
            fprintf(stderr, "NOTE: <port> must be an integer, using default port %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
        fprintf(stdout, "Running with host = %s, port = %d\n", host, port);
        remaining_args = argc-3;
        for(int i = 0; i < remaining_args; i++){
            resource = argv[3+i];
            fprintf(stdout, "\n\nProcessing request for %s\n\n", resource);
            process_request(host, port, resource);
        }
    }
    if(clock_gettime(CLOCK_REALTIME, &end) == -1) {
        perror("Get end time error\n");
    }
    elapsed.tv_sec = end.tv_sec - start.tv_sec;

    if(end.tv_nsec < start.tv_nsec) {
        elapsed.tv_sec++;
        elapsed.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
    }
    printf("Task finished in %d seconds and %lld nanoseconds\n", elapsed.tv_sec, elapsed.tv_nsec);
}