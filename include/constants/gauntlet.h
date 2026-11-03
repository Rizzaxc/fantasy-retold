#ifndef GUARD_CONSTANTS_GAUNTLET_H
#define GUARD_CONSTANTS_GAUNTLET_H

// Fantasy Retold: trainer gauntlets (design.md §2, Gauntlets). A gauntlet is a fixed run of trainer battles
// fought back to back with no healing; its battles pay (almost) no EXP and clearing it pays a one-time reward.
// Each id indexes sGauntlets (src/gauntlet.c); its reward flag is FLAG_GAUNTLET_REWARD_1 + id (8 reserved).

#define GAUNTLET_WINSTRATE 0 // Route 111: Victor, Victoria, Vivi, Vicky (FLAG_GAUNTLET_REWARD_1)
#define GAUNTLET_COUNT     1

#define GAUNTLET_NONE      0xFF

#define GAUNTLET_MAX_REWARD_ITEMS 4 // item slots per reward (money not included)

#endif // GUARD_CONSTANTS_GAUNTLET_H
