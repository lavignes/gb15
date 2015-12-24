#ifndef _GB15_GPU_H_
#define _GB15_GPU_H_

#include <gb15/state.h>

typedef void (*GB15HBlankCallback)(GB15State *state, void *userdata);

void gb15_gpu_init(GB15State *state);

void gb15_gpu_tick(GB15State *state, GB15HBlankCallback hblank, void *userdata);

#endif /* _GB15_GPU_H_ */