#ifndef _GB15_CPU_H_
#define _GB15_CPU_H_

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

typedef struct GB15Cpu {
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

    /**
     * Interrupt Master Enable
     */
    bool ime;

    /**
     * Stop Instruction Active
     */
    bool stopped;

    /**
     * Halt Instruction Active
     */
    bool halted;

    /**
     * Interrupts saved on halt
     */
    u8 halt_flags;

} GB15Cpu;

#endif /* _GB15_CPU_H_ */