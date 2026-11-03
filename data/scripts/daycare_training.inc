@ Fantasy Retold: the daycare man's training (design.md §2).
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

@ Fantasy Retold: the daycare man's Hyper Training (design.md §2).
@ For ¥1,000 and a BOTTLE CAP he maxes one stat's IV of a party POKéMON (its hyper-trained flag), at any level.
@ He keeps going while the player has both: after each stat it's back to the stat list for the same POKéMON;
@ B there goes back to the party, and B on the party leaves.
Common_EventScript_HyperTraining::
	msgbox gText_HyperTrainingIntro, MSGBOX_DEFAULT
Common_EventScript_HyperTrainingCheckCosts::
	checkitem ITEM_BOTTLE_CAP
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_HyperTrainingNoCap
	checkmoney HYPER_TRAINING_PRICE
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_HyperTrainingNoMoney
Common_EventScript_HyperTrainingChooseMon::
	msgbox gText_HyperTrainingWhichMon, MSGBOX_DEFAULT
	special ChoosePartyMon
	lock
	faceplayer
	goto_if_ge VAR_0x8004, PARTY_SIZE, Common_EventScript_DaycareTrainingLeave
	specialvar VAR_RESULT, HyperTraining_CheckMon
	goto_if_eq VAR_RESULT, HYPER_TRAINING_EGG, Common_EventScript_HyperTrainingEgg
	goto_if_eq VAR_RESULT, HYPER_TRAINING_ALL_MAXED, Common_EventScript_HyperTrainingAllMaxed
Common_EventScript_HyperTrainingChooseStat::
	bufferpartymonnick STR_VAR_1, VAR_0x8004
	message gText_HyperTrainingWhichStat
	waitmessage
	setvar VAR_0x8005, 0
Common_EventScript_HyperTrainingPushStat::
	specialvar VAR_0x8006, HyperTraining_BufferStatRow
	dynmultipush gText_HyperTrainingStatRow, VAR_0x8006
	addvar VAR_0x8005, 1
	goto_if_lt VAR_0x8005, HYPER_TRAINING_STAT_ROWS, Common_EventScript_HyperTrainingPushStat
	dynmultipush gText_EggShopChoiceCancel, HYPER_TRAINING_STAT_ROWS
	dynmultistack 19, 1, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_ge VAR_RESULT, HYPER_TRAINING_STAT_ROWS, Common_EventScript_HyperTrainingChooseMon
	copyvar VAR_0x8005, VAR_RESULT
	specialvar VAR_RESULT, HyperTraining_CanTrainStat
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_HyperTrainingStatMaxed
	showmoneybox 0, 0
	@ The money box prints through STR_VAR_1, so re-buffer the nickname for the prompt.
	bufferpartymonnick STR_VAR_1, VAR_0x8004
	msgbox gText_HyperTrainingConfirm, MSGBOX_YESNO
	goto_if_ne VAR_RESULT, YES, Common_EventScript_HyperTrainingDeclined
	specialvar VAR_RESULT, HyperTraining_Train
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_HyperTrainingDeclined
	updatemoneybox
	bufferpartymonnick STR_VAR_1, VAR_0x8004
	playse SE_SHOP
	msgbox gText_HyperTrainingDone, MSGBOX_DEFAULT
	hidemoneybox
	checkitem ITEM_BOTTLE_CAP
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_HyperTrainingOutOfCaps
	checkmoney HYPER_TRAINING_PRICE
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_HyperTrainingNoMoney
	specialvar VAR_RESULT, HyperTraining_CheckMon
	goto_if_eq VAR_RESULT, HYPER_TRAINING_ALL_MAXED, Common_EventScript_HyperTrainingChooseMon
	goto Common_EventScript_HyperTrainingChooseStat

Common_EventScript_HyperTrainingDeclined::
	hidemoneybox
	goto Common_EventScript_HyperTrainingChooseStat

Common_EventScript_HyperTrainingStatMaxed::
	msgbox gText_HyperTrainingStatMaxed, MSGBOX_DEFAULT
	goto Common_EventScript_HyperTrainingChooseStat

