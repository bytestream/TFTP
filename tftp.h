#ifndef TFTP_H_
#define TFTP_H_

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_BUF_LEN 1024

/*
 * Set printf to you my_printf with custom debugging
 */
#define printf my_printf

/*
 * Verbose flag (./myprog -v)
 */
extern int verbose_flag;
/*
 * isServer flag (./myprog -l) - listen mode
 */
extern int isServer;
/*
 * Hostname - of the server to connect to in client mode
 */
extern char *hostname;
/*
 * Port - of the server to connect to in client mode
 */
extern unsigned int port;
/*
 * Filename - the name of the file they wish to read/write
 */
extern char *filename;
/*
 * isReadMode - client mode to signify reading a file
 */
extern int isReadMode;
/*
 * isWriteMode - client mode to signifiy writing a file to the host
 */
extern int isWriteMode;

/*
 * Custom printf using the verbose_flag
 */
int my_printf(const char *message, ...);

/*
 * Parse the command line arguments
 */
int getArgv(int argc, char *argv[]);

/*
 * Send command line argument help to stdout
 */
void help();

/*
 * Sends a packet back to the client
 */
void sendPacket(char *message, int packetLen, int sockfd, struct sockaddr* client);

/*
 * Create ACK packet
 *	Mode 	4
 */
struct packet *createACKPacket(int blockNum);

/*
 * getType() returns the mode: 1 (Read request), 2 (Write request), 3 (Data), 4 (Acknowledgement), 5 (Error)
 */
unsigned int getType(char buffer[MAX_BUF_LEN]);

/*
 * Dissect the data from DATA packet
 */
char *getData(char buffer[MAX_BUF_LEN], int numbytes);

/*
 * Get the binary data of the requested file
 */
struct packet *getFileData(char *filename);

/*
 * Create DATA packet
 * 	Mode 	3
 */
struct packet *createDataPacket(int blockNum, char *data);

/*
 * Create the error message to send to the client
 *	0         Not defined, see error message (if any).
 *	1         File not found.
 *	2         Access violation.
 *	3         Disk full or allocation exceeded.
 *	4         Illegal TFTP operation.
 *	5         Unknown transfer ID.
 *	6         File already exists.
 *	7         No such user.
 */
struct packet *createErrorPacket(int ErrorCode, char *ErrMsg);

#endif

