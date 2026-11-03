@ Fantasy Retold: trainer gauntlet reward (design.md §2, Gauntlets).
@ Call with VAR_0x8009 = gauntlet id (GAUNTLET_*). Gives the one-time reward: every rolled item, then the money.
@ If the bag can't take all the items, nothing is given and VAR_RESULT = FALSE; the map's claim NPC calls
@ this again later. Returns VAR_RESULT = TRUE once the reward is in hand (also if it was claimed before).
@ The battles themselves are choreographed by each map; the trainers and rewards live in sGauntlets.
Common_EventScript_GauntletReward::
	specialvar VAR_RESULT, Gauntlet_IsClaimed
	goto_if_eq VAR_RESULT, TRUE, Common_EventScript_GauntletRewardDone
	specialvar VAR_RESULT, Gauntlet_GiveRewardItems
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_GauntletRewardBagFull
	setvar VAR_0x8004, 0
Common_EventScript_GauntletRewardNextItem::
	specialvar VAR_0x8005, Gauntlet_GetRewardItem
	goto_if_eq VAR_0x8005, ITEM_NONE, Common_EventScript_GauntletRewardMoney
	@ The items are already in the bag; reuse the obtain-item message and fanfare.
	copyvar VAR_0x8000, VAR_0x8005
	setvar VAR_0x8001, 1
	setvar VAR_0x8007, TRUE
	call EventScript_ObtainItemMessage
	addvar VAR_0x8004, 1
	goto Common_EventScript_GauntletRewardNextItem

Common_EventScript_GauntletRewardMoney::
	specialvar VAR_RESULT, Gauntlet_BufferRewardMoney
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_GauntletRewardDone
	playse SE_SHOP
	msgbox gText_GauntletRewardMoney, MSGBOX_DEFAULT
	waitse
Common_EventScript_GauntletRewardDone::
	setvar VAR_RESULT, TRUE
	return

Common_EventScript_GauntletRewardBagFull::
	msgbox gText_GauntletRewardBagFull, MSGBOX_DEFAULT
	setvar VAR_RESULT, FALSE
	return

gText_GauntletRewardMoney::
	.string "{PLAYER} received ¥{STR_VAR_1}!$"

gText_GauntletRewardBagFull::
	.string "Oh, your BAG is full.\p"
	.string "Make some room, then come back\n"
	.string "for your reward.$"
