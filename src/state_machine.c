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

int process_state_information_trama(unsigned char* packet, int packet_size) {
    static STATE state = START;
    static int counter = 0;
    unsigned char BCC2;

    for (int i = 0; i < packet_size; i++) {
        unsigned char buf = packet[i];

        printf("\npos: %d -> 0x%02X\n", i, packet[i]);
        printf("Current state: %d\t", state);
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
                if (buf == FRAME_NUMBER_0 || buf == FRAME_NUMBER_1) state = C_RCV;
                else if (buf == FLAG) state = FLAG_RCV;
                else state = START;
                break;

            case C_RCV:
                if (buf == (packet[1] ^ packet[2])) state = DATA_RCV;
                else if (buf == FLAG) state = FLAG_RCV;
                else state = START;     // cabeçalho errado ignoradas sem qualquer ação
                break;

            case DATA_RCV:
                if (counter == packet_size - 6) {
                    printf("\nbuf: 0x%02X | BCC2: 0x%02X\n", buf, BCC2);
                    if (BCC2 == buf) state = BCC2_RCV;
                    else {
                        // data wrong
                        printf("\nError, BCC2 wrong\n");
                        state = START;
                        counter = 0;
                        return -1;
                    }
                    break;
                }
                printf("counter: %d\t", counter);
                if (counter == 0) BCC2 = buf;
                else BCC2 ^= buf;

                printf("BCC2: 0x%02X\n", BCC2);

                counter++;
                break;

            case BCC2_RCV:
                if (buf == FLAG) state = STOP_RCV;
                else state = START;
                counter = 0;
                break;

            default:
                break;
        }
        printf("updated state: %d\n", state);
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
