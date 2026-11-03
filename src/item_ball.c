#include "global.h"
#include "item_ball.h"
#include "event_data.h"
#include "gimmick_reward.h"
#include "constants/event_objects.h"
#include "constants/items.h"

static u32 GetItemBallAmountFromTemplate(u32);
static u32 GetItemBallIdFromTemplate(u32);

static u32 GetItemBallAmountFromTemplate(u32 itemBallId)
{
    u32 amount = gMapHeader.events->objectEvents[itemBallId].movementRangeX;

    if (amount > MAX_BAG_ITEM_CAPACITY)
        return MAX_BAG_ITEM_CAPACITY;

    return (amount == 0) ? 1 : amount;
}

static u32 GetItemBallIdFromTemplate(u32 itemBallId)
{
    enum Item itemId = gMapHeader.events->objectEvents[itemBallId].trainerRange_berryTreeId;

    return (itemId >= ITEMS_COUNT) ? (ITEM_NONE + 1) : itemId;
}

void GetItemBallIdAndAmountFromTemplate(void)
{
    u32 itemBallId = (gSpecialVar_LastTalked - 1);
    // Vitamin / X-item / repel balls give a seeded Mega Stone or Z-Crystal instead (design.md §2).
    u16 gimmickItem = GetOverworldGimmickItem(gMapHeader.events->objectEvents[itemBallId].flagId);

    if (gimmickItem != ITEM_NONE)
    {
        gSpecialVar_Result = gimmickItem;
        gSpecialVar_0x8009 = 1;
        return;
    }
    gSpecialVar_Result = GetItemBallIdFromTemplate(itemBallId);
    gSpecialVar_0x8009 = GetItemBallAmountFromTemplate(itemBallId);
}
