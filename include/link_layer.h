// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(int fd, const unsigned char *buf, int bufSize, LinkLayer ll);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(int fd, unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics, int fd, LinkLayer ll);

int open_serial_port(const char* serialPort);

int close_serial_port(int fd);

int send_supervision_frame(unsigned char C, int fd);

unsigned char calculateBCC2(const unsigned char* packet, int packet_size);

unsigned char* make_information_frame(const unsigned char* packet, int packet_size, int frame_number, unsigned char BCC2);

unsigned char* byte_stuffing(const unsigned char* packet, int packetSize, int* new_packet_size);

int byte_destuffing(unsigned char* packet, int packet_size);

#endif // _LINK_LAYER_H_
