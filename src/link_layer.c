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

// TODO: implmentar number_of_tries nos ciclos do Rx

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int BAUDRATE;
int RETRANSMISSIONS;
LinkLayerRole ROLE;
int rxTrama = 0;
int txTrama = 0;

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
    printf("\nENTERED llopen\n");
    BAUDRATE = connectionParameters.baudRate;
    ROLE = connectionParameters.role;
    RETRANSMISSIONS = connectionParameters.nRetransmissions;

    int fd = open_serial_port(connectionParameters.serialPort);

    if (fd < 0) {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    int byte;
    unsigned char buf;

    switch (connectionParameters.role) {

        case LlRx:
            while (STOP == FALSE) {
                byte = read(fd, &buf, 1);

                if (byte > 0) {
                    STOP = process_state_receiver(buf);
                }
            }
            //int bytes = 
            send_supervision_frame(UA, fd);
            //printf("sent %d bytes\n\n", bytes);

            break;
        
        case LlTx:
            (void)signal(SIGALRM, alarmHandler);

            while (STOP == FALSE && alarmCount < connectionParameters.nRetransmissions) {
                if (alarmEnabled == FALSE) {
                    alarm(connectionParameters.timeout);
                    alarmEnabled = TRUE;

                    //int bytes = 
                    send_supervision_frame(SET, fd);
                    //printf("sent %d bytes\n\n", bytes);
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

    printf("\nLEFT llopen\n");
    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize, LinkLayer ll)
{
    printf("\nENTER llwrite\n");
    printf("tx: %d\n", txTrama);
    if (!buf) {
        printf("Invalid packet\n");
        return -1;
    }

    int new_packet_size;

    unsigned char BCC2 = calculateBCC2(buf, bufSize);
    unsigned char* stuffed_packet = byte_stuffing(buf, bufSize, &new_packet_size);
    unsigned char* frame = make_information_frame(stuffed_packet, new_packet_size, txTrama == 0 ? FRAME_NUMBER_0 : FRAME_NUMBER_1, BCC2);

    //unsigned char teste[] = {0x02, 0x00, 0x02, 0x7E, 0x05, 0x7D, 0x69};
    //unsigned char BCC2 = calculateBCC2(teste, 7);
    //unsigned char* stuffed_packet = byte_stuffing(teste, 7, &new_packet_size);
    //unsigned char* frame = make_information_frame(stuffed_packet, new_packet_size, txTrama == 0 ? FRAME_NUMBER_0 : FRAME_NUMBER_1, BCC2);

    //if (!frame) return -1;
    
    free(stuffed_packet);

    (void)signal(SIGALRM, alarmHandler);
    STOP = alarmEnabled = FALSE;
    alarmCount = 0;

   // int bytes_written, 
    int answer;

    while (STOP == FALSE && alarmCount < ll.nRetransmissions) {
        if (alarmEnabled == FALSE) {
            alarm(ll.timeout);
            alarmEnabled = TRUE;

            //bytes_written = 
            write(fd, frame, new_packet_size + 6);
            //printf("sent %d bytes\n", bytes_written);
        }

        while (alarmEnabled == TRUE) {
            unsigned char buf;
            int bytes_received = read(fd, &buf, 1);
            //printf("rx: %d tx: %d\n", rxTrama, txTrama);
            if (bytes_received > 0) {
                answer = process_state_confirmation_rejection(buf);
            }

            if (answer != -1) break;
        }

        if (answer == 0 || answer == 1) {
            if(answer != txTrama) {
                STOP = TRUE;
                alarm(0);
            }
        }
    }

    free(frame);

    if (alarmCount == ll.nRetransmissions) {
        printf("Number of tries exceeded limit\n");
        return -1;
    }

    txTrama = answer == 0 ? 0 : 1;

    printf("\nLEFT llwrite\n");
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(int fd, unsigned char *packet)
{
    static int time = 0;    // FIXME: remover depois
    int counter = 0;        // FIXME: remover depois

    printf("\nENTER llread\n");

    int error, number_of_tries = 0;
    STOP = FALSE;
    printf("rx: %d\n", rxTrama);

    unsigned char buf;
    int packet_size = 0;

    while (STOP == FALSE && number_of_tries < RETRANSMISSIONS) {
        int bytes_received = read(fd, &buf, 1);

        if (bytes_received > 0) {
            
        /*    if (time > 3 && counter == 300) {
                buf ^= 0xFF;
                time++;
                counter++;
            }  */
            //if ((rand() % 100000) < 1 * 100000 / 100) buf ^= 0xFF;
            error = process_state_information_trama(packet, buf, &packet_size);
            
            if(error == -2){
                printf("emissor run out of tries\n");
                return -2;            
            }

            // erro no campo de dados
            if (error == -1) {
                printf("sent error\n");
                send_supervision_frame(rxTrama == 0 ? REJ0 : REJ1, fd);
                packet_size = 0;
                number_of_tries++;
            }

            if (error == 1) {
                printf("sent no error\n");
                send_supervision_frame(rxTrama == 0 ? RR1 : RR0, fd);
                rxTrama = rxTrama == 0 ? 1 : 0;
                STOP = TRUE;
            }

            counter++;  // FIXME: remover depois
        }
    }

    //printf("\nPACKET\n");
    //for (int i = 0; i < packet_size; i++) {
    //    printf("pos: %d -> 0x%02X\n", i, packet[i]);
   // }

    time++;
    printf("\nLEFT llread\n");
    return packet_size;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
//TODO: showStatistics
int llclose(int showStatistics, int fd, LinkLayer ll)
{
    printf("\nENTERED llclose\n");
    int byte, cycle = 0;
    // int bytes;
    unsigned char buf;
    STOP = alarmEnabled = FALSE;
    alarmCount = 0;

    switch (ROLE) {
        
        case LlRx:
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
                    //bytes = 
                    send_supervision_frame(DISC, fd);
                    //printf("sent %d bytes\n", bytes);
                }

                cycle++;
                STOP = FALSE;
            }
            break;

        case LlTx:
            (void)signal(SIGALRM, alarmHandler);

            while (cycle < 2) {
                while (STOP == FALSE && alarmCount < ll.nRetransmissions) {

                    if (alarmEnabled == FALSE) {
                        alarm(ll.timeout);
                        alarmEnabled = TRUE;

                        if (cycle == 0)
                            //bytes = 
                            send_supervision_frame(DISC, fd);
                        else
                            //bytes = 
                            send_supervision_frame(UA, fd);

                        //printf("sent %d bytes\n\n", bytes);
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

    printf("\nLEFT llclose\n");
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

unsigned char* make_information_frame(const unsigned char* packet, int packet_size, int frame_number, unsigned char BCC2) {
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

    frame[3 + packet_size + 1] = BCC2;
    frame[3 + packet_size + 2] = FLAG;
/*
    printf("\nINFORMATION FRAME\n");
    for (int i = 0; i < packet_size + 6; i++) {
        printf("pos: %d -> 0x%02X\n", i, frame[i]);
    }
*/
    return frame;
}

unsigned char* byte_stuffing(const unsigned char* packet, int packet_size, int* new_packet_size) {
    int counter = 0;

    for (int i = 0; i < packet_size; i++) {
        if (packet[i] == FLAG || packet[i] == ESCAPE) counter++;
    }

    *new_packet_size = packet_size + counter;

    unsigned char* stuffed_packet = (unsigned char*)calloc(*new_packet_size, sizeof(unsigned char));

    if (!stuffed_packet) {
        printf("Error stuffing packet\n");
        return NULL;
    }

    int pos = 0;

    for (int i = 0; i < packet_size; i++) {
        if (packet[i] == FLAG) {
            stuffed_packet[pos++] = ESCAPE;
            stuffed_packet[pos++] = FLAG_STUFF;
        }
        else if (packet[i] == ESCAPE) {
            stuffed_packet[pos++] = ESCAPE;
            stuffed_packet[pos++] = ESCAPE_STUFF;
        }
        else {
            stuffed_packet[pos++] = packet[i];
        }
    }
/*
    printf("\nSTUFFED PAKCET\n");
    for (int i = 0; i < *new_packet_size; i++) {
        printf("pos: %d -> 0x%02X\n", i, stuffed_packet[i]);
    }
*/
    return stuffed_packet;
}
