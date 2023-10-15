// Application layer protocol implementation

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

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

int buildDataPacket(unsigned char *dataBuf, unsigned int dataLenght, unsigned char *packetBuf){

    packetBuf[0] = PACKET_DADOS;

    unsigned char l1 = (unsigned char) (dataLenght % 256);
    unsigned char l2 = (unsigned char) (dataLenght / 256);

    packetBuf[1] = l2;
    packetBuf[2] = l1;

    for(int i = 0; i< dataLenght; i++){
        packetBuf[i + 3] = dataBuf[i];
    }

    return 3 + dataLenght;

}


int sendFile(int fd, char *fileName){

    FILE* fp = openFile(fileName, "r");

    if(fp == NULL){
        return -1;
    }

    unsigned char packetBuf[PACK_MAX_SIZE];

    unsigned int fileSize = findFileSize(fp);

    unsigned int packetSize = (unsigned int) buildControlPacket(PACKET_START, fileName, fileSize, packetBuf);

    
    if (llwrite(fd, packetBuf, packetSize) < 0)
    {
        closeFile(fp);
        return -1;
    }

    unsigned char dataBuf[DATA_MAX_SIZE];
    unsigned int bytes_read = 0;

    while(TRUE){
        bytes_read = fread(dataBuf,sizeof(unsigned char),DATA_MAX_SIZE,fp);

        packetSize = buildDataPacket(dataBuf,bytes_read, packetBuf);

        if(bytes_read != DATA_MAX_SIZE){
            if(feof(fp)){
                if(llwrite(fd,packetBuf,packetSize) < 0){
                    closeFile(fp);
                    return -1;
                }
                break;
            }
            else{
                closeFile(fp);
                return -1;
            }
        }

        if(llwrite(fd,packetBuf,packetSize) < 0){
            closeFile(fp);
            return -1;
        }
    }

    packetSize = buildControlPacket(PACKET_END, fileName, fileSize,  packetBuf);

    if (llwrite(fd, packetBuf, packetSize) < 0)
    {
        closeFile(fp);
        return -1;
    }

    if (closeFile(fp) != 0){
        return -1;
    }


    return 0;
}


int rebuildControlPacket(char *fileName, unsigned int *fileSize, unsigned char *packetBuf){

    if((packetBuf[0] != PACKET_START) && (packetBuf[0] != PACKET_END)){
        return -1;
    }

    if(packetBuf[1] != TYPE_SIZE){
        return -1;
    }

    unsigned char l1 = packetBuf[2];

    for(int i = 0; i < l1; i++){
        unsigned int tmp = (unsigned int) (packetBuf[i + 3]);
        (*fileSize) = (*fileSize) | (tmp << (8*(l1-1)));
    }

    unsigned int n = l1 + 3;

    if(packetBuf[n] != TYPE_NAME){
        return -1;
    }

    n++;

    unsigned char nameLenght = packetBuf[n];

    for(unsigned char i = 0; i < nameLenght; i++){
        fileName[i] = packetBuf[i + n]; 
    }

    return 0;
}


int rebuildDataPacket(unsigned char *dataBuf, unsigned int *dataLenght, unsigned char *packetBuf){

    if(packetBuf[0] != PACKET_DADOS){
        return -1;
    }

    unsigned char l1 = packetBuf[2];
    unsigned char l2 = packetBuf[1];

    *dataLenght = (unsigned int) ((l2 * 256) + l1);

    for(unsigned int i = 0; i < (*dataLenght); i++){
        dataBuf[i] = packetBuf[i + 3];
    }

    return 0;
}

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
