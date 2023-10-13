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

#endif
