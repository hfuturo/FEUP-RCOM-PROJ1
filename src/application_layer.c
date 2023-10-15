// Application layer protocol implementation

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "application_layer.h"
#include "link_layer.h"
#include "utils.h"

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

    int error;

    switch (ll.role) {

        case LlRx:
            unsigned char frame[MAX_PAYLOAD_SIZE];
            int size = -1;

            while ((size = llread(fd, frame)) < 0);
 /*           long int file_size;
            unsigned char* file = read_control_packet(frame + 4, packet_size - 6, &file_size);

            FILE* fp = fopen(, "a+");

            if (!fp) {
                printf("Error opening file\n");
                llclose(FALSE, fd, ll);
            }
*/

            break;

        case LlTx:

            FILE* fp = fopen(filename, "r");
            if (!fp) {
                printf("Error, invalid file\n");
                llclose(FALSE, fd, ll);
                return;
            }

            error = fseek(fp, 0L, SEEK_END);
            if (error < 0) {
                printf("Error setting fp to end of file\n");
                llclose(FALSE, fd, ll);
                return;
            }

            long int file_size = ftell(fp);
            if (file_size == -1) {
                printf("Error reading file size\n");
                llclose(FALSE, fd, ll);
                return;
            }

            rewind(fp);

            long int packet_size;
            unsigned char* open_control_packet = make_control_packet(PACKET_START, filename, file_size, &packet_size);

            if (!open_control_packet) {
                printf("Error making control packet\n");
                llclose(FALSE, fd, ll);
                return;
            }

            llwrite(fd, open_control_packet, packet_size, ll);

            error = fclose(fp);
            if (error != 0) {
                printf("Error closing file\n");
                llclose(FALSE, fd, ll);
                return;
            }

            break;

        default:
            printf("Invalid role\n");
            exit(-1);
            break;
    }

    // nao esquecer estatisticas!
    llclose(FALSE, fd, ll);
}

//TODO: nao esquecer de dar free
unsigned char* make_control_packet(unsigned int control_field, const char* file_name, long int file_size, long int *packet_size) {
    int file_name_size = strlen(file_name);
    int file_size_bytes = (int)ceil(log2f((float)file_size) / 8.0);  // obtem-se o numero de bits e divide-se por 8

    *packet_size = 1 + 2 + file_size_bytes + 2 + file_name_size;

    unsigned char* packet = (unsigned char*)calloc(sizeof(unsigned char), *packet_size);

    if (!packet) {
        printf("Error alocating memory for packet\n");
        return NULL;
    }

    int pos = 0;

    packet[pos++] = control_field;         // C
    packet[pos++] = PACKET_FILE_SIZE;      // T1
    packet[pos] = file_size_bytes;         // L1
                      
    for (int i = 0; i < file_size_bytes; i++) {                     //  2   A    D    8
        packet[pos + file_size_bytes - i] = file_size & 0xFF;       // 10 1010 1101 1000
        file_size >>= 8;
    } 

    pos += file_size_bytes + 1;

    packet[pos++] = PACKET_FILE_NAME;    // T2
    packet[pos++] = file_name_size;      // L2

    if (!memcpy(packet + pos, file_name, file_name_size)) {
        printf("Error copying %s to packet\n", file_name);
        return NULL;
    }

    printf("\nPACKET\n");
    for (int i = 0; i < *packet_size; i++) {                     
        printf("pos: %d -> 0x%02X\n", i, packet[i]);
    } 

    return packet;
} 
