#ifndef GUARD_CONSTANTS_LAB_RESEARCH_H
#define GUARD_CONSTANTS_LAB_RESEARCH_H

// Fantasy Retold: Birch's Aide research trade (design.md §2).

// LabResearch_GetState
#define LAB_RESEARCH_CLOSED      0 // no request until the next badge / Elite Four clear
#define LAB_RESEARCH_OPEN        1 // any two Pokémon
#define LAB_RESEARCH_OPEN_TYPED  2 // one of them must have the type in STR_VAR_1

// LabResearch_CheckPair
#define LAB_RESEARCH_PAIR_OK         0
#define LAB_RESEARCH_PAIR_NEEDS_TYPE 1 // neither has the requested type
#define LAB_RESEARCH_PAIR_NO_REWARD  2 // no species beats them

// From the 4th completed request on, the Aide asks for a type.
#define LAB_RESEARCH_UNTYPED_REQUESTS 3

#endif // GUARD_CONSTANTS_LAB_RESEARCH_H