Common_EventScript_HyperTrainingEgg::
	msgbox gText_DaycareTrainingEgg, MSGBOX_DEFAULT
	goto Common_EventScript_HyperTrainingChooseMon

Common_EventScript_HyperTrainingAllMaxed::
	msgbox gText_HyperTrainingAllMaxed, MSGBOX_DEFAULT
	goto Common_EventScript_HyperTrainingChooseMon

Common_EventScript_HyperTrainingNoCap::
	msgbox gText_HyperTrainingNoCap, MSGBOX_DEFAULT
	release
	end

Common_EventScript_HyperTrainingOutOfCaps::
	msgbox gText_HyperTrainingOutOfCaps, MSGBOX_DEFAULT
	release
	end

Common_EventScript_HyperTrainingNoMoney::
	msgbox gText_HyperTrainingNoMoney, MSGBOX_DEFAULT
	release
	end

gText_HyperTrainingChoice::
	.string "HYPER TRAIN$"

gText_HyperTrainingStatRow::
	.string "{STR_VAR_2}$"

gText_HyperTrainingIntro::
	.string "Bring me a BOTTLE CAP and ¥1,000,\n"
	.string "and I'll max out one stat of\l"
	.string "a POKéMON. Any level will do!$"

gText_HyperTrainingWhichMon::
	.string "Which POKéMON shall I train?$"

gText_HyperTrainingWhichStat::
	.string "Which of {STR_VAR_1}'s stats\n"
	.string "should I max out?$"

gText_HyperTrainingConfirm::
	.string "Max out {STR_VAR_1}'s {STR_VAR_2}\n"
	.string "for ¥1,000 and a BOTTLE CAP?$"

gText_HyperTrainingDone::
	.string "{STR_VAR_1}'s {STR_VAR_2}\n"
	.string "is now maxed out!$"

gText_HyperTrainingStatMaxed::
	.string "That stat can't go any higher.$"

gText_HyperTrainingAllMaxed::
	.string "{STR_VAR_1}'s stats are all\n"
	.string "maxed out already!$"

gText_HyperTrainingNoCap::
	.string "You don't have a BOTTLE CAP.\n"
	.string "Come back when you find one.$"

gText_HyperTrainingOutOfCaps::
	.string "That was your last BOTTLE CAP.\n"
	.string "Come back when you find another.$"

gText_HyperTrainingNoMoney::
	.string "You don't have enough money,\n"
	.string "I'm afraid.$"

@ Fantasy Retold: the daycare man's Signature Training (design.md §2).
@ For ¥5,000 he makes a mastered move a fully evolved POKéMON still knows its Signature Move, as on evolving
@ into its final stage. One POKéMON per conversation; B on the move list goes back to the party.
Common_EventScript_SignatureTraining::
	msgbox gText_SignatureTrainingIntro, MSGBOX_DEFAULT
	checkmoney SIGNATURE_TRAINING_PRICE
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_HyperTrainingNoMoney
Common_EventScript_SignatureTrainingChooseMon::
	msgbox gText_SignatureTrainingWhichMon, MSGBOX_DEFAULT
	special ChoosePartyMon
	lock
	faceplayer
	goto_if_ge VAR_0x8004, PARTY_SIZE, Common_EventScript_DaycareTrainingLeave
	specialvar VAR_RESULT, SignatureTraining_CheckMon
	goto_if_eq VAR_RESULT, SIGNATURE_TRAINING_EGG, Common_EventScript_SignatureTrainingEgg
	goto_if_eq VAR_RESULT, SIGNATURE_TRAINING_NOT_FINAL, Common_EventScript_SignatureTrainingNotFinal
	goto_if_eq VAR_RESULT, SIGNATURE_TRAINING_HAS_SIGNATURE, Common_EventScript_SignatureTrainingHasSignature
	goto_if_eq VAR_RESULT, SIGNATURE_TRAINING_NO_MASTERED, Common_EventScript_SignatureTrainingNoMastered
	message gText_SignatureTrainingWhichMove
	waitmessage
	setvar VAR_0x8005, 0
