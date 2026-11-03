Route114_LanettesHouse_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, Route114_LanettesHouse_OnTransition
	.byte 0

Route114_LanettesHouse_OnTransition:
	setflag FLAG_LANDMARK_LANETTES_HOUSE
	end

Route114_LanettesHouse_EventScript_Lanette::
	lock
	faceplayer
	goto_if_unset FLAG_RECEIVED_DOLL_LANETTE, Route114_LanettesHouse_EventScript_GiveDoll
Route114_LanettesHouse_EventScript_ParadoxLab::
	goto_if_unset FLAG_BADGE05_GET, Route114_LanettesHouse_EventScript_OfferAdvice
	goto_if_unset FLAG_RECEIVED_PARADOX_GIFT, Route114_LanettesHouse_EventScript_ParadoxGift
	goto Route114_LanettesHouse_EventScript_ParadoxMenu

Route114_LanettesHouse_EventScript_GiveDoll::
	setflag FLAG_SYS_PC_LANETTE
	msgbox Route114_LanettesHouse_Text_EverythingClutteredKeepThis, MSGBOX_DEFAULT
	givedecoration DECOR_LOTAD_DOLL
	goto_if_eq VAR_RESULT, FALSE, Route114_LanettesHouse_EventScript_NoRoomForDoll
	setflag FLAG_RECEIVED_DOLL_LANETTE
	goto_if_set FLAG_BADGE05_GET, Route114_LanettesHouse_EventScript_ParadoxLab
	release
	end

Route114_LanettesHouse_EventScript_NoRoomForDoll::
	goto_if_unset FLAG_BADGE05_GET, Common_EventScript_ShowNoRoomForDecor
	msgbox gText_NoRoomLeftForAnother, MSGBOX_DEFAULT
	goto Route114_LanettesHouse_EventScript_ParadoxLab

Route114_LanettesHouse_EventScript_OfferAdvice::
	msgbox Route114_LanettesHouse_Text_OrganizeYourBoxes, MSGBOX_DEFAULT
	release
	end

@ Fantasy Retold: Lanette's Paradox Lab (design.md §2). After badge 5 she gives one random Lv10 paradox (once per
@ save), then converts the canon source species into their paradoxes (ParadoxLab_* in src/paradox_lab.c).
Route114_LanettesHouse_EventScript_ParadoxGift::
	msgbox Route114_LanettesHouse_Text_ParadoxIntro, MSGBOX_DEFAULT
	specialvar VAR_TEMP_TRANSFERRED_SPECIES, ParadoxLab_RollGift
	givemon VAR_TEMP_TRANSFERRED_SPECIES, PARADOX_LAB_GIFT_LEVEL
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PARTY, Route114_LanettesHouse_EventScript_ParadoxGiftParty
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PC, Route114_LanettesHouse_EventScript_ParadoxGiftPC
	msgbox Route114_LanettesHouse_Text_ParadoxNoRoom, MSGBOX_DEFAULT
	release
	end

Route114_LanettesHouse_EventScript_ParadoxGiftParty::
	call Route114_LanettesHouse_EventScript_ParadoxGiftFanfare
	msgbox gText_NicknameThisPokemon, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Route114_LanettesHouse_EventScript_ParadoxGiftDone
	call Common_EventScript_GetGiftMonPartySlot
	call Common_EventScript_NameReceivedPartyMon
	lock
	faceplayer
	goto Route114_LanettesHouse_EventScript_ParadoxGiftDone

Route114_LanettesHouse_EventScript_ParadoxGiftPC::
	call Route114_LanettesHouse_EventScript_ParadoxGiftFanfare
	msgbox gText_NicknameThisPokemon, MSGBOX_YESNO
	call_if_eq VAR_RESULT, YES, Common_EventScript_NameReceivedBoxMon
	call Common_EventScript_TransferredToPC
	goto Route114_LanettesHouse_EventScript_ParadoxGiftDone

Route114_LanettesHouse_EventScript_ParadoxGiftFanfare::
	setflag FLAG_RECEIVED_PARADOX_GIFT
	playfanfare MUS_OBTAIN_ITEM
	message Route114_LanettesHouse_Text_ReceivedParadox
	waitmessage
	waitfanfare
	copyvar VAR_0x8006, VAR_TEMP_TRANSFERRED_SPECIES
	call Route114_LanettesHouse_EventScript_TryGiveBoosterEnergy
	bufferspeciesname STR_VAR_1, VAR_TEMP_TRANSFERRED_SPECIES
	return

