#include <stdio.h>

#include "state_machine.h"
#include "utils.h"

int process_state_receiver(unsigned char frame) {
    static STATE state = START;

 //   printf("current state: %d\t current buf: 0x%02X\t", state, frame);

    switch (frame) {

        case FLAG:
            if (state == BCC_RCV) state = STOP_RCV;
            else state = FLAG_RCV;
            break;

        case A:
            if (state == FLAG_RCV) state = A_RCV;
            else if (state == A_RCV) state = C_RCV; // SET == A
            else state = START;
            break;

        case BCC1_EMISSOR:
            if (state == C_RCV) state = BCC_RCV;
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
            if (state == BCC_RCV) state = STOP_RCV;
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
            if (state == C_RCV) state = BCC_RCV;
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
            if (state == BCC_RCV) state = STOP_RCV;
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
            if (state == C_RCV) state = BCC_RCV;
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
