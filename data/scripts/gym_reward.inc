@ Fantasy Retold: gym leader reward (design.md §10).
@ Call with VAR_0x8008 = gym number (1-8). Offers a random held item or a random Egg of the gym's type, once.
@ Backing out (B) leaves it unclaimed; the leader offers it again when talked to.
Common_EventScript_GymReward::
	specialvar VAR_RESULT, GymReward_IsClaimed
	goto_if_eq VAR_RESULT, TRUE, Common_EventScript_GymRewardDone
	special GymReward_BufferType
	msgbox gText_GymRewardOffer, MSGBOX_DEFAULT
Common_EventScript_GymRewardChoose::
	message gText_GymRewardWhichOne
	waitmessage
	dynmultichoice 19, 6, FALSE, 2, 0, DYN_MULTICHOICE_CB_NONE, gText_GymRewardChoiceItem, gText_GymRewardChoiceEgg
	switch VAR_RESULT
	case 0, Common_EventScript_GymRewardItem
	case 1, Common_EventScript_GymRewardEgg
	msgbox gText_GymRewardLater, MSGBOX_DEFAULT
Common_EventScript_GymRewardDone::
	return

Common_EventScript_GymRewardItem::
	specialvar VAR_0x8009, GymReward_RollItem
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
	.string "As a token of your victory, you may\n"
	.string "take one more reward.\p"
	.string "An item for a POKéMON to hold\n"
	.string "in battle…\p"
	.string "…or an EGG of a {STR_VAR_2}-type\n"
	.string "POKéMON from a faraway land.$"

gText_GymRewardWhichOne::
	.string "Which will you take?$"

gText_GymRewardChoiceItem::
	.string "HELD ITEM$"

gText_GymRewardChoiceEgg::
	.string "EGG$"

gText_GymRewardLater::
	.string "Take your time. Come see me\n"
	.string "when you've decided.$"

gText_GymRewardReceivedEgg::
	.string "{PLAYER} received the EGG.$"

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
