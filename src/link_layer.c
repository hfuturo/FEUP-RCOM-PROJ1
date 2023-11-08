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
#include <time.h>

#include "link_layer.h"
#include "state_machine.h"
#include "utils.h"

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
                    //STOP = process_state_receiver(buf);
                    STOP = process_message_state(buf, A, SET);
                }
            }

            //send_supervision_frame(UA, fd);
            send_supervision_frame_2(A, UA, fd);
            break;
        
        case LlTx:
            (void)signal(SIGALRM, alarmHandler);

            while (STOP == FALSE && alarmCount < connectionParameters.nRetransmissions) {
                if (alarmEnabled == FALSE) {
                    alarm(connectionParameters.timeout);
                    alarmEnabled = TRUE;
                    //send_supervision_frame(SET, fd);
                    send_supervision_frame_2(A, SET, fd);
                }

                while (STOP == FALSE && alarmEnabled == TRUE) {
                    byte = read(fd, &buf, 1);

                    if (byte > 0) {
                        //STOP = process_state_emissor(buf);
                        STOP = process_message_state(buf, A, UA);
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
    unsigned char* stuffed_packet = byte_stuffing(buf, bufSize, &new_packet_size, BCC2);
    unsigned char* frame = make_information_frame(stuffed_packet, new_packet_size, txTrama == 0 ? FRAME_NUMBER_0 : FRAME_NUMBER_1, BCC2);
    
    free(stuffed_packet);

    (void)signal(SIGALRM, alarmHandler);
    STOP = alarmEnabled = FALSE;
    alarmCount = 0;

    int answer;

    while (STOP == FALSE && alarmCount < ll.nRetransmissions) {
        if (alarmEnabled == FALSE) {
            alarm(ll.timeout);
            alarmEnabled = TRUE;
            write(fd, frame, new_packet_size + 5);
        }
        

        while (alarmEnabled == TRUE) {
            unsigned char buf;
            int bytes_received = read(fd, &buf, 1);
            if (bytes_received > 0) {
                answer = process_state_confirmation_rejection(buf);
            }

            if (answer == 0 || answer == 1) {
                if (answer != txTrama) {
                    STOP = TRUE;
                    alarm(0);
                    break;
                }
            }
            else if (answer == 2 || answer == 3) {  // se rejeitar envia novamente
                alarm(ll.timeout);
                write(fd, frame, new_packet_size + 5);
                answer = -1;    // necessário meter answer a valor diferente de 0,1,2,3 
            }                   // para evitar enviar frame muitas vezes seguidas e crashar
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
    printf("\nENTER llread\n");

    //static int time = 0;
    int counter = 0;

    int error, number_of_tries = 0;
    STOP = FALSE;
    printf("rx: %d\n", rxTrama);

    unsigned char buf;
    int packet_size = 0;
    int tx;
    //int p_erro_BCC1 = 5;
    //int p_erro_BCC1 = 10;
    //int p_erro_BCC1 = 15;
    //int p_erro_BCC1 = 20;
    //int p_erro_BCC2 = 5;
    //int p_erro_BCC2 = 10;
    //int p_erro_BCC2 = 15;
    //int p_erro_BCC2 = 20;

    while (STOP == FALSE && number_of_tries < RETRANSMISSIONS) {
        int bytes_received = read(fd, &buf, 1);

        if (bytes_received > 0) {
            /*
            if ( counter == 3) {
                printf("Ok\n");
                int number = (rand() % 100) + 1;
                if (number <= p_erro_BCC1) {
                    buf ^= 0xFF;
                }
            }
            if (counter == 5) {
                int number = rand() % 100 + 1;
                if (number <= p_erro_BCC2) {
                    buf ^= 0xFF;
                }
            }
            */   
            error = process_state_information_trama(packet, buf, &packet_size, &tx);
 
            // emissor run out of tries
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
                if (tx != rxTrama) {
                    send_supervision_frame(rxTrama == 0 ? RR0 : RR1, fd);
                    packet_size = 0;
                    printf("same frame\n");
                    continue;
                }
                printf("sent no error\n");
                send_supervision_frame(rxTrama == 0 ? RR1 : RR0, fd);
                rxTrama = rxTrama == 0 ? 1 : 0;
                STOP = TRUE;
            }
            counter++;
        }
    }
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
    unsigned char buf;
    STOP = alarmEnabled = FALSE;
    alarmCount = 0;

    switch (ROLE) {
        
        case LlRx:

            // recebe DISC
            while (STOP == FALSE) {
                byte = read(fd, &buf, 1);

                if (byte > 0) {
                    //STOP = process_state_disc(buf);
                    STOP = process_message_state(buf, A, DISC);
                }
            }

            (void)signal(SIGALRM, alarmHandler);
            STOP = FALSE;

            // envia DISC e espera por UA
            while (STOP == FALSE && alarmCount < ll.nRetransmissions) {

                if (alarmEnabled == FALSE) {
                    alarm(ll.timeout);
                    alarmEnabled = TRUE;

                    //send_supervision_frame(DISC, fd);
                    send_supervision_frame_2(A_RECEIVER, DISC, fd);
                }

                while (STOP == FALSE && alarmEnabled == TRUE) {
                    byte = read(fd, &buf, 1);

                    if (byte > 0) {
                        //STOP = process_state_emissor(buf);
                        STOP = process_message_state(buf, A_RECEIVER, UA);
                    }

                    if (STOP) {
                        alarm(0);
                    }
                }
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
                            //send_supervision_frame(DISC, fd);
                            send_supervision_frame_2(A, DISC, fd);
                        else
                            //send_supervision_frame(UA, fd);
                            send_supervision_frame_2(A_RECEIVER, UA, fd);
                    }

                    if (cycle == 1) {
                        alarm(0);
                        break;
                    }

                    while (STOP == FALSE && alarmEnabled == TRUE) {
                        byte = read(fd, &buf, 1);

                        if (byte > 0) {
                            if (cycle == 0) 
                                //STOP = process_state_disc(buf);
                                STOP = process_message_state(buf, A_RECEIVER, DISC);
                            else                                           
                                //STOP = process_state_emissor(buf);  
                                STOP = process_message_state(buf, A, UA);   
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

// teste
int send_supervision_frame_2(unsigned char a,unsigned char c, int fd) {
    unsigned char frame[] = {FLAG, a, c, a^c, FLAG};
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
    unsigned char* frame = (unsigned char*)calloc(packet_size + 5, sizeof(unsigned char));

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

    frame[3 + packet_size + 1] = FLAG;

    return frame;
}

unsigned char* byte_stuffing(const unsigned char* packet, int packet_size, int* new_packet_size, unsigned char BCC2) {
    int counter = 0;

    for (int i = 0; i < packet_size; i++) {
        if (packet[i] == FLAG || packet[i] == ESCAPE) counter++;
    }

    counter++;  // BCC2

    int stuff_bcc2 = FALSE;

    if (BCC2 == FLAG || BCC2 == ESCAPE) {
        counter++;  // stuff BCC2
        stuff_bcc2 = TRUE;
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

    if (stuff_bcc2) {
        stuffed_packet[pos++] = ESCAPE;
        if (BCC2 == FLAG) stuffed_packet[pos++] = FLAG_STUFF;
        else stuffed_packet[pos++] = ESCAPE_STUFF;  
    }
    else {
        stuffed_packet[pos++] = BCC2;
    }
    
    return stuffed_packet;
}
