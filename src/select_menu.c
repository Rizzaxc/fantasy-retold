#include "global.h"
#include "event_data.h"
#include "field_specials.h"
#include "link.h"
#include "select_menu.h"
#include "string_util.h"
#include "constants/flags.h"

// Fantasy Retold: the Select-button menu (design.md §2). The menu itself is Common_EventScript_SelectMenu
// (data/scripts/select_menu.inc); these are its specials.

static const u8 sText_RepelOff[] = _("REPEL: OFF");
static const u8 sText_RepelOn[] = _("REPEL: ON");
static const u8 sText_ExpShareOff[] = _("EXP. SHARE: OFF");
static const u8 sText_ExpShareOn[] = _("EXP. SHARE: ON");

static bool8 sQueuedRegisteredItem;

// The link rooms keep vanilla's Select behaviour (nothing happens).
bool32 SelectMenu_IsBlocked(void)
{
    return InUnionRoom() == TRUE || InMultiPartnerRoom() == TRUE;
}

bool32 SelectMenu_TakeQueuedRegisteredItem(void)
{
    bool32 queued = sQueuedRegisteredItem;

    sQueuedRegisteredItem = FALSE;
    return queued;
}

void SelectMenu_BufferLabels(void)
{
    StringCopy(gStringVar1, FlagGet(FLAG_INFINITE_REPEL) ? sText_RepelOn : sText_RepelOff);
    StringCopy(gStringVar2, FlagGet(FLAG_EXP_SHARE) ? sText_ExpShareOn : sText_ExpShareOff);
}

void SelectMenu_ToggleRepel(void)
{
    FlagToggle(FLAG_INFINITE_REPEL);
}

void SelectMenu_ToggleExpShare(void)
{
    FlagToggle(FLAG_EXP_SHARE);
}

void SelectMenu_QueueRegisteredItem(void)
{
    sQueuedRegisteredItem = TRUE;
}
