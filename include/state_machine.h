#ifndef _STATE_MACHINE_H_
#define _STATE_MACHINE_H_

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    FRAME0_RCV,
    FRAME1_RCV,
    BCC1_RCV,
    DATA_RCV,
    DESTUFF_RCV,
    BCC2_RCV,
    STOP_RCV
} STATE;

int process_state_receiver(unsigned char frame);

int process_state_emissor(unsigned char frame);

int process_state_disc(unsigned char frame);

int process_state_information_trama(unsigned char* packet, unsigned char buf, int* pos);

int process_state_confirmation_rejection(unsigned char buf);

#endif
