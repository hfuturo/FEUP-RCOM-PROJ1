// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#include <link_layer.h>

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,  
                      int nTries, int timeout, const char *filename);

// Constrói um pacote de controlo
unsigned char* make_control_packet(unsigned int control_field, const char* file_name, long int file_size, long int* packet_size);  

// Constrói um pacote de informação
unsigned char* buildDataPacket(unsigned char *dataBuf, unsigned int dataLenght, int* size); 

// Envia um ficheiro
int sendFile(int fd, const char *filename, LinkLayer ll);   

// Reconstrói um pacote de controlo
char* rebuildControlPacket(long int *fileSize, unsigned char *packetBuf);   

// Reconstrói um pacote de informação
unsigned char* rebuildDataPacket(long int *dataLenght, unsigned char *packetBuf);   

// Recebe um ficheiro
int receiveFile(int fd, const char* filename, LinkLayer ll);    

#endif // _APPLICATION_LAYER_H_
