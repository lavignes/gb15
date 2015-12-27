#ifndef _GB15_REGFILE_H_
#define _GB15_REGFILE_H_

#include <gb15/register.h>

typedef struct GB15RegFile {
    union {
        u16 pc;
        GB15LongRegister pc_fine;
    };

    union {
        u16 sp;
        GB15LongRegister sp_fine;
    };

    union {
        struct {
            u8 f;
            u8 a;
        };
        union {
            u16 af;
            GB15LongRegister af_fine;
        };
    };

    union {
        struct {
            u8 c;
            u8 b;
        };
        union {
            u16 bc;
            GB15LongRegister bc_fine;
        };
    };

    union {
        struct {
            u8 e;
            u8 d;
        };
        union {
            u16 de;
            GB15LongRegister de_fine;
        };
    };

    union {
        struct {
            u8 l;
            u8 h;
        };
        union {
            u16 hl;
            GB15LongRegister hl_fine;
        };
    };

} GB15RegFile;

#endif /* _GB15_REGFILE_H_ */