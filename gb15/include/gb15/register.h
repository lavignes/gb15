#ifndef _GB15_REGISTER_H_
#define _GB15_REGISTER_H_

#include <gb15/types.h>

typedef struct GB15Register {
    union {
        struct {
            u8 nil0: 1;
            u8 nil1: 1;
            u8 nil2: 1;
            u8 c: 1;
            u8 h: 1;
            u8 n: 1;
            u8 z: 1;
        };
        u8 value;
    };

} GB15Register;

typedef struct GB15LongRegister {
    union {
        struct {
            u8 l;
            u8 h;
        };
        u16 value;
    };

} GB15LongRegister;

#endif /* _GB15_REGISTER_H_ */