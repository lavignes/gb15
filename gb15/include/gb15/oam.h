#ifndef _GB15_OAM_H_
#define _GB15_OAM_H_

#include <gb15/types.h>

typedef enum GB15Obj {
    GB15_OBJ0 =  0xFE00,
    GB15_OBJ1 =  0xFE04,
    GB15_OBJ2 =  0xFE08,
    GB15_OBJ3 =  0xFE0C,
    GB15_OBJ4 =  0xFE10,
    GB15_OBJ5 =  0xFE14,
    GB15_OBJ6 =  0xFE18,
    GB15_OBJ7 =  0xFE1C,
    GB15_OBJ8 =  0xFE20,
    GB15_OBJ9 =  0xFE24,
    GB15_OBJ10 = 0xFE28,
    GB15_OBJ11 = 0xFE2C,
    GB15_OBJ12 = 0xFE30,
    GB15_OBJ13 = 0xFE34,
    GB15_OBJ14 = 0xFE38,
    GB15_OBJ15 = 0xFE3C,
    GB15_OBJ16 = 0xFE40,
    GB15_OBJ17 = 0xFE44,
    GB15_OBJ18 = 0xFE48,
    GB15_OBJ19 = 0xFE4C,
    GB15_OBJ20 = 0xFE50,
    GB15_OBJ21 = 0xFE54,
    GB15_OBJ22 = 0xFE58,
    GB15_OBJ23 = 0xFE5C,
    GB15_OBJ24 = 0xFE60,
    GB15_OBJ25 = 0xFE64,
    GB15_OBJ26 = 0xFE68,
    GB15_OBJ27 = 0xFE6C,
    GB15_OBJ28 = 0xFE70,
    GB15_OBJ29 = 0xFE74,
    GB15_OBJ30 = 0xFE78,
    GB15_OBJ31 = 0xFE7C,
    GB15_OBJ32 = 0xFE80,
    GB15_OBJ33 = 0xFE84,
    GB15_OBJ34 = 0xFE88,
    GB15_OBJ35 = 0xFE8C,
    GB15_OBJ36 = 0xFE90,
    GB15_OBJ37 = 0xFE94,
    GB15_OBJ38 = 0xFE98,
    GB15_OBJ39 = 0xFE9C,

} GB15Obj;

typedef enum GB15OamFlag {
    GB15_OAM_FLAG_PRIO =      1 << 7,
    GB15_OAM_FLAG_FLIPV =     1 << 6,
    GB15_OAM_FLAG_FLIPH =     1 << 5,
    GB15_OAM_FLAG_DMG_COLOR = 1 << 4,
    GB15_OAM_FLAG_CHR =       1 << 3,

}GB15OamFlag;

typedef struct GB15Oam {
    u8 y;
    u8 x;
    u8 chr;
    u8 attribute;

} GB15Oam;

#endif /* _GB15_OAM_H_ */