#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include "du-ftp.h"
#include "du-proto.h"


#define BUFF_SZ 1024
static char sbuffer[BUFF_SZ];
static char rbuffer[BUFF_SZ];
static char full_file_path[FNAME_SZ];

/*
 *  Helper function that processes the command line arguements.  Highlights
 *  how to use a very useful utility called getopt, where you pass it a
 *  format string and it does all of the hard work for you.  The arg
 *  string basically states this program accepts a -p or -c flag, the
 *  -p flag is for a "pong message", in other words the server echos
 *  back what the client sends, and a -c message, the -c option takes
 *  a course id, and the server looks up the course id and responds
 *  with an appropriate message. 
 */
static int initParams(int argc, char *argv[], prog_config *cfg){
    int option;
    //setup defaults if no arguements are passed
    static char cmdBuffer[64] = {0};

    //setup defaults if no arguements are passed
    cfg->prog_mode = PROG_MD_CLI;
    cfg->port_number = DEF_PORT_NO;
    strcpy(cfg->file_name, PROG_DEF_FNAME);
    strcpy(cfg->svr_ip_addr, PROG_DEF_SVR_ADDR);
    
    while ((option = getopt(argc, argv, ":p:f:a:csh")) != -1){
        switch(option) {
            case 'p':
                strncpy(cmdBuffer, optarg, sizeof(cmdBuffer));
                cfg->port_number = atoi(cmdBuffer);
                break;
            case 'f':
                strncpy(cfg->file_name, optarg, sizeof(cfg->file_name));
                break;
            case 'a':
                strncpy(cfg->svr_ip_addr, optarg, sizeof(cfg->svr_ip_addr));
                break;
            case 'c':
                cfg->prog_mode = PROG_MD_CLI;
                break;
            case 's':
                cfg->prog_mode = PROG_MD_SVR;
                break;
            case 'h':
                printf("USAGE: %s [-p port] [-f fname] [-a svr_addr] [-s] [-c] [-h]\n", argv[0]);
                printf("WHERE:\n\t[-c] runs in client mode, [-s] runs in server mode; DEFAULT= client_mode\n");
                printf("\t[-a svr_addr] specifies the servers IP address as a string; DEFAULT = %s\n", cfg->svr_ip_addr);
                printf("\t[-p portnum] specifies the port number; DEFAULT = %d\n", cfg->port_number);
                printf("\t[-f fname] specifies the filename to send or recv; DEFAULT = %s\n", cfg->file_name);
                printf("\t[-p] displays what you are looking at now - the help\n\n");
                exit(0);
            case ':':
                perror ("Option missing value");
                exit(-1);
            default:
            case '?':
                perror ("Unknown option");
                exit(-1);
        }
    }
    return cfg->prog_mode;
}

int server_loop(dp_connp dpc, void *sBuff, void *rBuff, int sbuff_sz, int rbuff_sz){
    int rcvSz;


    FILE *f = fopen(full_file_path, "wb+");
    if(f == NULL){
        printf("ERROR:  Cannot open file %s\n", full_file_path);
        exit(-1);
    }
    if (dpc->isConnected == false){
        perror("Expecting the protocol to be in connect state, but its not");
        exit(-1);
    }

    char *wBuf = rBuff + sizeof(dp_ftp_pdu);
    //Loop until a disconnect is received, or error hapens
    while(1) {

        //receive request from client
        rcvSz = dprecv(dpc, rBuff, rbuff_sz);
        if (rcvSz == DP_CONNECTION_CLOSED){
            fclose(f);
            printf("Client closed connection\n");
            return DP_CONNECTION_CLOSED;
        }

        dp_ftp_pdu *inPdu = rBuff;
        dp_ftp_pdu outPdu;
        memcpy(&outPdu, inPdu, sizeof(dp_ftp_pdu));
        outPdu.status = DP_FTP_RCVOK;

        if(inPdu->status < 0) {
            printf("Received error message from client, aborting transmission.\n");
            exit(0);
        } else if(inPdu->status == DP_FTP_SNDCLOSE) {
            printf("Data transmission complete!\n");
            break;
        }

        if(inPdu->bytesSent == 0 && inPdu->status == DP_FTP_SNDOK) {
            fprintf(stderr, "DU FTP Error: Data transmission lacks data. Notifying sender and aborting.\n");
            outPdu.status = DP_FTP_ERR_NO_DATA;
        } else if(inPdu->bytesSent != (rcvSz - sizeof(dp_ftp_pdu))) {
            fprintf(stderr, "DU FTP Error: Data size mismatch. Notifying sender and aborting.\n");
            outPdu.status = DP_FTP_ERR_BAD_SIZE;
        }

        u_int16_t calc_checksum = checksum(wBuf, inPdu->bytesSent);
        if(calc_checksum != inPdu->checksum) {
            fprintf(stderr, "DU FTP Error: Checksums do not match, data corruption has occurred. Notifying sender and aborting.\n");
            outPdu.status = DP_FTP_ERR_CHKSM_FAIL;
        }


        dpsend(dpc, &outPdu, sizeof(outPdu));

        if(outPdu.status < 0) {
            exit(-1);
        }
        fwrite(wBuf, 1, inPdu->bytesSent, f);
        rcvSz = rcvSz > 50 ? 50 : rcvSz;    //Just print the first 50 characters max

        printf("========================> \n%.*s\n========================> \n", 
            rcvSz, (char *)wBuf);
    }
    return 0;
}



