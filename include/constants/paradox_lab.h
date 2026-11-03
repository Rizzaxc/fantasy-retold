#ifndef GUARD_CONSTANTS_PARADOX_LAB_H
#define GUARD_CONSTANTS_PARADOX_LAB_H

// Fantasy Retold: Lanette's Paradox Lab (design.md §2).

#define PARADOX_LAB_GIFT_LEVEL 10

// ParadoxLab_CheckMon
#define PARADOX_LAB_MON_EGG     0
#define PARADOX_LAB_MON_NONE    1 // no paradox counterpart
#define PARADOX_LAB_MON_PARADOX 2 // already a paradox
#define PARADOX_LAB_MON_ONE     3 // one paradox: the choice is already in VAR_0x8005
#define PARADOX_LAB_MON_TWO     4 // ancient (STR_VAR_2) or future (STR_VAR_3): the player picks

// VAR_0x8005 for ParadoxLab_BufferTarget / ParadoxLab_Convert
#define PARADOX_LAB_ANCIENT 0
#define PARADOX_LAB_FUTURE  1

#endif // GUARD_CONSTANTS_PARADOX_LAB_H
