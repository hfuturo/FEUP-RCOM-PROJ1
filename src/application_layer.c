// Application layer protocol implementation

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "application_layer.h"
#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    if (!serialPort) {
        printf("Invalid serial port\n");
        return;
    }

    if (!role) {
        printf("Invalid role\n");
        return;
    }

    if (!filename) {
        printf("Invalid file\n");
        return;
    }

    LinkLayer ll;

    strcpy(ll.serialPort, serialPort);
    ll.role = strcmp(role,"tx") == 0 ? LlTx : LlRx;
    ll.baudRate = baudRate;
    ll.nRetransmissions = nTries;
    ll.timeout = timeout;

    int fd = llopen(ll);

    if (fd < 0) {
        perror(ll.serialPort);
        exit(-1);
    }

    switch (ll.role) {

        case LlRx:
            
            break;

        case LlTx:

            break;
    }

    // nao esquecer estatisticas!
    llclose(FALSE, fd, ll);
}
