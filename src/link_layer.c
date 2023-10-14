// Link layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "link_layer.h"
#include "state_machine.h"
#include "utils.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int BAUDRATE;
LinkLayerRole role;

struct termios oldtio;

void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{

    BAUDRATE = connectionParameters.baudRate;
    role = connectionParameters.role;

    int fd = open_serial_port(connectionParameters.serialPort);

    if (fd < 0) {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    int byte;
    unsigned char buf;

    switch (connectionParameters.role) {

        case LlRx:
            printf("\n\nEntered Receiver\n");

            while (STOP == FALSE) {
                byte = read(fd, &buf, 1);

                if (byte > 0) {
                    STOP = process_state_receiver(buf);
                }
            }
            int bytes = send_supervision_frame(UA, fd);
            printf("sent %d bytes\n\n", bytes);

            break;
        
        case LlTx:
            printf("\n\nEntered emissor\n");

            (void)signal(SIGALRM, alarmHandler);

            while (STOP == FALSE && alarmCount < connectionParameters.nRetransmissions) {
                if (alarmEnabled == FALSE) {
                    alarm(connectionParameters.timeout);
                    alarmEnabled = TRUE;

                    int bytes = send_supervision_frame(SET, fd);
                    printf("sent %d bytes\n\n", bytes);
                }

                while (STOP == FALSE && alarmEnabled == TRUE) {
                    byte = read(fd, &buf, 1);

                    if (byte > 0) {
                        STOP = process_state_emissor(buf);
                    }

                    if (STOP == TRUE) {
                        alarm(0);
                    }
                }
            }

            if (alarmCount >= connectionParameters.nRetransmissions) {
                close_serial_port(fd);
                return -1;
            }

            break;

        default:
            printf("Invalid roles\n");
            return -1;

    }

    printf("left llopen\n");

    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////

//TODO: byte stuffing
int llwrite(int fd, const unsigned char *buf, int bufSize)
{
    if (!buf) {
        printf("Invalid packet\n");
        return -1;
    }

    unsigned char* frame = make_information_frame(buf, bufSize, FRAME_NUMBER_0);
    if (!frame) return -1;
    
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
//TODO: showStatistics
int llclose(int showStatistics, int fd, LinkLayer ll)
{
    int byte, bytes, cycle = 0;
    unsigned char buf;
    STOP = alarmEnabled = FALSE;
    alarmCount = 0;

    switch (role) {
        
        case LlRx:
            printf("\nEntered llclose receiver\n\n");
            while (cycle < 2) {
                while (STOP == FALSE) {
                    byte = read(fd, &buf, 1);

                    if (byte > 0) {
                        if (cycle == 0)
                            STOP = process_state_disc(buf);
                        else
                            STOP = process_state_emissor(buf);  // este caso e uma excecao, uma vez que o emissor envia UA em vez de SET
                    }
                }

                if (cycle == 0) {
                    bytes = send_supervision_frame(DISC, fd);
                    printf("sent %d bytes\n", bytes);
                }

                cycle++;
                STOP = FALSE;
            }
            break;

        case LlTx:
            printf("\nEntered llclose emissor\n\n");
            (void)signal(SIGALRM, alarmHandler);

            while (cycle < 2) {
                while (STOP == FALSE && alarmCount < ll.nRetransmissions) {

                    if (alarmEnabled == FALSE) {
                        alarm(ll.timeout);
                        alarmEnabled = TRUE;

                        if (cycle == 0)
                            bytes = send_supervision_frame(DISC, fd);
                        else
                            bytes = send_supervision_frame(UA, fd);

                        printf("sent %d bytes\n\n", bytes);
                    }

                    if (cycle == 1) {
                        alarm(0);
                        break;
                    }

                    while (STOP == FALSE && alarmEnabled == TRUE) {
                        byte = read(fd, &buf, 1);

                        if (byte > 0) {
                            if (cycle == 0) 
                                STOP = process_state_disc(buf);
                            else 
                                STOP = process_state_emissor(buf);
                        }

                        if (STOP == TRUE) {
                            alarm(0);
                        }
                    }
                }

                alarmEnabled = FALSE;
                STOP = FALSE;
                alarmCount = 0;
                cycle++;
            }
            break;

        default:
            printf("Invalid role\n");
            return -1;
    }

    return close_serial_port(fd);
}

int open_serial_port(const char* serialPort) {
    int fd = open(serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0) {
        perror(serialPort);
        exit(-1);
    }

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    struct termios newtio;

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    return fd;
}


int close_serial_port(int fd) {

    printf("closing serial port\n");

    sleep(1);

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    return close(fd);
} 

int send_supervision_frame(unsigned char C, int fd) {
    unsigned char frame[] = {FLAG, A, C, A^C, FLAG};
    return write(fd, frame, 5);
}

unsigned char calculateBCC2(const unsigned char* packet, int packet_size) {
    unsigned char BCC2 = packet[0];
    for (int i = 1; i < packet_size; i++) {
        BCC2 ^= packet[i];
    }
    return BCC2;
}

unsigned char* make_information_frame(const unsigned char* packet, int packet_size, int frame_number) {
    unsigned char* frame = (unsigned char*)calloc(packet_size + 6, sizeof(unsigned char));

    if (!frame) {
        printf("Error alocating memory in send_information_frame\n");
        return NULL;
    }

    frame[0] = FLAG;
    frame[1] = A;
    frame[2] = frame_number;
    frame[3] = A ^ frame_number;

    if (!memcpy(frame + 4, packet, packet_size)) {
        printf("Error creating information frame\n");
        return NULL;
    }

    frame[3 + packet_size + 1] = calculateBCC2(packet, packet_size);
    frame[3 + packet_size + 2] = FLAG;

    printf("\nINFORMATION FRAME\n");
    for (int i = 0; i < packet_size + 6; i++) {
        printf("pos: %d -> 0x%02X\n", i, frame[i]);
    }

    return 0;
}