@ Every Quark Drive Pokémon she hands over (gift or conversion) comes with a Booster Energy (species in VAR_0x8006).
Route114_LanettesHouse_EventScript_TryGiveBoosterEnergy::
	specialvar VAR_RESULT, ParadoxLab_IsQuarkDrive
	goto_if_eq VAR_RESULT, FALSE, Route114_LanettesHouse_EventScript_NoBoosterEnergy
	msgbox Route114_LanettesHouse_Text_TakeBoosterEnergy, MSGBOX_DEFAULT
	giveitem ITEM_BOOSTER_ENERGY
Route114_LanettesHouse_EventScript_NoBoosterEnergy::
	return

Route114_LanettesHouse_EventScript_ParadoxGiftDone::
	msgbox Route114_LanettesHouse_Text_ExplainConversion, MSGBOX_DEFAULT
	release
	end

Route114_LanettesHouse_EventScript_ParadoxMenu::
	message Route114_LanettesHouse_Text_ParadoxWhatToDo
	waitmessage
	dynmultichoice 19, 6, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, Route114_LanettesHouse_Text_ChoiceConvert, Route114_LanettesHouse_Text_ChoiceAdvice, gText_EggShopChoiceCancel
	goto_if_eq VAR_RESULT, 0, Route114_LanettesHouse_EventScript_ParadoxConvert
	goto_if_eq VAR_RESULT, 1, Route114_LanettesHouse_EventScript_OfferAdvice
	goto Route114_LanettesHouse_EventScript_ParadoxBye

Route114_LanettesHouse_EventScript_ParadoxConvert::
	msgbox Route114_LanettesHouse_Text_ParadoxWhichMon, MSGBOX_DEFAULT
	special ChoosePartyMon
	lock
	faceplayer
	goto_if_ge VAR_0x8004, PARTY_SIZE, Route114_LanettesHouse_EventScript_ParadoxMenu
	specialvar VAR_RESULT, ParadoxLab_CheckMon
	goto_if_eq VAR_RESULT, PARADOX_LAB_MON_EGG, Route114_LanettesHouse_EventScript_ParadoxEgg
	goto_if_eq VAR_RESULT, PARADOX_LAB_MON_NONE, Route114_LanettesHouse_EventScript_ParadoxNoCounterpart
	goto_if_eq VAR_RESULT, PARADOX_LAB_MON_PARADOX, Route114_LanettesHouse_EventScript_ParadoxAlready
	goto_if_eq VAR_RESULT, PARADOX_LAB_MON_TWO, Route114_LanettesHouse_EventScript_ParadoxPickEra
	goto Route114_LanettesHouse_EventScript_ParadoxConfirm

Route114_LanettesHouse_EventScript_ParadoxPickEra::
	message Route114_LanettesHouse_Text_ParadoxAncientOrFuture
	waitmessage
	dynmultichoice 19, 6, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, Route114_LanettesHouse_Text_ChoiceAncient, Route114_LanettesHouse_Text_ChoiceFuture, gText_EggShopChoiceCancel
	goto_if_ge VAR_RESULT, 2, Route114_LanettesHouse_EventScript_ParadoxConvert
	copyvar VAR_0x8005, VAR_RESULT
Route114_LanettesHouse_EventScript_ParadoxConfirm::
	special ParadoxLab_BufferTarget
	msgbox Route114_LanettesHouse_Text_ParadoxConfirm, MSGBOX_YESNO
	goto_if_ne VAR_RESULT, YES, Route114_LanettesHouse_EventScript_ParadoxConvert
	msgbox Route114_LanettesHouse_Text_ParadoxLeaveItToMe, MSGBOX_DEFAULT
	closemessage
	fadescreen FADE_TO_BLACK
	playse SE_PC_LOGIN
	waitse
	specialvar VAR_RESULT, ParadoxLab_Convert
	fadescreen FADE_FROM_BLACK
	goto_if_eq VAR_RESULT, FALSE, Route114_LanettesHouse_EventScript_ParadoxBye
	playfanfare MUS_EVOLVED
	message Route114_LanettesHouse_Text_ParadoxTurnedInto
	waitmessage
	waitfanfare
	call Route114_LanettesHouse_EventScript_TryGiveBoosterEnergy
	msgbox Route114_LanettesHouse_Text_ParadoxDone, MSGBOX_DEFAULT
	release
	end

