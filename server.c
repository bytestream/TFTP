#include "server.h"
#include <pthread.h>

int sockfd;
struct sockaddr_in server, client;
unsigned int addr_len = sizeof(struct sockaddr), numbytes;
char buffer[MAX_BUF_LEN];

/*
 * Returns during packet construction
 * 	bufferLen - Required due to a number of \0's in the packet
 */
struct packet {
	char *buffer;
	int bufferLen;
};

/*
 * Args for thread
 */
struct arg_struct {
    char *filename;
    int num;
};

/*
 * Used to keep track of ACK replies for each client
 */
struct ACK *acks;
int totalACKs = 0;
struct ACK {
	int receivedACK;
	int tid;
};

/*
 * Used to keep track of DATA replies for each client
 */
struct DATA *dataBlocks;
int totalDataBlocks = 0;
struct DATA {
	int receivedData;
	char *buffer;
	int bufferLen;
	int tid;
};

/*
 * Dissect the filename from the read/write packet
 */
char *getFilename(char buffer[MAX_BUF_LEN]) {
	char *file = malloc(255); // Linux max filename length
	int i, c = 0;
	for (i = 2; i < 255; i++, c++) {
		*(file+c) = buffer[i];
		if (buffer[i] == '\0') break;
	}

	return file;
}

/*
 * Dissect the mode from the packet
 */
char *getMode(char buffer[MAX_BUF_LEN], int filename_length) {
	char *mode = malloc(8); // max length of mode is 8 chars (netascii)
	int i, c = 0; filename_length += 3;
	for (i = filename_length; i < (filename_length+8); i++, c++) {
		*(mode+c) = buffer[i];
		if (buffer[i] == '\0') break;
	}

	return mode;
}

/*
 * tGet
 */
void tGet(void *arguments) {
	struct arg_struct *args = arguments;
	// send data in 512 byte chunks
	struct packet *f;
	if ((f = getFileData(args->filename)) != NULL) {
		int dataBlock = 0;
		// not required by RFC but when testing with some clients, they required it
		struct packet *a = createACKPacket(dataBlock);
		sendPacket(a->buffer, a->bufferLen, sockfd, (struct sockaddr*)&client);
		free(a);

		dataBlock = 1;
		while (dataBlock < f[0].bufferLen) {
			struct packet *d;
			if ((d = createDataPacket(dataBlock, f[dataBlock].buffer)) == NULL) {
				struct packet *e = createErrorPacket(0, "Unable to read file.");
				sendPacket(e->buffer, e->bufferLen, sockfd, (struct sockaddr*)&client);
				break;
			}
			sendPacket(d->buffer, d->bufferLen, sockfd, (struct sockaddr*)&client);

			// wait for ACK
			printf("\tWaiting for ACK packet.\n");
			int i;
			for (i = 0; ; i++) {
				if (i == 8) {
					printf("\tTimeout on waiting for ACK response - resending the data!!\n");
					break; // resend the data
				}
				if (acks[args->num].receivedACK == 1) {
					acks[args->num].receivedACK = 0;
					dataBlock++;
					break;
				}
				usleep(500000); // sleep .5 second
			}
		}
		// decrement totalAcks
		totalACKs -= 1;
	} else {
		struct packet *e = createErrorPacket(0, "File not found.");
		sendPacket(e->buffer, e->bufferLen, sockfd, (struct sockaddr*)&client);
	}
}

/*
 * tPut - client sends server a file
 */
void tPut(void *arguments) {
	struct arg_struct *args = arguments;
	int dataBlock = 0;
	// get the data
	FILE *fp = fopen(args->filename, "wb");
	if (fp != NULL) {
		// send ACK packet
		struct packet *a = createACKPacket(dataBlock);
		sendPacket(a->buffer, a->bufferLen, sockfd, (struct sockaddr*)&client);
		free(a);

		int i;
		for (i = 0; ; i++) {
			if (i != 0 && i % 8 == 0 && dataBlocks[args->num].receivedData == 0) {
				if (dataBlocks[args->num].bufferLen < 512) break;
				printf("\tTimeout on waiting for DATA - resending ACK response!!\n");
				// we waited 4 seconds and no reply, so retrainsmit
				struct packet *a = createACKPacket(dataBlock);
				sendPacket(a->buffer, a->bufferLen, sockfd, (struct sockaddr*)&client);
				free(a);
			}
			if (dataBlocks[args->num].receivedData == 1) {
				// unset received
				dataBlocks[args->num].receivedData = 0;
				// send ACK in reply to DATA
				struct packet *a = createACKPacket(++dataBlock);
				sendPacket(a->buffer, a->bufferLen, sockfd, (struct sockaddr*)&client);
				// write file to disk
				char *theData = getData(dataBlocks[args->num].buffer, dataBlocks[args->num].bufferLen);
				fwrite(theData, sizeof(char), dataBlocks[args->num].bufferLen-4, fp); // -4 bytes from the TFTP headers
				// last packet
				if (dataBlocks[args->num].bufferLen < 512) break;
			}
			usleep(500000); // sleep .5 second
		}
		fclose(fp);
	}
	else {
		struct packet *e = createErrorPacket(2, "Unable to write to file.");
		sendPacket(e->buffer, e->bufferLen, sockfd, (struct sockaddr*)&client);
	}
}

