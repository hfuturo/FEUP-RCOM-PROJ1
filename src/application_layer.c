// Application layer protocol implementation

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include "application_layer.h"
#include "link_layer.h"
#include "utils.h"
#include "files.h"

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

    //int error;

    switch (ll.role) {

        case LlRx: {
            unsigned char frame[MAX_PAYLOAD_SIZE];
            int size = -1;

            while ((size = llread(fd, frame)) < 0);
            
            long int file_size = 0;

            char* file_name = rebuildControlPacket(&file_size, frame);

            if (!file_name) return;
            printf("file name: %s\n", file_name);

            //FILE* fp = fopen(file_name, "a+");
            FILE* fp = fopen(filename, "a+");

            if (!fp) {
                printf("Error creating file\n");
                free(file_name);
            }

            free(file_name);

            unsigned int count = 0;
            long int dataLenght;

            while (TRUE) {
                if (llread(fd, frame) == -2) {
                    llclose(FALSE, fd, ll);
                    return;
                }
                
                if(frame[0] == PACKET_END){
                    long int aux;
                    char* trash = rebuildControlPacket(&aux, frame);
                    if (trash) free(trash);
                    break;  
                }

                unsigned char* data = rebuildDataPacket(&dataLenght, frame);

                //printf("\nREBUILT DATA\n");
                //for (int i = 0; i < dataLenght; i++) printf("pos: %d -> 0x%02X\n", i, data[i]);

                count += dataLenght;
                
                if(fwrite(data, sizeof(unsigned char), dataLenght, fp) < 0){
                    free(data);
                    llclose(FALSE, fd, ll);
                    return;
                }

                free(data);
            }

            int error = fclose(fp);
            if (error < 0) {
                printf("Error closing file\n");
                return;
            }

            break;
        }

        case LlTx:
            if (sendFile(fd, filename, ll) == -1) return;
            break;

        default:
            printf("Invalid role\n");
            exit(-1);
            break;
    }

    // nao esquecer estatisticas!
    llclose(FALSE, fd, ll);
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

/*
int buildControlPacket(unsigned char control, char *fileName, unsigned int fileSize, unsigned char *packetBuf){

    unsigned int n = 3;

    packetBuf[0] = control;
    packetBuf[1] = TYPE_SIZE;

    unsigned char octNumber = 0;
    unsigned int current = fileSize;

    while(current > 0){

        unsigned char oct = (unsigned char) (current & 0xff);
        current = (current >> 8);

        octNumber++;

        for(unsigned char i = octNumber + 2; i >3 ; i--){
            packetBuf[i] = packetBuf[i-1]; 
        }

        packetBuf[3]= oct;

    }

    packetBuf[2] = octNumber;

    n += octNumber;

    packetBuf[n++]= TYPE_NAME;

    packetBuf[n++] = (unsigned char) (strlen(fileName)+1);

    for(unsigned int i = 0; i<(strlen(fileName)+1); i++){
        packetBuf[n + i] = fileName[i];
    }

    return n + strlen(fileName)+1;
}
*/


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
/*
    for (int i = 0; i < *size; i++) {
        printf("pos: %d -> 0x%02X\n", i, data_packet[i]);
    } */

    return data_packet;
}


int sendFile(int fd, const char *filename, LinkLayer ll){

    int error;

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Error, invalid file\n");
        llclose(FALSE, fd, ll);
        return -1;
    }

    error = fseek(fp, 0L, SEEK_END);
    if (error < 0) {
        printf("Error setting fp to end of file\n");
        llclose(FALSE, fd, ll);
        return -1;
    }

    long int file_size = ftell(fp);
    if (file_size == -1) {
        printf("Error reading file size\n");
        llclose(FALSE, fd, ll);
        return -1;
    }

    rewind(fp);

    long int packet_size;
    unsigned char* open_control_packet = make_control_packet(PACKET_START, filename, file_size, &packet_size);

    //printf("\nCONTROL PACKET\n");
    //for (int i = 0; i < packet_size; i++) {
    //    printf("pos: %d -> 0x%02X\n", i, open_control_packet[i]);
    //}

    if (!open_control_packet) {
        printf("Error making control packet\n");
        llclose(FALSE, fd, ll);
        return -1;
    }

    if (llwrite(fd, open_control_packet, packet_size, ll) == -1) {
        llclose(FALSE, fd, ll);
        free(open_control_packet);
        return -1;
    }
    free(open_control_packet);

    unsigned char dataBuf[MAX_PAYLOAD_SIZE - 100];
    unsigned int bytes_read = 0;

    while ((bytes_read = fread(dataBuf, sizeof(unsigned char), MAX_PAYLOAD_SIZE-100, fp)) > 0) {
        int size;
        unsigned char * data_packet = buildDataPacket(dataBuf, bytes_read, &size);
       // printf("\nBUILT DATA\n");
       // for (int i = 0; i < size; i++) printf("pos: %d -> 0x%02X\n", i, data_packet[i]);
        if (llwrite(fd, data_packet, size, ll) == -1) {
            free(data_packet);
            llclose(FALSE, fd, ll);
            return -1;
        }

        free(data_packet);
    }
    
    unsigned char* close_control_packet = make_control_packet(PACKET_END, filename, file_size, &packet_size);
    if (!close_control_packet) {
        printf("Error making control packet\n");
        llclose(FALSE, fd, ll);
        return -1;
    }
    if (llwrite(fd, close_control_packet, packet_size, ll) == -1) {
        llclose(FALSE, fd, ll);
        free(open_control_packet);
        return -1;
    }
    free(close_control_packet);

    error = fclose(fp);
    if (error != 0) {
        printf("Error closing file\n");
        llclose(FALSE, fd, ll);
        return -1;
    } 

    return 0;
}

char* rebuildControlPacket(long int *fileSize, unsigned char *packetBuf){
    //printf("0x%02X\n", packetBuf[0]);
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
/*
int receiveFile(int fd){

    unsigned char dataBuf[DATA_MAX_SIZE];

    unsigned char packetBuf[PACK_MAX_SIZE];

    unsigned int packetSize = 0;

    unsigned int fileSizeStart = 0;
    unsigned int fileSizeEnd = 0;

    char fileName[256];

    packetSize = llread(fd, packetBuf);

    if (packetSize < 0){
        return -1;
    }

    if (packetBuf[0] == PACKET_START){
        if (rebuildControlPacket(fileName, &fileSizeStart, packetBuf) < 0){
            return -1;
        }
    }
    else{
        return -1;
    }

    FILE *fp = openFile(fileName, "w");

    if (fp == NULL){
        return -1;
    }

    unsigned int dataLenght;
    unsigned int count = 0;

    while(TRUE){
        packetSize = llread(fd, packetBuf);
        if(packetBuf[0] == PACKET_END){
            rebuildControlPacket(fileName, &fileSizeEnd, packetBuf);
            break;  
        }
        rebuildDataPacket(dataBuf, &dataLenght, packetBuf);
        count += dataLenght;
        
        if(fwrite(dataBuf,sizeof(unsigned char),dataLenght,fp) < 0){
            closeFile(fp);
            return -1;
        }
        

    }

    if((count |= fileSizeStart) || (count |= fileSizeEnd)){
        return -1;
    }

    if (closeFile(fp) != 0){
        return -1;
    }


    return 0;
}
*/
