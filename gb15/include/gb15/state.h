#ifndef _GB15_STATE_H_
#define _GB15_STATE_H_

#include <gb15/memmap.h>
#include <gb15/regfile.h>

typedef struct GB15State {

    GB15RegFile regfile;

    GB15MemMap memmap;

    /**
     * Number of t-states left in the current instruction
     */
    u8 tclocks;

    /**
     * Interrupts Transitioning. DI or EI was just executed.
     * This is the number of instructions left before the interrupt state
     * will transition.
     */
    u8 di_mclocks;
    u8 ei_mclocks;

    /**
     * Stop Instruction Active
     */
    bool stopped;

    /**
     * Halt Instruction Active
     */
    bool halted;

    /**
     * GPU State
     */
    u32 gpu_tclocks;
    u32 lcd[23040];
    u32 bg_tiles[23040];

} GB15State;

#endif /* _GB15_STATE_H_ */