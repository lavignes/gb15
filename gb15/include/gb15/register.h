#ifndef _GB15_REGISTER_H_
#define _GB15_REGISTER_H_

#include <gb15/types.h>

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