#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_ntoa
#include <netdb.h> // hostent

#define MAX_BUF_LEN 1024

/*
 * Create the error message to send to the client
 *	1         Read
 *	2         Write
 */
struct packet *createReadWritePacket(int OpCode, char *Filename);

/*
 * Connect to the host and read/write a file
 */
int initClient(char *hostname, int port, char *filename);

#endif