void start_client(dp_connp dpc){
    static char sBuff[BUFF_SZ];

    if(!dpc->isConnected) {
        printf("Client not connected\n");
        return;
    }


    FILE *f = fopen(full_file_path, "rb");

    dp_ftp_pdu *ftpPdu = (dp_ftp_pdu *)sBuff;    
    strncpy(ftpPdu->file_name, full_file_path, sizeof(ftpPdu->file_name));

    if(f == NULL){
        printf("ERROR:  Cannot open file %s\n", full_file_path);
        exit(-1);
    }
    if (dpc->isConnected == false){
        perror("Expecting the protocol to be in connect state, but its not");
        exit(-1);
    }

    //get size of file
    if(fseek(f, 0, SEEK_END) == -1) {
        perror("Failed to seek to the end of the file");
        exit(-1);
    }
    ftpPdu->file_size = ftell(f);
    rewind(f);

    int bytes = 0, status;
    char *rBuf = sBuff + sizeof(dp_ftp_pdu);
    while ((bytes = fread(rBuf, 1, sizeof(sBuff)-sizeof(dp_ftp_pdu), f )) > 0) {
        ftpPdu->bytesSent = bytes;
        ftpPdu->status = DP_FTP_SNDOK;
        ftpPdu->checksum = checksum(rBuf, bytes);

        status = dpsend(dpc, sBuff, bytes+sizeof(dp_ftp_pdu));
        ftpPdu->totlSnt += bytes;

        

        if(status < 0) { //send failed, attempt to inform server and exit
            ftpPdu->status = status;
            dpsend(dpc, ftpPdu, sizeof(dp_ftp_pdu));
            exit(-1);
        }

        dp_ftp_pdu inPdu;
        //get status from recipient
        dprecv(dpc, &inPdu, sizeof(dp_ftp_pdu));
        if(inPdu.status == DP_FTP_ERR_BAD_SIZE) {
            printf("Receiver received incorrect data size, aborting transmission.\n");
            exit(0);
        } else if(inPdu.status == DP_FTP_ERR_NO_DATA) {
            printf("Receiver received transmission missing data, aborting transmission.\n");
            exit(0);
        } else if(inPdu.status == DP_FTP_ERR_CHKSM_FAIL) {
            printf("Receiver failed checksum matching, aborting transmission.\n");
            exit(0);
        } else if(inPdu.status < 0) {
            printf("Data receipt failed, exiting.\n");
            exit(0);
        }
    }

    fclose(f);
    dpdisconnect(dpc);
}

void start_server(dp_connp dpc){
    server_loop(dpc, sbuffer, rbuffer, sizeof(sbuffer), sizeof(rbuffer));
}


int main(int argc, char *argv[])
{
    prog_config cfg;
    int cmd;
    dp_connp dpc;
    int rc;


    //Process the parameters and init the header - look at the helpers
    //in the cs472-pproto.c file
    cmd = initParams(argc, argv, &cfg);

    printf("MODE %d\n", cfg.prog_mode);
    printf("PORT %d\n", cfg.port_number);
    printf("FILE NAME: %s\n", cfg.file_name);

    switch(cmd){
        case PROG_MD_CLI:
            //by default client will look for files in the ./outfile directory
            snprintf(full_file_path, sizeof(full_file_path), "./outfile/%s", cfg.file_name);
            dpc = dpClientInit(cfg.svr_ip_addr,cfg.port_number);
            rc = dpconnect(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_client(dpc);
            exit(0);
            break;

        case PROG_MD_SVR:
            //by default server will look for files in the ./infile directory
            snprintf(full_file_path, sizeof(full_file_path), "./infile/%s", cfg.file_name);
            dpc = dpServerInit(cfg.port_number);
            rc = dplisten(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_server(dpc);
            break;
        default:
            printf("ERROR: Unknown Program Mode.  Mode set is %d\n", cmd);
            break;
    }
}

void printPdu(dp_ftp_pdu *pdu) {
    printf("DP FTP TRANSMISSION:\n\tfile name:\t\t%s \n\tfile size:\t\t%d bytes\n\ttransmission status:\t%d\n", 
        pdu->file_name, pdu->file_size, pdu->status);
}

u_int16_t checksum(char *buf, int bufSz) {
    u_int16_t total = 0;
    for(char *i = buf; (i-buf) < bufSz; i++){
        total += *i;
    }
    return total;
}