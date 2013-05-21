#include "tftp.h"

/*
 * Returns during packet construction
 * 	bufferLen - Required due to a number of \0's in the packet
 */
struct packet {
	char *buffer;
	int bufferLen;
};

int verbose_flag;
int isServer;
int isReadMode;
int isWriteMode;
char *filename;
char *hostname;
unsigned int port = 3335;

/*
 * Custom printf using the verbose_flag
 */
int my_printf(const char *message, ...) {
	if (verbose_flag) {
		va_list arg;
		int len = 0;
		va_start(arg, message);
		len = vprintf(message, arg);
		va_end(arg);
		fflush(stdout);
		return len;
	} else {
		return 0;
	}
}

/*
 * Send command line argument help to stdout
 */
void help() {
        puts("Usage:");
        puts("\tClient: ./tftp [-p port] [-v] [-r|w file] host");
        puts("\tServer: ./tftp -l [-p port] [-v]");
        puts("\n[-p port] the listening/connect to port");
        puts("[-v] enable output");
        puts("[-l] enable server flag");
}

/*
 * Parse the command line arguments
 */
int getArgv(int argc, char *argv[]) {
	int c;
	while (1) {
		static struct option options[] = {
			{ "verbose", no_argument, 	&verbose_flag, 1 },
			{ "listen", no_argument, 	&isServer, 1 },
			{ "port", required_argument, 	0, 'p' },
			{ "read", required_argument, 	0, 'r' },
			{ "write", required_argument,  	0, 'w' },
			{ 0, 0, 0, 0 }
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "vlp:r:w:", options, &option_index);

		if (c == -1)
			break;
		switch (c) {
			case 'v':
				verbose_flag = 1;
				break;
			case 'l':
				isServer = 1;
				break;
			case 'p':
				port = atoi(optarg);
				if (port < 1 || port > 65556) {
					help();
					return -1;
				}
				break;
			case 'r':
				isReadMode = 1;
				filename = optarg;
				break;
			case 'w':
				isWriteMode = 1;
				filename = optarg;
				break;
			case '?':
				break;
			default:
				help();
				return -1;
		}
	}

	if (optind < argc) {
		while (optind < argc)
			if (optind == argc-1) // last element
				hostname = argv[optind++];
	}

	return 0;
}

/*
 * Sends a packet back to the client
 */
void sendPacket(char *message, int packetLen, int sockfd, struct sockaddr* client) {
	int bytes_sent;
	if ((bytes_sent = sendto(sockfd, message, packetLen, 0, client, sizeof(struct sockaddr))) == -1) {
		perror("sendto");
		exit(1);
	}
	printf("\tSent packet [%d bytes]: to %s:%d\n", bytes_sent, inet_ntoa(((struct sockaddr_in*)client)->sin_addr), ntohs(((struct sockaddr_in*)client)->sin_port));
}

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
struct packet *createErrorPacket(int ErrorCode, char *ErrMsg) {
	if (ErrorCode < 0 || ErrorCode > 7)
		return '\0';

	struct packet *p = malloc(sizeof(*p));

	char *buffer = malloc(strlen(ErrMsg) + 5);
		*(short*)(buffer+0) = htons(5);  
		*(short*)(buffer+2) = htons(ErrorCode);  
		strcpy(buffer+4, ErrMsg);

	p->buffer = buffer;
	p->bufferLen = strlen(ErrMsg) + 5;

	return p;
}

/*
 * Create ACK packet
 *	Mode 	4
 */
struct packet *createACKPacket(int blockNum) {
	if (blockNum < 0)
		return NULL;

	struct packet *p = malloc(sizeof(*p));

	char *buffer = malloc(sizeof(int) * 2);
		*(short*)(buffer+0) = htons(4);
		*(short*)(buffer+2) = htons(blockNum);

	p->buffer = buffer;
	p->bufferLen = 4;

	return p;
}

/*
 * getType() returns the mode: 1 (Read request), 2 (Write request), 3 (Data), 4 (Acknowledgement), 5 (Error)
 */
unsigned int getType(char buffer[MAX_BUF_LEN]) {
	if (sizeof(buffer)/sizeof(buffer[0]) >= 1)
		return (int)buffer[1];
	else
		return -1;
}

/*
 * Dissect the data from DATA packet
 */
char *getData(char buffer[MAX_BUF_LEN], int numbytes) {
	char *theData = malloc(numbytes);
	int i, c = 0;
	for (i = 4; i < numbytes; i++, c++) {
		*(theData+c) = buffer[i];
	}

	return theData;
}

/*
 * Get the binary data of the requested file
 */
struct packet *getFileData(char *filename) {
    struct packet *file;

    char *file_buffer;
    FILE *fp = fopen(filename, "rb");
    if (fp != NULL) {
        if (fseek(fp, 0L, SEEK_END) == 0) {
            long fsize = ftell(fp);
	    if (fsize == 0) return NULL;
            file = malloc((fsize/513 * sizeof(*file)));
            fseek(fp, 0L, SEEK_SET);
            int i;
            for (i = 1; i <= (fsize/512)+1; i++) {
                file_buffer = malloc(sizeof(char) * 512); // 512
                size_t len = fread(file_buffer, sizeof(char), 512, fp);
                if (len != 0) {
                    //file_buffer[++len] = '\0';
                    file[i].buffer = file_buffer;
                    file[i].bufferLen = len;
                }
            }
            file[0].bufferLen = i;
        }
		//fclose(fp);
		return file;
    }
    return NULL;
}

/*
 * Create DATA packet
 * 	Mode 	3
 */
struct packet *createDataPacket(int blockNum, char *data) {
	if (data == NULL || strlen(data) <= 0)
		return NULL;

	struct packet *p = malloc(sizeof(*p));

	char *buffer = malloc(2 + strlen(data) + 3);
		*(short*)(buffer+0) = htons(3);
		*(short*)(buffer+2) = htons(blockNum);
		strcpy(buffer+4, data);

	p->buffer = buffer;
	p->bufferLen = 2 + strlen(data) + 2;

	return p;
}