Route114_LanettesHouse_EventScript_ParadoxEgg::
	msgbox Route114_LanettesHouse_Text_ParadoxEgg, MSGBOX_DEFAULT
	goto Route114_LanettesHouse_EventScript_ParadoxConvert

Route114_LanettesHouse_EventScript_ParadoxNoCounterpart::
	msgbox Route114_LanettesHouse_Text_ParadoxNoCounterpart, MSGBOX_DEFAULT
	goto Route114_LanettesHouse_EventScript_ParadoxConvert

Route114_LanettesHouse_EventScript_ParadoxAlready::
	msgbox Route114_LanettesHouse_Text_ParadoxAlready, MSGBOX_DEFAULT
	goto Route114_LanettesHouse_EventScript_ParadoxConvert

Route114_LanettesHouse_EventScript_ParadoxBye::
	msgbox Route114_LanettesHouse_Text_ParadoxBye, MSGBOX_DEFAULT
	release
	end

Route114_LanettesHouse_EventScript_Notebook::
	lockall
	msgbox Route114_LanettesHouse_Text_ResearchNotesPage1, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, YES, Route114_LanettesHouse_EventScript_NotebookPage2
	msgbox Route114_LanettesHouse_Text_ClosedTheNotebook, MSGBOX_DEFAULT
	releaseall
	end

Route114_LanettesHouse_EventScript_NotebookPage2::
	msgbox Route114_LanettesHouse_Text_ResearchNotesPage2, MSGBOX_YESNO
	call_if_eq VAR_RESULT, YES, Route114_LanettesHouse_EventScript_NotebookPage3
	releaseall
	end

Route114_LanettesHouse_EventScript_NotebookPage3::
	msgbox Route114_LanettesHouse_Text_ResearchNotesPage3, MSGBOX_DEFAULT
	return

Route114_LanettesHouse_EventScript_PC::
	msgbox Route114_LanettesHouse_Text_EmailFromBill, MSGBOX_SIGN
	end

Route114_LanettesHouse_Text_EverythingClutteredKeepThis:
	.string "LANETTE: Oh! {PLAYER}{KUN}!\p"
	.string "I'm sorry everything is so cluttered…\n"
	.string "When I get engrossed in research,\l"
	.string "things end up this way…\p"
	.string "This is embarrassing… Please keep\n"
	.string "this a secret in exchange for this.$"

Route114_LanettesHouse_Text_OrganizeYourBoxes:
	.string "May I offer advice about my POKéMON\n"
	.string "Storage System?\p"
	.string "You should organize your BOXES so you\n"
	.string "can tell which POKéMON are in them.$"

Route114_LanettesHouse_Text_ParadoxIntro:
	.string "LANETTE: {PLAYER}{KUN}, look at this!\p"
	.string "My Storage System started picking\n"
	.string "up strange data. POKéMON data that\l"
	.string "shouldn't exist… yet. Or anymore.\p"
	.string "Some look ancient, some look like\n"
	.string "machines from the far future.\l"
	.string "I call them PARADOX POKéMON.\p"
	.string "I managed to rebuild one, but it\n"
	.string "needs a TRAINER to grow strong.\l"
	.string "Here, please take it!$"

Route114_LanettesHouse_Text_ReceivedParadox:
	.string "{PLAYER} received {STR_VAR_1}!$"

Route114_LanettesHouse_Text_TakeBoosterEnergy:
	.string "Its future circuits run on this.\n"
	.string "Take it too!$"

Route114_LanettesHouse_Text_ParadoxNoRoom:
	.string "Oh… Your party and BOXES are all\n"
	.string "full. Come back once you've made\l"
	.string "some room!$"

Route114_LanettesHouse_Text_ExplainConversion:
	.string "It's still young, so be patient\n"
	.string "with it. It only grows in battle.\p"
	.string "And there's more! Each PARADOX\n"
	.string "POKéMON matches a POKéMON from\l"
	.string "today.\p"
	.string "Bring me one of those, and I can\n"
	.string "rewrite its data into its PARADOX.\p"
	.string "It keeps its level, nature and\n"
	.string "training, but forgets every move.\l"
	.string "And there's no going back!$"

