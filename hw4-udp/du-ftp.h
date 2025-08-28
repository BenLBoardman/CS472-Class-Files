#pragma once

#define PROG_MD_CLI     0
#define PROG_MD_SVR     1
#define DEF_PORT_NO     2080
#define FNAME_SZ        150
#define PROG_DEF_FNAME  "test.c"
#define PROG_DEF_SVR_ADDR   "127.0.0.1"

//DP FTP Success Codes
#define DP_FTP_SNDOK           2 //010
#define DP_FTP_SNDCLOSE        3 //011
#define DP_FTP_RCV             4 //100
#define DP_FTP_RCVOK           (DP_FTP_SNDOK | DP_FTP_RCV)  //110

//DP FTP failure codes
#define DP_FTP_ERR_BAD_SIZE     -1
#define DP_FTP_ERR_NO_DATA      -2
#define DP_FTP_ERR_CHKSM_FAIL   -3

typedef struct prog_config{
    int     prog_mode;
    int     port_number;
    char    svr_ip_addr[16];
    char    file_name[128];
} prog_config;

typedef struct dp_ftp_pdu{
    char        file_name[FNAME_SZ];
    u_int32_t   file_size; //the length of the file, in bytes
    u_int32_t   bytesSent; //the number of data bytes in the current transmission
    u_int32_t   totlSnt; //total bytes sent across all transmissions for this file
    int8_t      status; //the status of the transmission
    u_int16_t   checksum; //a 16-bit checksum/crc used to ensure no data corruption
} dp_ftp_pdu;


void printPdu(dp_ftp_pdu*);

u_int16_t checksum(char *, int);
