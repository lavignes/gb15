#ifndef _GB15_TYPES_H_
#define _GB15_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef uintptr_t uz;
typedef intptr_t iz;

#ifdef __cplusplus
#define GB15_EXTERN extern "C"
#else
#define GB15_EXTERN extern
#endif

#endif /* _GB15_TYPES_H_ */