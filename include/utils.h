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
#define RR0             0x05    // informa emissor que esta pronto para receber trama 0
#define RR1             0x85    // informa emissor que esta pronto para receber trama 1
#define REJ0            0x01    // rejeita trama 0 enviado pelo emissor (tem erro)
#define REJ1            0x81    // rejeita trama 1 enviado pelo emissor (tem erro)
#define BCC1_FRAME0     (A^FRAME_NUMBER_0)
#define BCC1_FRAME1     (A^FRAME_NUMBER_1)

#define PACKET_DADOS   0x01
#define PACKET_START   0x02
#define PACKET_END     0x03
#define PACKET_FILE_SIZE    0x00
#define PACKET_FILE_NAME    0x01   

#define TYPE_SIZE      0x00
#define TYPE_NAME      0x01

#define DATA_MAX_SIZE   1024 
#define PACK_MAX_SIZE   (DATA_MAX_SIZE + 3) 

#define FRAME_NUMBER_0  0x00
#define FRAME_NUMBER_1  0x40

#define ESCAPE          0x7D
#define FLAG_STUFF      0x5E
#define ESCAPE_STUFF    0x5D

#define SHOW_STATISTICS     1

#endif
