@ Fantasy Retold: Mega Ring / Z-Ring and Birch's starter gift (design.md §10).

@ Called by Wattson after his gym reward. The player keeps one ring for good; B puts the choice off and he
@ offers it again when talked to.
MauvilleCity_Gym_EventScript_GiveGimmickRing::
	goto_if_ne VAR_BIRCH_GIMMICK_STATE, BIRCH_GIMMICK_NONE, Common_EventScript_NopReturn
	msgbox gText_GimmickRingOffer, MSGBOX_DEFAULT
MauvilleCity_Gym_EventScript_ChooseGimmickRing::
	message gText_GimmickRingWhichOne
	waitmessage
	dynmultichoice 19, 6, FALSE, 2, 0, DYN_MULTICHOICE_CB_NONE, gText_GimmickRingChoiceMega, gText_GimmickRingChoiceZ
	switch VAR_RESULT
	case 0, MauvilleCity_Gym_EventScript_ConfirmMegaRing
	case 1, MauvilleCity_Gym_EventScript_ConfirmZRing
	msgbox gText_GimmickRingLater, MSGBOX_DEFAULT
	return

MauvilleCity_Gym_EventScript_ConfirmMegaRing::
	msgbox gText_GimmickRingConfirmMega, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, MauvilleCity_Gym_EventScript_ChooseGimmickRing
	giveitem ITEM_MEGA_RING
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_Gym_EventScript_GimmickRingBagFull
	setvar VAR_BIRCH_GIMMICK_STATE, BIRCH_GIMMICK_RING
	msgbox gText_GimmickRingExplainMega, MSGBOX_DEFAULT
	return

MauvilleCity_Gym_EventScript_ConfirmZRing::
	msgbox gText_GimmickRingConfirmZ, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, MauvilleCity_Gym_EventScript_ChooseGimmickRing
	giveitem ITEM_Z_POWER_RING
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_Gym_EventScript_GimmickRingBagFull
	setvar VAR_BIRCH_GIMMICK_STATE, BIRCH_GIMMICK_RING
	msgbox gText_GimmickRingExplainZ, MSGBOX_DEFAULT
	return

MauvilleCity_Gym_EventScript_GimmickRingBagFull::
	msgbox gText_TooBadBagIsFull, MSGBOX_DEFAULT
	return

@ Once the player has a ring, Birch calls (ShouldDoBirchGimmickCall) and waits in the lab with the gift.
Common_EventScript_BirchGimmickCall::
	lockall
	call Common_EventScript_BirchGimmickBufferRing
	pokenavcall gText_BirchGimmickCall
	waitmessage
	setvar VAR_BIRCH_GIMMICK_STATE, BIRCH_GIMMICK_CALLED
	releaseall
	end

Common_EventScript_BirchGimmickBufferRing::
	bufferitemname STR_VAR_2, ITEM_Z_POWER_RING
	checkitem ITEM_MEGA_RING
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_NopReturn
	bufferitemname STR_VAR_2, ITEM_MEGA_RING
	return

@ From Birch in the lab, or on Route 101 / 103 if the call caught him there. Starts after lock / faceplayer.
ProfBirch_EventScript_GimmickGift::
	call Common_EventScript_BirchGimmickBufferRing
	msgbox gText_BirchGimmickGiftIntro, MSGBOX_DEFAULT
	specialvar VAR_0x8004, BirchGimmick_GetGift
	goto_if_eq VAR_0x8004, ITEM_NONE, ProfBirch_EventScript_GimmickGiftNone
	giveitem VAR_0x8004
	goto_if_eq VAR_RESULT, FALSE, ProfBirch_EventScript_GimmickGiftBagFull
	setvar VAR_BIRCH_GIMMICK_STATE, BIRCH_GIMMICK_GIFT_DONE
	checkitem ITEM_MEGA_RING
	goto_if_eq VAR_RESULT, TRUE, ProfBirch_EventScript_GimmickGiftExplainMega
	msgbox gText_BirchGimmickGiftExplainZ, MSGBOX_DEFAULT
	release
	end