Route114_LanettesHouse_Text_ParadoxWhatToDo:
	.string "LANETTE: Hi, {PLAYER}{KUN}!\n"
	.string "What can I do for you?$"

Route114_LanettesHouse_Text_ChoiceConvert:
	.string "CONVERT$"

Route114_LanettesHouse_Text_ChoiceAdvice:
	.string "ADVICE$"

Route114_LanettesHouse_Text_ParadoxWhichMon:
	.string "Which POKéMON should I convert?$"

Route114_LanettesHouse_Text_ParadoxEgg:
	.string "An EGG? There's no data to rewrite\n"
	.string "until it hatches!$"

Route114_LanettesHouse_Text_ParadoxNoCounterpart:
	.string "Hmm… I haven't found any PARADOX\n"
	.string "data that matches {STR_VAR_1}.$"

Route114_LanettesHouse_Text_ParadoxAlready:
	.string "{STR_VAR_1} is already a PARADOX\n"
	.string "POKéMON!$"

Route114_LanettesHouse_Text_ParadoxAncientOrFuture:
	.string "{STR_VAR_1} has two PARADOXES!\n"
	.string "Which one should it become?$"

Route114_LanettesHouse_Text_ChoiceAncient:
	.string "{STR_VAR_2}$"

Route114_LanettesHouse_Text_ChoiceFuture:
	.string "{STR_VAR_3}$"

Route114_LanettesHouse_Text_ParadoxConfirm:
	.string "{STR_VAR_1} will become\n"
	.string "{STR_VAR_2}.\p"
	.string "It will forget all its moves, its\n"
	.string "SIGNATURE MOVE too, and it can't\l"
	.string "be undone. Is that okay?$"

Route114_LanettesHouse_Text_ParadoxLeaveItToMe:
	.string "Leave it to me!$"

Route114_LanettesHouse_Text_ParadoxTurnedInto:
	.string "{STR_VAR_1} turned into\n"
	.string "{STR_VAR_2}!$"

Route114_LanettesHouse_Text_ParadoxDone:
	.string "It worked! Take good care of it,\n"
	.string "{PLAYER}{KUN}!$"

Route114_LanettesHouse_Text_ParadoxBye:
	.string "Come back anytime!$"

Route114_LanettesHouse_Text_ResearchNotesPage1:
	.string "It's LANETTE's research notes.\n"
	.string "There's information about BOXES.\p"
	.string "Design BOXES to hold 30 POKéMON each.\p"
	.string "Each TRAINER should be able to store\n"
	.string "420 POKéMON on the PC system.\p"
	.string "Keep reading?$"

Route114_LanettesHouse_Text_ResearchNotesPage2:
	.string "A marking system should be added to\n"
	.string "make POKéMON easier to organize.\p"
	.string "The name and wallpaper design of each\n"
	.string "BOX will be made changeable to please\l"
	.string "the stored POKéMON.\p"
	.string "Keep reading?$"

Route114_LanettesHouse_Text_ResearchNotesPage3:
	.string "When storing a POKéMON, it should be\n"
	.string "sent to the BOX inspected last.\p"
	.string "If that BOX is full, the received\n"
	.string "POKéMON is stored in the next BOX.\p"
	.string "In other words, when a BOX is examined,\n"
	.string "it is automatically selected as the BOX\l"
	.string "to which POKéMON are sent.$"

Route114_LanettesHouse_Text_ClosedTheNotebook:
	.string "{PLAYER} closed the notebook.$"

Route114_LanettesHouse_Text_EmailFromBill:
	.string "There's an e-mail from someone on\n"
	.string "the PC.\p"
	.string "“… … … … … … …\p"
	.string "“Your Storage System offers more\n"
	.string "convenience than mine.\p"
	.string "“It has a lot of user-friendly features\n"
	.string "that make it fun and useful, too.\p"
	.string "“It makes me proud that I played\n"
	.string "a part in its development.\p"
	.string "“Here's hoping that you'll continue\n"
	.string "research in Storage Systems.\p"
	.string "“From BILL\n"
	.string "… … … … … … … …”$"

