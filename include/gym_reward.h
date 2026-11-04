#ifndef GUARD_GYM_REWARD_H
#define GUARD_GYM_REWARD_H

bool32 IsInHoennFamily(enum Species species, u32 depth);
enum Item RollGymRewardHeldItem(void);
enum Item RollGymRewardStatItem(void);

#endif // GUARD_GYM_REWARD_H