ProfBirch_EventScript_GimmickGiftExplainMega::
	msgbox gText_BirchGimmickGiftExplainMega, MSGBOX_DEFAULT
	release
	end

ProfBirch_EventScript_GimmickGiftBagFull::
	msgbox gText_BirchGimmickGiftBagFull, MSGBOX_DEFAULT
	release
	end

@ The starter's line has no Mega Stone: nothing to give, so the gift is closed.
ProfBirch_EventScript_GimmickGiftNone::
	setvar VAR_BIRCH_GIMMICK_STATE, BIRCH_GIMMICK_GIFT_DONE
	msgbox gText_BirchGimmickGiftNone, MSGBOX_DEFAULT
	release
	end

gText_GimmickRingOffer::
	.string "Wahahahah! One more thing!\p"
	.string "A fellow from a faraway land\n"
	.string "left me a pair of rings.\p"
	.string "With the MEGA RING, a POKéMON\n"
	.string "holding a MEGA STONE can\l"
	.string "MEGA EVOLVE in battle.\p"
	.string "With the Z-RING, a POKéMON\n"
	.string "holding a Z-CRYSTAL can\l"
	.string "unleash a Z-MOVE.\p"
	.string "But you only get one!\n"
	.string "There's no trading back!$"

gText_GimmickRingWhichOne::
	.string "Which ring will you take?$"

gText_GimmickRingChoiceMega::
	.string "MEGA RING$"

gText_GimmickRingChoiceZ::
	.string "Z-RING$"

gText_GimmickRingLater::
	.string "Wahahahah! Mull it over!\n"
	.string "Come back when you've decided.$"

gText_GimmickRingConfirmMega::
	.string "The MEGA RING, then?\n"
	.string "You can't change your mind!$"

gText_GimmickRingConfirmZ::
	.string "The Z-RING, then?\n"
	.string "You can't change your mind!$"

gText_GimmickRingExplainMega::
	.string "Give a MEGA STONE to the POKéMON\n"
	.string "it belongs to, and it can\l"
	.string "MEGA EVOLVE once per battle.\p"
	.string "The GAME CORNER in MAUVILLE\n"
	.string "trades MEGA STONES for COINS!$"

gText_GimmickRingExplainZ::
	.string "Give a Z-CRYSTAL to a POKéMON\n"
	.string "with a matching move, and it can\l"
	.string "use a Z-MOVE once per battle.\p"
	.string "The GAME CORNER in MAUVILLE\n"
	.string "trades Z-CRYSTALS for COINS!$"

gText_BirchGimmickCall::
	.string "… … … … … …\n"
	.string "… … … … … Beep!\p"
	.string "PROF. BIRCH: Hi, {PLAYER}!\n"
	.string "It's PROF. BIRCH!\p"
	.string "WATTSON told me you got\n"
	.string "the {STR_VAR_2}!\p"
	.string "I have something to go with it,\n"
	.string "just right for your partner.\p"
	.string "Drop by my LAB in LITTLEROOT\n"
	.string "when you get the chance!\p"
	.string "… … … … … …\n"
	.string "… … … … … Click!$"

gText_BirchGimmickGiftIntro::
	.string "PROF. BIRCH: Ah, {PLAYER}!\n"
	.string "So that's the {STR_VAR_2}!\p"
	.string "My research turned this up.\n"
	.string "It's perfect for the POKéMON\l"
	.string "you chose here at the LAB.$"

gText_BirchGimmickGiftExplainMega::
	.string "PROF. BIRCH: Once your partner\n"
	.string "is fully evolved, have it hold\l"
	.string "that stone, and it can\l"
	.string "MEGA EVOLVE!$"

gText_BirchGimmickGiftExplainZ::
	.string "PROF. BIRCH: Have your partner\n"
	.string "hold that crystal, and a move\l"
	.string "of its type becomes a Z-MOVE!$"

gText_BirchGimmickGiftBagFull::
	.string "Oh, your BAG is full!\n"
	.string "Come back when you have room.$"

gText_BirchGimmickGiftNone::
	.string "PROF. BIRCH: Hmm… Sorry, {PLAYER}.\n"
	.string "It turns out your partner's line\l"
	.string "has no MEGA STONE after all.$"
