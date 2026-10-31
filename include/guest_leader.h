#ifndef GUARD_GUEST_LEADER_H
#define GUARD_GUEST_LEADER_H

bool32 GuestLeader_TryCreateParty(struct Pokemon *party, u16 trainerId);
u32 GuestLeader_GetPrizeLevel(u16 trainerId, u32 level);
void GuestLeader_OnWhiteOut(void);

#endif // GUARD_GUEST_LEADER_H
