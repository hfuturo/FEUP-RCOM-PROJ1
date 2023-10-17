#include <stdio.h>

#include "state_machine.h"
#include "utils.h"

int process_state_receiver(unsigned char frame) {
    static STATE state = START;

 //   printf("current state: %d\t current buf: 0x%02X\t", state, frame);

    switch (frame) {

        case FLAG:
            if (state == BCC1_RCV) state = STOP_RCV;
            else state = FLAG_RCV;
            break;

        case A:
            if (state == FLAG_RCV) state = A_RCV;
            else if (state == A_RCV) state = C_RCV; // SET == A
            else state = START;
            break;

        case BCC1_EMISSOR:
            if (state == C_RCV) state = BCC1_RCV;
            else state = START;
            break;

        default:
            printf("Invalid trama/frame\n");
            break;
    }

  //  printf("updated state: %d\n", state);

    if (state == STOP_RCV) {
        state = START;
        return 1;
    }

    return 0;
}

int process_state_emissor(unsigned char frame) {
    static STATE state = START;

 //   printf("current state: %d\t current buf: 0x%02X\t", state, frame);

    switch (frame) {

        case FLAG:
            if (state == BCC1_RCV) state = STOP_RCV;
            else state = FLAG_RCV;
            break;

        case A:
            if (state == FLAG_RCV) state = A_RCV;
            else state = START;
            break;

        case UA:
            if (state == A_RCV) state = C_RCV;
            else state = START;
            break;

        case BCC1_RECEIVER:
            if (state == C_RCV) state = BCC1_RCV;
            else state = START;
            break;

        default:
            printf("Invalid trama/frame\n");
            break;
    }

  //  printf("updated state: %d\n", state);

    if (state == STOP_RCV) {
        state = START;
        return 1;
    }

    return 0;
}

int process_state_disc(unsigned char frame) {
    static STATE state = START;

 //   printf("current state: %d\t current buf: 0x%02X\t", state, frame);

    switch (frame) {

        case FLAG:
            if (state == BCC1_RCV) state = STOP_RCV;
            else state = FLAG_RCV;
            break;

        case A:
            if (state == FLAG_RCV) state = A_RCV;
            else state = START;
            break;

        case DISC:
            if (state == A_RCV) state = C_RCV;
            else state = START;
            break;

        case BCC1_DISC:
            if (state == C_RCV) state = BCC1_RCV;
            else state = START;
            break;

        default:
            printf("Invalid trama/frame\n");
            break;
    }

  //  printf("updated state: %d\n", state);

    if (state == STOP_RCV) {
        state = START;
        return 1;
    }

    return 0;
}

int process_state_information_trama(unsigned char* packet, unsigned char buf, int* pos) {
    static STATE state = START;

    //printf("buf: 0x%02X\t", buf);
    //printf("Current state: %d\t", state);

    //printf("pos: %d\n", *pos);
    switch (state) {
        case START:
            if (buf == FLAG) state = FLAG_RCV;
            break;

        case FLAG_RCV:
            if (buf == FLAG) state = FLAG_RCV;
            else if (buf == A) state = A_RCV;
            else state = START;
            break;

        case A_RCV:
            if (buf == FRAME_NUMBER_0) state = FRAME0_RCV;
            else if (buf == FRAME_NUMBER_1) state = FRAME1_RCV;
            else if (buf == FLAG) state = FLAG_RCV;
            else state = START;
            break;

        case FRAME0_RCV:
            if (buf == (A ^ FRAME_NUMBER_0)) state = DATA_RCV;
            else if (buf == FLAG) state = FLAG_RCV;
            else state = START;     // cabeçalho errado ignoramos sem qualquer acao
            break;

        case FRAME1_RCV:
            if (buf == (A ^ FRAME_NUMBER_1)) state = DATA_RCV;
            else if (buf == FLAG) state = FLAG_RCV;
            else state = START;     // cabeçalho errado ignoramos sem qualquer acao
            break;

        case DATA_RCV:
            if (buf == ESCAPE) state = DESTUFF_RCV;
            else if (buf == FLAG) {

               // printf("\nPACKET CONTENT\n");
               // for (int i = 0; i < *pos; i++) {
               //     printf("pos: %d -> 0x%02X\n", i, packet[i]);
               // }

               // printf("pos: %d\n", *pos);

                unsigned char BCC2 = packet[*pos - 1];
                
                packet[*pos - 1] = '\0';    // terminar a string para nao ficar com BCC2 nem com FLAG
                unsigned char BCC = packet[0];

                for (int i = 1; i < *pos - 1; i++) {
                    BCC ^= packet[i];
                }

                //printf("BCC: 0x%02X  BCC2: 0x%02X\n", BCC, BCC2);

                if (BCC == BCC2) {
                    state = START;
                    return 1;
                }
                else {
                    state = START;
                    return -1;
                }
    
            }
            else {
                packet[*pos] = buf;
                *pos += 1;
            }
            break;

        case DESTUFF_RCV:
            state = DATA_RCV;
            if (buf == FLAG_STUFF) packet[*pos] = FLAG;
            else if (buf == ESCAPE_STUFF) packet[*pos] = ESCAPE;
            *pos += 1;

            break;

        default:
            break;
    }
    //printf("updated state: %d\n", state);

    if (state == STOP_RCV) {
        state = START;
    }

    return 0;
}

int process_state_confirmation_rejection(unsigned char buf) {
    static STATE state = START;
    static unsigned char answer = 0;
    //printf("current state: %d\t current buf: 0x%02X\t", state, buf);
    static int result = -1;
    unsigned char possible_answers[] = {RR0, RR1, REJ0, REJ1};

    switch (state) {

        case START:
            if (buf == FLAG) state = FLAG_RCV;
            break;

        case FLAG_RCV:
            if (buf == A) state = A_RCV;
            else if (buf == FLAG) state = FLAG_RCV;
            else state = START;
            break;

        case A_RCV:
            for (int i = 0; i < 4; i++) {
                if (buf == possible_answers[i]) {
                    result = i;
                    answer = possible_answers[i];
                    state = C_RCV;
                }
            }

            if (result != -1) break;

            if (buf == FLAG) state = FLAG_RCV;
            else state = START;

            break;

        case C_RCV:
            if (buf == (A ^ answer)) state = BCC1_RCV;
            else {
                answer = 0;
                result = -1;
                if (buf == FLAG) state = FLAG_RCV;
                else state = START;
            }
            break;

        case BCC1_RCV:
            if (buf == FLAG) state = STOP_RCV;
            else {
                state = START;
                result = -1;
                answer = 0;
            }
            break;

        default:
            break;
    }

    //printf("updated state: %d\n", state);

    if (state == STOP_RCV) {
        state = START;
        answer = 0;
        int aux = result;
        result = -1;
        return aux;
    }

    return -1;
}
