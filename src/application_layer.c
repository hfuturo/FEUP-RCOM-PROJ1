// Application layer protocol implementation

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

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

    switch (ll.role) {

        case LlRx: {
            if (receiveFile(fd, filename, ll) == -1) {
                printf("Error receiveFile\n");
                return;
            }
            break;
        }

        case LlTx: {
            if (sendFile(fd, filename, ll) == -1) {
                printf("Error sendFile\n");
                return;
            }
            break;
        }

        default:
            printf("Invalid role\n");
            llclose(SHOW_STATISTICS, fd, ll);
            exit(-1);
            break;
    }

    // nao esquecer estatisticas!
    llclose(SHOW_STATISTICS, fd, ll);
}

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

    return packet;
} 

unsigned char* buildDataPacket(unsigned char *dataBuf, unsigned int dataLenght, int* size){

    *size = 3 + dataLenght;

    unsigned char* data_packet = (unsigned char*)calloc(*size, sizeof(unsigned char));
    if (!data_packet) {
        printf("Error creating data packet\n");
        return NULL;
    }

    data_packet[0] = PACKET_DADOS;

    unsigned char l1 = (unsigned char) (dataLenght % 256);
    unsigned char l2 = (unsigned char) (dataLenght / 256);

    data_packet[1] = l2;
    data_packet[2] = l1;

    if (!memcpy(data_packet + 3, dataBuf, dataLenght)) {
        printf("Error creating data packet\n");
        return NULL;
    }

    return data_packet;
}


int sendFile(int fd, const char *filename, LinkLayer ll){

    int error;

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Error, invalid file\n");
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    }

    error = fseek(fp, 0L, SEEK_END);
    if (error < 0) {
        printf("Error setting fp to end of file\n");
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    }

    long int file_size = ftell(fp);
    if (file_size == -1) {
        printf("Error reading file size\n");
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    }

    rewind(fp);

    long int packet_size;
    unsigned char* open_control_packet = make_control_packet(PACKET_START, filename, file_size, &packet_size);

    if (!open_control_packet) {
        printf("Error making control packet\n");
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    }

    if (llwrite(fd, open_control_packet, packet_size, ll) == -1) {
        llclose(SHOW_STATISTICS, fd, ll);
        free(open_control_packet);
        return -1;
    }
    free(open_control_packet);

    unsigned char dataBuf[MAX_PAYLOAD_SIZE - 100];
    unsigned int bytes_read = 0;

    while ((bytes_read = fread(dataBuf, sizeof(unsigned char), MAX_PAYLOAD_SIZE - 100, fp)) > 0) { 
        int size;
        unsigned char * data_packet = buildDataPacket(dataBuf, bytes_read, &size);
        if (llwrite(fd, data_packet, size, ll) == -1) {
            free(data_packet);
            llclose(SHOW_STATISTICS, fd, ll);
            return -1;
        }

        free(data_packet);
    }
    
    unsigned char* close_control_packet = make_control_packet(PACKET_END, filename, file_size, &packet_size);
    if (!close_control_packet) {
        printf("Error making control packet\n");
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    }
    if (llwrite(fd, close_control_packet, packet_size, ll) == -1) {
        llclose(SHOW_STATISTICS, fd, ll);
        free(open_control_packet);
        return -1;
    }
    free(close_control_packet);

    error = fclose(fp);
    if (error != 0) {
        printf("Error closing file\n");
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    } 

    return 0;
}

char* rebuildControlPacket(long int *fileSize, unsigned char *packetBuf){
    if((packetBuf[0] != PACKET_START) && (packetBuf[0] != PACKET_END)){
        printf("ERROR CONTROL REBUILD PACKET\n");
        return NULL;
    }

    if(packetBuf[1] != PACKET_FILE_SIZE){
        printf("ERROR FILE SIZE PACKET\n");
        return NULL;
    }

    unsigned char l1 = packetBuf[2];

    for(int i = 0; i < l1; i++){
        unsigned int tmp = (unsigned int) (packetBuf[i + 3]);
        (*fileSize) = (*fileSize) | (tmp << (8*(l1-1-i)));
    }

    unsigned int n = l1 + 3;

    if(packetBuf[n] != TYPE_NAME){
        printf("ERROR TYPE NAME PACKET\n");
        return NULL;
    }

    n++;

    unsigned char nameLenght = packetBuf[n];

    char* file_name = (char*)calloc(sizeof(char), nameLenght);
    if (!file_name) {
        printf("Error reading control packet\n");
        return NULL;
    }

    if (!memcpy(file_name, packetBuf + ++n, nameLenght)) {
        printf("Error reading control packet\n");
        return NULL;
    }

    return file_name;
}

unsigned char* rebuildDataPacket(long int *dataLenght, unsigned char *packetBuf){

    if(packetBuf[0] != PACKET_DADOS){
        return NULL;
    }

    unsigned char l1 = packetBuf[2];
    unsigned char l2 = packetBuf[1];

    *dataLenght = (long int) ((l2 * 256) + l1);

    unsigned char* data = (unsigned char*)calloc(sizeof(unsigned char), *dataLenght);
    if (!data) {
        printf("Error reading data packet\n");
        return NULL;
    }

    if (!memcpy(data, packetBuf + 3, *dataLenght)) {
        printf("Error reading data packet\n");
        free(data);
        return NULL;
    }

    return data;
}

int receiveFile(int fd, const char* filename, LinkLayer ll){
    unsigned char frame[MAX_PAYLOAD_SIZE];
    int size = -1;

    while ((size = llread(fd, frame)) < 0);
    
    long int file_size = 0;

    char* file_name = rebuildControlPacket(&file_size, frame);
    if (!file_name) {
        printf("Error filename receiveFile\n");
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    }

    FILE* fp = fopen(filename, "a+");
    if (!fp) {
        printf("Error creating file\n");
        free(file_name);
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    }

    unsigned int count = 0;
    long int dataLenght;

    while (TRUE) {
        if (llread(fd, frame) == -2) {
            llclose(SHOW_STATISTICS, fd, ll);
            return -1;
        }
        
        if(frame[0] == PACKET_END){
            long int file_size_end = 0;
            char* file_name_end = rebuildControlPacket(&file_size_end, frame);
            if (!file_name_end) {
                printf("Error filename_end\n");
                llclose(SHOW_STATISTICS, fd, ll);
                return -1;
            }

            if (strcmp(file_name, file_name_end) != 0 || file_size != file_size_end) {
                free(file_name_end);
                free(file_name);
                printf("START PACKET DIFFERS FROM END PACKET\n");
                llclose(SHOW_STATISTICS, fd, ll);
                return -1;
            }

            free(file_name);
            free(file_name_end);

            break;  
        }

        unsigned char* data = rebuildDataPacket(&dataLenght, frame);

        count += dataLenght;
        
        if(fwrite(data, sizeof(unsigned char), dataLenght, fp) < 0){
            free(data);
            llclose(SHOW_STATISTICS, fd, ll);
            return -1;
        }

        free(data);
    }

    int error = fclose(fp);
    if (error < 0) {
        printf("Error closing file\n");
        llclose(SHOW_STATISTICS, fd, ll);
        return -1;
    }

    return 0;
}

