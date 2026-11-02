@ Fantasy Retold: the daycare lady's egg shop (design.md §10).
@ Three Eggs sold as "<REGION> <TIER> EGG"; the species is rolled on purchase and hidden until it hatches.
@ Restocked after every badge, rival battle and Elite Four clear.
@ Reached from Route117_PokemonDayCare_EventScript_DaycareWoman, which has already locked the player.
Common_EventScript_EggShop::
	specialvar VAR_RESULT, EggShop_Restock
	goto_if_eq VAR_RESULT, 0, Common_EventScript_EggShopEmpty
	showmoneybox 0, 0
	msgbox gText_EggShopIntro, MSGBOX_DEFAULT
Common_EventScript_EggShopChoose::
	special EggShop_BufferMenu
	message gText_EggShopWhichOne
	waitmessage
	dynmultichoice 29, 1, FALSE, 4, 0, DYN_MULTICHOICE_CB_NONE, gText_EggShopRow1, gText_EggShopRow2, gText_EggShopRow3, gText_EggShopChoiceCancel
	goto_if_ge VAR_RESULT, 3, Common_EventScript_EggShopLeave
	copyvar VAR_0x8004, VAR_RESULT
	specialvar VAR_RESULT, EggShop_BufferOffer
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_EggShopSlotSoldOut
	msgbox gText_EggShopOffer, MSGBOX_YESNO
	goto_if_ne VAR_RESULT, YES, Common_EventScript_EggShopChoose
	specialvar VAR_RESULT, EggShop_CanAfford
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_EggShopNotEnoughMoney
	specialvar VAR_RESULT, EggShop_Buy
	goto_if_eq VAR_RESULT, MON_CANT_GIVE, Common_EventScript_EggShopNoRoom
	copyvar VAR_0x8005, VAR_RESULT
	updatemoneybox
	playfanfare MUS_OBTAIN_ITEM
	message gText_EggShopReceivedEgg
	waitfanfare
	call_if_eq VAR_0x8005, MON_GIVEN_TO_PC, Common_EventScript_EggShopSentToPC
	specialvar VAR_RESULT, EggShop_Restock
	goto_if_eq VAR_RESULT, 0, Common_EventScript_EggShopSoldOutNow
	goto Common_EventScript_EggShopChoose

Common_EventScript_EggShopSentToPC::
	msgbox gText_EggShopEggSentToPC, MSGBOX_DEFAULT
	return

Common_EventScript_EggShopSlotSoldOut::
	msgbox gText_EggShopSlotSoldOut, MSGBOX_DEFAULT
	goto Common_EventScript_EggShopChoose

Common_EventScript_EggShopNotEnoughMoney::
	msgbox gText_EggShopNotEnoughMoney, MSGBOX_DEFAULT
	goto Common_EventScript_EggShopChoose

Common_EventScript_EggShopNoRoom::
	msgbox gText_EggShopNoRoom, MSGBOX_DEFAULT
	goto Common_EventScript_EggShopChoose

Common_EventScript_EggShopSoldOutNow::
	hidemoneybox
	msgbox gText_EggShopSoldOut, MSGBOX_DEFAULT
	release
	end

Common_EventScript_EggShopLeave::
	hidemoneybox
	msgbox gText_EggShopComeAgain, MSGBOX_DEFAULT
	release
	end

Common_EventScript_EggShopEmpty::
	msgbox gText_EggShopSoldOut, MSGBOX_DEFAULT
	release
	end

gText_EggShopChoiceDayCare::
	.string "DAY-CARE$"

gText_EggShopChoiceEggShop::
	.string "BUY AN EGG$"

gText_EggShopChoiceCancel::
	.string "CANCEL$"

gText_EggShopRow1::
	.string "{STR_VAR_1}$"

gText_EggShopRow2::
	.string "{STR_VAR_2}$"

gText_EggShopRow3::
	.string "{STR_VAR_3}$"

gText_EggShopIntro::
	.string "We've got exotic EGGS\n"
	.string "from operating the daycare.\p"
	.string "Fine POKéMON don't come cheap,\n"
	.string "mind you.$"

gText_EggShopWhichOne::
	.string "Which EGG catches your eye?$"

gText_EggShopOffer::
	.string "This {STR_VAR_1}…\n"
	.string "Who knows what will hatch from it?\p"
	.string "It's yours for ¥{STR_VAR_2}.\n"
	.string "Will you take it?$"

gText_EggShopSlotSoldOut::
	.string "That one's already gone, dear.$"

gText_EggShopNotEnoughMoney::
	.string "Oh, dear. You don't have\n"
	.string "enough money…$"

gText_EggShopNoRoom::
	.string "Your party and PC are both full.\n"
	.string "There's no room for the EGG.$"

gText_EggShopReceivedEgg::
	.string "{PLAYER} received the EGG.$"

gText_EggShopEggSentToPC::
	.string "Your party is full, so the EGG\n"
	.string "was sent to the PC.$"

gText_EggShopSoldOut::
	.string "We've no EGGS left, I'm afraid.\p"
	.string "Continue your journey,\n"
	.string "and we will have more soon.$"

gText_EggShopComeAgain::
	.string "Come again. More EGGS turn up\n"
	.string "as you make a name for yourself.$"
