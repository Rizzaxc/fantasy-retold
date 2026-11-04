#include "global.h"
#include "event_data.h"
#include "item.h"
#include "move.h"
#include "random.h"
#include "reusable_tm.h"
#include "string_util.h"
#include "constants/characters.h"
#include "constants/items.h"

// Fantasy Retold: design.md §2, Reusable TMs. The player owns at most one copy of each TM: a spot or gift
// that would give an owned TM gives a random evolution item instead. The same pool stocks the town marts.
static const enum Item sClassicEvoItems[] =
{
    ITEM_FIRE_STONE,
    ITEM_WATER_STONE,
    ITEM_THUNDER_STONE,
    ITEM_LEAF_STONE,
    ITEM_MOON_STONE,
    ITEM_SUN_STONE,
    ITEM_SHINY_STONE,
    ITEM_DUSK_STONE,
    ITEM_DAWN_STONE,
    ITEM_ICE_STONE,
    ITEM_LINKING_CORD,
    ITEM_METAL_COAT,
    ITEM_KINGS_ROCK,
    ITEM_DRAGON_SCALE,
    ITEM_UPGRADE,
    ITEM_DUBIOUS_DISC,
    ITEM_PROTECTOR,
    ITEM_ELECTIRIZER,
    ITEM_MAGMARIZER,
    ITEM_REAPER_CLOTH,
    ITEM_RAZOR_CLAW,
    ITEM_RAZOR_FANG,
    ITEM_OVAL_STONE,
    ITEM_PRISM_SCALE,
    ITEM_DEEP_SEA_TOOTH,
    ITEM_DEEP_SEA_SCALE,
    ITEM_SACHET,
    ITEM_WHIPPED_DREAM,
};

u32 GetClassicEvoItemCount(void)
{
    return ARRAY_COUNT(sClassicEvoItems);
}

enum Item GetClassicEvoItem(u32 index)
{
    return sClassicEvoItems[index % ARRAY_COUNT(sClassicEvoItems)];
}

enum Item FilterObtainedItem(enum Item item)
{
    if (IsReusableTM(item) && PlayerOwnsItem(item))
        return sClassicEvoItems[Random() % ARRAY_COUNT(sClassicEvoItems)];
    return item;
}

// Std_ObtainItem / Std_FindItem: VAR_0x8000 = item, VAR_0x8001 = amount. Returns TRUE if an owned TM
// was swapped; then STR_VAR_1 = "TM24 Thunderbolt" and STR_VAR_2 = the new item, for the giver's line.
bool8 ObtainItem_FilterTM(void)
{
    enum Item item = gSpecialVar_0x8000;
    enum Item newItem = FilterObtainedItem(item);
    u8 *end;

    if (newItem == item)
        return FALSE;

    end = CopyItemName(item, gStringVar1);
    *end++ = CHAR_SPACE;
    StringCopy(end, GetMoveName(GetItemTMHMMoveId(item)));
    CopyItemName(newItem, gStringVar2);
    gSpecialVar_0x8000 = newItem;
    gSpecialVar_0x8001 = 1;
    return TRUE;
}