Common_EventScript_SignatureTrainingPushMove::
	special SignatureTraining_BufferMove
	dynmultipush gText_HyperTrainingStatRow, VAR_0x8005
	addvar VAR_0x8005, 1
	goto_if_lt VAR_0x8005, VAR_0x8006, Common_EventScript_SignatureTrainingPushMove
	dynmultipush gText_EggShopChoiceCancel, SIGNATURE_TRAINING_CANCEL
	dynmultistack 19, 2, FALSE, 5, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_ge VAR_RESULT, SIGNATURE_TRAINING_CANCEL, Common_EventScript_SignatureTrainingChooseMon
	copyvar VAR_0x8005, VAR_RESULT
	special SignatureTraining_BufferMove
	showmoneybox 0, 0
	bufferpartymonnick STR_VAR_1, VAR_0x8004
	msgbox gText_SignatureTrainingConfirm, MSGBOX_YESNO
	goto_if_ne VAR_RESULT, YES, Common_EventScript_SignatureTrainingDeclined
	specialvar VAR_RESULT, SignatureTraining_Commit
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_SignatureTrainingDeclined
	updatemoneybox
	bufferpartymonnick STR_VAR_1, VAR_0x8004
	playse SE_SHOP
	msgbox gText_SignatureTrainingDone, MSGBOX_DEFAULT
	hidemoneybox
	goto Common_EventScript_DaycareTrainingLeave

Common_EventScript_SignatureTrainingDeclined::
	hidemoneybox
	goto Common_EventScript_DaycareTrainingLeave

Common_EventScript_SignatureTrainingEgg::
	msgbox gText_DaycareTrainingEgg, MSGBOX_DEFAULT
	goto Common_EventScript_SignatureTrainingChooseMon

Common_EventScript_SignatureTrainingNotFinal::
	msgbox gText_SignatureTrainingNotFinal, MSGBOX_DEFAULT
	goto Common_EventScript_SignatureTrainingChooseMon

Common_EventScript_SignatureTrainingHasSignature::
	msgbox gText_SignatureTrainingHasSignature, MSGBOX_DEFAULT
	goto Common_EventScript_SignatureTrainingChooseMon

Common_EventScript_SignatureTrainingNoMastered::
	msgbox gText_SignatureTrainingNoMastered, MSGBOX_DEFAULT
	goto Common_EventScript_SignatureTrainingChooseMon

gText_SignatureTrainingChoice::
	.string "SIGNATURE$"

gText_SignatureTrainingIntro::
	.string "For ¥5,000, I can make a move a\n"
	.string "POKéMON has mastered into its\l"
	.string "SIGNATURE MOVE.\p"
	.string "It has to be fully evolved, mind.$"

gText_SignatureTrainingWhichMon::
	.string "Which POKéMON shall I train?$"

gText_SignatureTrainingWhichMove::
	.string "Which move should become\n"
	.string "{STR_VAR_1}'s SIGNATURE MOVE?$"

gText_SignatureTrainingConfirm::
	.string "Make {STR_VAR_2} {STR_VAR_1}'s\n"
	.string "SIGNATURE MOVE for ¥5,000?$"

gText_SignatureTrainingDone::
	.string "{STR_VAR_2} became\n"
	.string "{STR_VAR_1}'s SIGNATURE MOVE!$"

gText_SignatureTrainingNotFinal::
	.string "{STR_VAR_1} isn't fully evolved yet.\n"
	.string "It'll get the chance when it is.$"

gText_SignatureTrainingHasSignature::
	.string "{STR_VAR_1} already has {STR_VAR_2}\n"
	.string "as its SIGNATURE MOVE.\p"
	.string "The MOVE RELEARNER can make it\n"
	.string "forget that one first.$"

gText_SignatureTrainingNoMastered::
	.string "{STR_VAR_1} hasn't mastered any of\n"
	.string "the moves it knows.\p"
	.string "A move used 100 times is mastered.$"
