#ifndef _UTILS_H_
#define _UTILS_H_

#define FLAG            0x7E
#define A               0x03
#define SET             0x03
#define UA              0x07
#define BCC1_RECEIVER   (A^UA)  // bcc1 enviado pelo recetor
#define BCC1_EMISSOR    (A^SET) // bcc1 enviado pelo emissor
#define DISC            0x0B
#define BCC1_DISC       (DISC^A)

#define PACKET_DADOS   1
#define PACKET_START   2
#define PACKET_END     3
#define PACKET_FILE_SIZE    0
#define PACKET_FILE_NAME    1   

#define FRAME_NUMBER_0  0x00
#define FRAME_NUMBER_1  0x40

#endif
