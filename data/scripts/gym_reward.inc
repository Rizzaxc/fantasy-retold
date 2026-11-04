@ Fantasy Retold: gym leader reward (design.md §2).
@ Call with VAR_0x8008 = gym number (1-8). Offers one of three rewards, rolled once and shown up front:
@ a held item, a stat item, or an Egg of the gym's type. Backing out (B) leaves it unclaimed; the leader
@ offers the same three again when talked to.
Common_EventScript_GymReward::
	specialvar VAR_RESULT, GymReward_IsClaimed
	goto_if_eq VAR_RESULT, TRUE, Common_EventScript_GymRewardDone
Common_EventScript_GymRewardChoose::
	@ Also re-buffers the names, which a full-bag message overwrites.
	special GymReward_RollOffer
	message gText_GymRewardOffer
	waitmessage
	dynmultichoice 19, 6, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, gText_GymRewardChoiceHeldItem, gText_GymRewardChoiceStatItem, gText_GymRewardChoiceEgg
	switch VAR_RESULT
	case 0, Common_EventScript_GymRewardHeldItem
	case 1, Common_EventScript_GymRewardStatItem
	case 2, Common_EventScript_GymRewardEgg
	msgbox gText_GymRewardLater, MSGBOX_DEFAULT
Common_EventScript_GymRewardDone::
	return

Common_EventScript_GymRewardHeldItem::
	specialvar VAR_0x8009, GymReward_GetHeldItem
	goto Common_EventScript_GymRewardGiveItem

Common_EventScript_GymRewardStatItem::
	specialvar VAR_0x8009, GymReward_GetStatItem
Common_EventScript_GymRewardGiveItem::
	giveitem VAR_0x8009
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_GymRewardBagFull
	special GymReward_SetClaimed
	return

Common_EventScript_GymRewardBagFull::
	msgbox gText_TooBadBagIsFull, MSGBOX_DEFAULT
	goto Common_EventScript_GymRewardChoose

Common_EventScript_GymRewardEgg::
	specialvar VAR_RESULT, GymReward_GiveEgg
	goto_if_eq VAR_RESULT, MON_CANT_GIVE, Common_EventScript_GymRewardNoRoomForEgg
	copyvar VAR_0x8009, VAR_RESULT
	special GymReward_SetClaimed
	playfanfare MUS_OBTAIN_ITEM
	message gText_GymRewardReceivedEgg
	waitfanfare
	goto_if_eq VAR_0x8009, MON_GIVEN_TO_PC, Common_EventScript_GymRewardEggToPC
	msgbox gText_GymRewardEggHatch, MSGBOX_DEFAULT
	return

Common_EventScript_GymRewardEggToPC::
	msgbox gText_GymRewardEggSentToPC, MSGBOX_DEFAULT
	return

Common_EventScript_GymRewardNoRoomForEgg::
	msgbox gText_GymRewardNoRoomForEgg, MSGBOX_DEFAULT
	goto Common_EventScript_GymRewardChoose

gText_GymRewardOffer::
	.string "You also get to choose\n"
	.string "one of these rewards.\n"

gText_GymRewardChoiceHeldItem::
	.string "{STR_VAR_1}$"

gText_GymRewardChoiceStatItem::
	.string "{STR_VAR_2}$"

gText_GymRewardChoiceEgg::
	.string "{STR_VAR_3} EGG$"

gText_GymRewardLater::
	.string "Take your time. Come see me\n"
	.string "when you've decided.$"

gText_GymRewardReceivedEgg::
	.string "{PLAYER} received the {STR_VAR_3} EGG.$"

gText_GymRewardEggHatch::
	.string "Keep it with you as you travel,\n"
	.string "and it will hatch before long.$"

gText_GymRewardEggSentToPC::
	.string "Your party is full, so the EGG\n"
	.string "was sent to the PC.\p"
	.string "Keep it with you as you travel,\n"
	.string "and it will hatch before long.$"

gText_GymRewardNoRoomForEgg::
	.string "Your party and PC are both full.\n"
	.string "There's no room for the EGG.$"
