#ifndef GUARD_FIELD_MOVE_H
#define GUARD_FIELD_MOVE_H

#include "global.h"
#include "constants/field_move.h"

struct FieldMoveUnlock
{
   bool32 (*isUnlockedFunc)(enum FieldMove);
   const u8 *lockedMessage;
};

enum FieldMoveUnlockType
{
    CANT_UNLOCK,
    ALWAYS_UNLOCKED,
    BADGE_UNLOCK,
    FIELD_MOVE_UNLOCK_COUNT
};

struct FieldMoveInfo
{
    bool32 (*fieldMoveFunc)(void);
    enum FieldMoveUnlockType unlockType:3;
    enum Move moveID:11;
    u32 partyMsgID:7;
    u32 arg:8;
    u32 hideIfLocked:1;
    u32 padding:3;
};

extern const struct FieldMoveInfo gFieldMoveInfo[];
extern const struct FieldMoveUnlock gFieldMoveUnlocks[];

static inline bool32 SetUpFieldMove(enum FieldMove fieldMove)
{
    return gFieldMoveInfo[fieldMove].fieldMoveFunc();
}

static inline bool32 IsFieldMoveUnlocked(enum FieldMove fieldMove)
{
    return gFieldMoveUnlocks[gFieldMoveInfo[fieldMove].unlockType].isUnlockedFunc(fieldMove);
}

// Fantasy Retold: with OW_HM_WITHOUT_TEACHING, a badge-gated (HM) field move needs no Pokémon that knows it, only its badge.
static inline bool32 IsFieldMoveFreeToUse(enum FieldMove fieldMove)
{
    return OW_HM_WITHOUT_TEACHING
        && gFieldMoveInfo[fieldMove].unlockType == BADGE_UNLOCK
        && IsFieldMoveUnlocked(fieldMove);
}

static inline const u8 *FieldMove_GetLockedMessage(enum FieldMove fieldMove)
{
    return gFieldMoveUnlocks[gFieldMoveInfo[fieldMove].unlockType].lockedMessage;
}

static inline enum Move FieldMove_GetMoveId(enum FieldMove fieldMove)
{
    return gFieldMoveInfo[fieldMove].moveID;
}

static inline u32 FieldMove_GetPartyMsgID(enum FieldMove fieldMove)
{
    return gFieldMoveInfo[fieldMove].partyMsgID;
}

static inline bool32 FieldMove_IsVisible(enum FieldMove fieldMove)
{
    return !gFieldMoveInfo[fieldMove].hideIfLocked || IsFieldMoveUnlocked(fieldMove);
}

#endif //GUARD_FIELD_MOVE_H
