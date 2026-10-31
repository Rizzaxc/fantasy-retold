#ifndef GUARD_CONSTANTS_GUEST_LEADER_H
#define GUARD_CONSTANTS_GUEST_LEADER_H

// Fantasy Retold: guest gym leaders (design.md §10).

#define GUEST_LEADER_FIRST_GYM 4  // gyms 4-8 can host a guest
#define GUEST_LEADER_CHANCE    20 // % per roll
#define GUEST_LEADER_TRIM_SIZE 3  // mons each leader brings in tag team / doubles

// Locked format, rolled on engage
#define GUEST_FORMAT_NONE    0 // not rolled yet
#define GUEST_FORMAT_TAG     1 // guest, then the home leader, no heal
#define GUEST_FORMAT_DOUBLES 2 // guest + home leader together
#define GUEST_FORMAT_SOLO    3 // guest only

// GuestLeader_ConfigureBattle (VAR_0x8005)
#define GUEST_BATTLE_GUEST   0
#define GUEST_BATTLE_HOME    1
#define GUEST_BATTLE_DOUBLES 2

// Common_EventScript_GuestLeader result (VAR_RESULT)
#define GUEST_LEADER_RESULT_NONE    0 // no guest: run the vanilla leader battle
#define GUEST_LEADER_RESULT_WON     1 // won: run the gym's badge script
#define GUEST_LEADER_RESULT_BLOCKED 2 // not enough Pokémon for the format: release

#endif // GUARD_CONSTANTS_GUEST_LEADER_H
