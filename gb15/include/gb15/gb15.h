#ifndef _GB15_H_
#define _GB15_H_

#include <gb15/state.h>
#include <gb15/gpu.h>

GB15_EXTERN void gb15_boot(GB15State *state);

GB15_EXTERN void gb15_tick(GB15State *state, u8 *rom, GB15VBlankCallback vblank, void *userdata);

#endif /* _GB15_H_ */