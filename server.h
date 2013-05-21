#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h> // standard socket() requirements
#include <sys/socket.h> // standard socket() requirements
#include <netinet/in.h> // standard socket() requirements
#include <arpa/inet.h> // standard socket() requirements
#include <ctype.h>
#include <netdb.h>

#include "tftp.h"

/*
 * getType() returns the mode: 1 (Read request), 2 (Write request), 3 (Data), 4 (Acknowledgement), 5 (Error)
 */
unsigned int getType(char buffer[MAX_BUF_LEN]);

/*
 * Dissect the filename from the read/write packet
 */
char *getFilename(char buffer[MAX_BUF_LEN]);

/*
 * Dissect the mode from the packet
 */
char *getMode(char buffer[MAX_BUF_LEN], int filename_length);

/*
 * tGet
 */
void tGet(void *arguments);

/*
 * tPut
 */
void tPut(void *arguments);

/*
 * Main server listener
 */
int initServer(int port);

#endif