// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

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

unsigned char* make_control_packet(unsigned int control_field, const char* file_name, long int file_size, long int* packet_size);

unsigned char* read_control_packet(unsigned char* packet, int packet_size, long int* file_size);

int buildControlPacket(unsigned char control, char *fileName, unsigned int fileSize, unsigned char *packetBuf);

int buildDataPacket(unsigned char *dataBuf, unsigned int dataLenght, unsigned char *packetBuf);

int sendFile(int fd, char *fileName);

int rebuildControlPacket(char *fileName, unsigned int *fileSize, unsigned char *packetBuf);

int rebuildDataPacket(unsigned char *dataBuf, unsigned int *dataLenght, unsigned char *packetBuf);

int receiveFile(int fd);

#endif // _APPLICATION_LAYER_H_