/*
 * Main server listener
 */
int initServer(int port) {
	acks = malloc(sizeof(*acks) * 1024); // no more than 1024 concurrent connections ( we can't have this many threads any )
	dataBlocks = malloc(sizeof(*dataBlocks) * 1024); 

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	printf("\tInitialising socket...\n");
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = INADDR_ANY;
	memset(&(server.sin_zero), '\0', 8);

	printf("\tBinding to port %d...\n", port);
	if (bind(sockfd, (struct sockaddr*)&server, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	while (1) {
		printf("\tListening for packets...\n");
		if ((numbytes = recvfrom(sockfd, buffer, MAX_BUF_LEN-1, 0, (struct sockaddr*)&client, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}
	
		printf("\t\tReceived packet from %s:%d of length %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), numbytes);
		buffer[numbytes] = '\0';

		unsigned int type = getType(buffer);

		if (type == 1 || type == 2) { // read mode
			char *filename = getFilename(buffer), *mode;
			if (strchr (filename, 0x5C) || strchr (filename, 0x2F)) {
				struct packet *e = createErrorPacket(4, "You cannot traverse directories.");
				sendPacket(e->buffer, e->bufferLen, sockfd, (struct sockaddr*)&client);
				free(e);
			}
			else if (type == 1 && access(filename, F_OK) != 0) {
				struct packet *e = createErrorPacket(1, "File not found");
				sendPacket(e->buffer, e->bufferLen, sockfd, (struct sockaddr*)&client);
				free(e);
			}
			else if (strcmp("octet", (mode = getMode(buffer, strlen(filename)))) != 0) {
				struct packet *e = createErrorPacket(0, "Mode not supported");
				sendPacket(e->buffer, e->bufferLen, sockfd, (struct sockaddr*)&client);
				free(e);
			}
			else { // everything is going swimmingly

				if (type == 1) { // read mode
					acks[totalACKs++].tid = ntohs(client.sin_port); 

					struct arg_struct args;
					args.filename = filename;
					args.num = totalACKs-1;

					pthread_t child_thread;
					pthread_create(&child_thread, NULL, (void*(*)(void*))tGet, (void *)&args);
				}

				if (type == 2) { // write mode
					dataBlocks[totalDataBlocks++].tid = ntohs(client.sin_port); 

					struct arg_struct args;
					args.filename = filename;
					args.num = totalDataBlocks-1;

					pthread_t child_thread;
					pthread_create(&child_thread, NULL, (void*(*)(void*))tPut, (void *)&args);
				}
			}
		} 
		else if (type == 3) { // DATA mode
			printf("\tReceived DATA from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
			int i;
			for (i = 0; i < totalDataBlocks; i++) {
				if (dataBlocks[i].tid == ntohs(client.sin_port)) {
					dataBlocks[i].receivedData = 1;
					dataBlocks[i].buffer = buffer; // just in case we get another packet that overwrites buffer
					dataBlocks[i].bufferLen = numbytes;
					break;
				}
			}
		}
		else if (type == 4) { // ACK mode
			printf("\tReceived ACK from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
			int i;
			for (i = 0; i < totalACKs; i++) {
				if (acks[i].tid == ntohs(client.sin_port)) {
					acks[i].receivedACK = 1;
					break;
				}
			}
		}
		else if (type == 5) { // error mode
			printf("\tReceived ERROR from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
		}
		else {
			struct packet *e = createErrorPacket(0, "Unknown packet type.");
			sendPacket(e->buffer, e->bufferLen, sockfd, (struct sockaddr*)&client);
			free(e);
		}
	}

	return 0;
}
