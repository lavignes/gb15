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
            union {
                u8 a;
                GB15Register a_fine;
            };
            union {
                u8 f;
                GB15Register f_fine;
            };
        };
        union {
            u16 af;
            GB15LongRegister af_fine;
        };
    };

    union {
        struct {
            union {
                u8 c;
                GB15Register c_fine;
            };
            union {
                u8 b;
                GB15Register b_fine;
            };
        };
        union {
            u16 bc;
            GB15LongRegister bc_fine;
        };
    };

    union {
        struct {
            union {
                u8 e;
                GB15Register e_fine;
            };
            union {
                u8 d;
                GB15Register d_fine;
            };
        };
        union {
            u16 de;
            GB15LongRegister de_fine;
        };
    };

    union {
        struct {
            union {
                u8 l;
                GB15Register l_fine;
            };
            union {
                u8 h;
                GB15Register h_fine;
            };
        };
        union {
            u16 hl;
            GB15LongRegister hl_fine;
        };
    };

} GB15RegFile;

#endif /* _GB15_REGFILE_H_ */