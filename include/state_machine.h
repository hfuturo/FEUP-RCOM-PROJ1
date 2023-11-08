#ifndef _STATE_MACHINE_H_
#define _STATE_MACHINE_H_

// Representa todos os estados possíveis
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

// teste
// Máquina de estado utilizada para mensagems de SET, UA, e DISC 
int process_message_state(unsigned char frame, unsigned char a, unsigned char c);

// Máquina de estados utilizado pelo receiver -> recebe SET
int process_state_receiver(unsigned char frame);

// Máquina de estados utilizada pelo emissor -> recebe UA
int process_state_emissor(unsigned char frame);

// Máquina de estados que recebe DISC
int process_state_disc(unsigned char frame);

// Máquina de estados que recebe um trama de informação
int process_state_information_trama(unsigned char* packet, unsigned char buf, int* pos, int* tx);

// Máquina de estados que verifica resposta do llread() -> recebe RR0 || RR1 || REJ0 || REJ1
int process_state_confirmation_rejection(unsigned char buf);

#endif
