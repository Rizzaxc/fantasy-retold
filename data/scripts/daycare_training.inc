@ Fantasy Retold: the daycare man's training (design.md §10).
@ He raises a party POKéMON for ¥200 a level, up to 5 levels below the party's highest level.
@ One POKéMON per conversation: backing out, a refusal or a finished training ends it.
@ Reached from Route117_EventScript_DaycareMan, which has already locked the player.
Common_EventScript_DaycareTraining::
	msgbox gText_DaycareTrainingIntro, MSGBOX_DEFAULT
	msgbox gText_DaycareTrainingWhichMon, MSGBOX_DEFAULT
	special ChoosePartyMon
	lock
	faceplayer
	goto_if_ge VAR_0x8004, PARTY_SIZE, Common_EventScript_DaycareTrainingLeave
	specialvar VAR_RESULT, DaycareTraining_BufferOffer
	goto_if_eq VAR_RESULT, DAYCARE_TRAINING_EGG, Common_EventScript_DaycareTrainingEgg
	goto_if_eq VAR_RESULT, DAYCARE_TRAINING_AT_CAP, Common_EventScript_DaycareTrainingAtCap
	goto_if_eq VAR_RESULT, DAYCARE_TRAINING_NO_MONEY, Common_EventScript_DaycareTrainingNoMoney
	message gText_DaycareTrainingOffer
	waitmessage
	@ After the message: the money box prints through STR_VAR_1, which holds the nickname.
	showmoneybox 0, 0
	dynmultichoice 29, 6, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, gText_DaycareTrainingChoiceOneLevel, gText_DaycareTrainingChoiceToLevel, gText_EggShopChoiceCancel
	goto_if_eq VAR_RESULT, 0, Common_EventScript_DaycareTrainingOneLevel
	goto_if_eq VAR_RESULT, 1, Common_EventScript_DaycareTrainingTrain
	hidemoneybox
	goto Common_EventScript_DaycareTrainingLeave

Common_EventScript_DaycareTrainingOneLevel::
	copyvar VAR_0x8005, VAR_0x8006
Common_EventScript_DaycareTrainingTrain::
	specialvar VAR_RESULT, DaycareTraining_Charge
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_DaycareTrainingChargeFailed
	updatemoneybox
	playse SE_SHOP
	msgbox gText_DaycareTrainingLeaveItToMe, MSGBOX_DEFAULT
	hidemoneybox
	special DaycareTraining_LevelUpMon
	lock
	faceplayer
	goto Common_EventScript_DaycareTrainingLeave

Common_EventScript_DaycareTrainingChargeFailed::
	hidemoneybox
Common_EventScript_DaycareTrainingNoMoney::
	msgbox gText_DaycareTrainingNotEnoughMoney, MSGBOX_DEFAULT
	release
	end

Common_EventScript_DaycareTrainingEgg::
	msgbox gText_DaycareTrainingEgg, MSGBOX_DEFAULT
	release
	end

Common_EventScript_DaycareTrainingAtCap::
	msgbox gText_DaycareTrainingAtCap, MSGBOX_DEFAULT
	release
	end

Common_EventScript_DaycareTrainingLeave::
	msgbox gText_DaycareTrainingComeAgain, MSGBOX_DEFAULT
	release
	end

gText_DaycareTrainingChoice::
	.string "TRAINING$"

gText_DaycareTrainingChoiceOneLevel::
	.string "ONE LEVEL$"

gText_DaycareTrainingChoiceToLevel::
	.string "TO LV. {STR_VAR_2}$"

gText_DaycareTrainingIntro::
	.string "I can train your POKéMON for you.\n"
	.string "It's ¥200 a level.\p"
	.string "Mind you, I only train up to five\n"
	.string "levels below your strongest one.$"

gText_DaycareTrainingWhichMon::
	.string "Which POKéMON shall I train?$"

gText_DaycareTrainingOffer::
	.string "I can raise {STR_VAR_1} as far as\n"
	.string "Lv. {STR_VAR_2} for ¥{STR_VAR_3}.\p"
	.string "How much training should it get?$"

gText_DaycareTrainingLeaveItToMe::
	.string "Leave it to me!$"

gText_DaycareTrainingNotEnoughMoney::
	.string "You can't afford even one level,\n"
	.string "I'm afraid.$"

gText_DaycareTrainingEgg::
	.string "That's an EGG! It'll need to\n"
	.string "hatch before I can train it.$"

gText_DaycareTrainingAtCap::
	.string "{STR_VAR_1} is as strong as I can\n"
	.string "make it for now.\p"
	.string "I only train up to Lv. {STR_VAR_2}, five\n"
	.string "below your strongest POKéMON.$"

gText_DaycareTrainingComeAgain::
	.string "Come back any time.$"
