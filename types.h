
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;

#if (0x12 << 8 | 0x34) == 0x1234
typedef union {
  struct {
    u8 lo;
    u8 hi;
  };
  uint16_t w;
} u16t;
#else
typedef union {
  struct {
    u8 hi;
    u8 lo;
  };
  uint16_t w;
} u16;
#endif
#define U16T(x) ((u16t){.w = (x)})
typedef uint16_t u16;
typedef int8_t s8;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;

#ifndef NULL
#define NULL 0
#endif

#endif
