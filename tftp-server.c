#include <stdio.h>

#include "tftp.h"
#include "server.h"
#include "client.h"

int main(int argc, char *argv[]) {
        if (getArgv(argc, argv) == -1)
                return -1;

        if (isServer) {
                printf("Initiating server...\n");
                initServer(port);
        }
        else {
                if (hostname == NULL) {
                        puts("Please specify a hostname when in client mode.");
                        help();
                        return 1;
                }
                if (isReadMode == 1 && isWriteMode == 1) {
                        puts("You cannot use both read and write mode at the same time.");
                        help();
                        return 1;
                }
                printf("Initiating client...\n");
                initClient(hostname, port, filename);
        }

        return 0;
}

