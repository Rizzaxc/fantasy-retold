#ifndef GUARD_REUSABLE_TM_H
#define GUARD_REUSABLE_TM_H

#include "constants/items.h"

// Fantasy Retold: design.md §2, Reusable TMs
u32 GetClassicEvoItemCount(void);
enum Item GetClassicEvoItem(u32 index);
enum Item FilterObtainedItem(enum Item item);

#endif // GUARD_REUSABLE_TM_H
