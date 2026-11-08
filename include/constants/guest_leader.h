#ifndef GUARD_CONSTANTS_GUEST_LEADER_H
#define GUARD_CONSTANTS_GUEST_LEADER_H

// Fantasy Retold: guest gym leaders (design.md §2).

#define GUEST_LEADER_FIRST_GYM 4 // gyms 4-8 can host a guest
#ifdef RELEASE
#define GUEST_LEADER_CHANCE 24 // % per roll
#else
#define GUEST_LEADER_CHANCE 100 // % per roll (dev builds: always, for testing)
#endif
#define GUEST_LEADER_TAG_TEAM_SIZE 4 // mons each leader brings in tag team
#define GUEST_LEADER_DOUBLES_SIZE 3  // mons each leader brings in doubles
#define GUEST_LEADER_ALL_GIMMICKS_GYM 6 // from this gym on a guest uses all its gimmicks; before, one rolled

// Gimmick a guest keeps, rolled with the format (stored in VAR_GUEST_LEADER)
#define GUEST_GIMMICK_ALL 0
#define GUEST_GIMMICK_MEGA 1
#define GUEST_GIMMICK_Z_MOVE 2
#define GUEST_GIMMICK_TERA 3

// Locked format, rolled on engage
#define GUEST_FORMAT_NONE 0    // not rolled yet
#define GUEST_FORMAT_TAG 1     // guest, then the home leader, no heal
#define GUEST_FORMAT_DOUBLES 2 // guest + home leader together
#define GUEST_FORMAT_SOLO 3    // guest only

// GuestLeader_ConfigureBattle (VAR_0x8005)
#define GUEST_BATTLE_GUEST 0
#define GUEST_BATTLE_HOME 1
#define GUEST_BATTLE_DOUBLES 2

// Common_EventScript_GuestLeader result (VAR_RESULT)
#define GUEST_LEADER_RESULT_NONE 0    // no guest: run the vanilla leader battle
#define GUEST_LEADER_RESULT_WON 1     // won: run the gym's badge script
#define GUEST_LEADER_RESULT_BLOCKED 2 // not enough Pokémon for the format: release

#endif // GUARD_CONSTANTS_GUEST_LEADER_H
