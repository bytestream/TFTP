#include "tftp.h"
#include "client.h"
#include <pthread.h>

/*
 * Global vars
 */
int sockfd;
unsigned int addrr_len = sizeof(struct sockaddr), numbytes;
struct sockaddr_in server_addr, client_addr;
char packet[MAX_BUF_LEN];

int receivedACK = 0;
int receivedDATA = 0;
int receivedERROR = 0;

/*
 * Returns during packet construction
 * 	bufferLen - Required due to a number of \0's in the packet
 */
struct packet {
	char *buffer;
	int bufferLen;
};

/*
 * Create the error message to send to the client
 *	1         Read
 *	2         Write
 */
struct packet *createReadWritePacket(int OpCode, char *Filename) {
	if (OpCode < 1 || OpCode > 2 || Filename == NULL)
		return NULL;

	struct packet* p = malloc(sizeof(*p));

	char *mode = "octet";

	char *buffer = malloc(2 + strlen(Filename) + strlen(mode) + 3); // strlen doesn't include \0
		*(short*)(buffer+0) = htons(OpCode);    
		strcpy(buffer+2, Filename);
		strcpy(buffer+strlen(Filename)+3, mode);

	p->buffer = buffer;
	p->bufferLen = 2 + strlen(Filename) + strlen(mode) + 2;

	return p;
}

/*
 * Run in a thread and listen for packets
 */
void listenForPackets(void) {
	while (1) {
		if ((numbytes = recvfrom(sockfd, packet, MAX_BUF_LEN-1, 0, (struct sockaddr*)&client_addr, &addrr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}

		int type = getType(packet);
		if (type == 3) { // DATA
			receivedDATA = 1;
		}
		if (type == 4) { // ACK
			receivedACK = 1;
		}
		if (type == 5) { // ERROR
			receivedERROR = 1;
		}
	}
}

/*
 * Connect to the host and read/write a file
 */
int initClient(char *hostname, int port, char *filename) {
	struct hostent *he;

	printf("\tResolving hostname: %s\n", hostname);
	if ((he=gethostbyname(hostname)) == NULL) {
		perror("Error resolving hostname");
		return 1;
	}

	printf("\tInitialising socket on port %d\n", port);
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Error initialising socket.");
		return 1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr*)he->h_addr);
	memset(&(server_addr.sin_zero), '\0', 8);

	struct timeval tv;
	tv.tv_sec = 10;  /* 10 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

	// create a thread listening for packets
	pthread_t listener_thread;
	pthread_create(&listener_thread, NULL, (void*(*)(void*))listenForPackets, NULL);


	if (isReadMode) {
		printf("\tSending read request for file: %s\n", filename);
		struct packet *p;
		if ((p = createReadWritePacket(1, filename)) == NULL) {
			printf("\tError creating packet.");
		}
		else { 
			int i;
			for (i = 0; ; i++) {
				if (i == 4) {
					printf("\tTransfer timed out.\n");
					exit(1);
				}
				// send RRQ
				sendPacket(p->buffer, p->bufferLen, sockfd, (struct sockaddr *)&server_addr);

				usleep(1000000);

				if (receivedERROR) {
					printf("\tError code %d: %s\n", packet[1], getData(packet, numbytes));
					exit(1);
				}
				if (receivedACK) {
					receivedACK = 0;
					break;
				}
			}

			int dataBlock = 1;
			FILE *fp = fopen(filename, "wb");
			if (fp != NULL) {
				for (i = 0; ; i++) {
					if (receivedDATA) {
						receivedDATA = 0;

						// write data
						char *theData = getData(packet, numbytes);
						fwrite(theData, sizeof(char), numbytes-4, fp);

						// send ACK
						for (i = 0; ; i++) {
							if (i == 4) {
								printf("\tTransfer timed out.\n");
								exit(1);
							}
							struct packet *ack = createACKPacket(dataBlock); 
							sendPacket(ack->buffer, ack->bufferLen, sockfd, (struct sockaddr*)&server_addr);

							usleep(1000000);

							if (receivedERROR) {
								printf("\tError code %d: %s\n", packet[1], getData(packet, numbytes));
								exit(1);
							}
							if (receivedDATA) {
								receivedDATA = 0;
								dataBlock++;
								break;
							} 
							if (numbytes < 512) exit(1);
						}
					}
				}
				fclose(fp);
			}
			else {
				printf("Error code 0: unable to open/read file.\n");
				exit(1);
			}
		}

	}
	else if (isWriteMode) {
		printf("\tSending write request for file: %s\n", filename);
		struct packet *p;
		if ((p = createReadWritePacket(2, filename)) == NULL) {
			printf("\tError creating packet.");
		} 
		else {
			// send WRQ
			int i;
			for (i = 0; ; i++) {
				if (i == 4) {
					printf("\tTransfer timed out.\n");
					exit(1);
				}
				// send RRQ
				sendPacket(p->buffer, p->bufferLen, sockfd, (struct sockaddr *)&server_addr);

				usleep(1000000);

				if (receivedERROR) {
					printf("\tError code %d: %s\n", packet[1], getData(packet, numbytes));
					exit(1);
				}
				if (receivedACK) {
					receivedACK = 0;
					break;
				}
			}
			
			// get the file data into a buffer
			struct packet *f;
			if ((f = getFileData(filename)) != NULL) {
				int dataBlock = 1, count = 0;
				printf("Buffer size: %d\n", f[0].bufferLen);
				if (f[0].bufferLen <= 0) {
					printf("\tError code 0: File is of size 0 bytes.\n");
					exit(1);
				}
				while (dataBlock < f[0].bufferLen) {
					struct packet *d = createDataPacket(dataBlock, f[dataBlock].buffer);
					sendPacket(d->buffer, d->bufferLen, sockfd, (struct sockaddr*)&server_addr);

					// wait for ACK
					printf("\tWaiting for ACK packet.\n");
					int c;
					for (c = 0; ; c++) {
						if (c == 8) {
							break; // resend the data
						}
						if (receivedACK) {
							dataBlock++;
							break;
						}
						usleep(500000);
					}
					count++;
					usleep(1000000); // without this you end up with race conditions because of threads...
				}
			}
			else {
				printf("\tError code 4: File not found.\n");
				exit(1);
			}
		}
	}

	return 0;
}
