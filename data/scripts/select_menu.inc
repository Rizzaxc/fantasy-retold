@ Fantasy Retold: the Select-button menu (design.md §2). Toggles the infinite Repel and the Exp. Share, and runs the
@ registered key item (what Select did in vanilla); with the PokéNav, it also calls Birch's Aide for his research trade.
@ Labels are buffered into STR_VAR_1/2 by SelectMenu_BufferLabels.
@ A toggle reopens the menu with its row still selected.
Common_EventScript_SelectMenu::
	lockall
	setvar VAR_0x8004, 0
Common_EventScript_SelectMenuShow::
	special SelectMenu_BufferLabels
	goto_if_set FLAG_SYS_POKENAV_GET, Common_EventScript_SelectMenuShowWithResearch
	dynmultichoice 29, 1, FALSE, 4, VAR_0x8004, DYN_MULTICHOICE_CB_NONE, gText_SelectMenuRow1, gText_SelectMenuRow2, gText_SelectMenuKeyItem, gText_SelectMenuCancel
	goto_if_eq VAR_RESULT, 3, Common_EventScript_SelectMenuClose
	goto Common_EventScript_SelectMenuPicked

Common_EventScript_SelectMenuShowWithResearch::
	dynmultichoice 29, 1, FALSE, 5, VAR_0x8004, DYN_MULTICHOICE_CB_NONE, gText_SelectMenuRow1, gText_SelectMenuRow2, gText_SelectMenuKeyItem, gText_SelectMenuLabResearch, gText_SelectMenuCancel
Common_EventScript_SelectMenuPicked::
	switch VAR_RESULT
	case 0, Common_EventScript_SelectMenuToggleRepel
	case 1, Common_EventScript_SelectMenuToggleExpShare
	case 2, Common_EventScript_SelectMenuKeyItem
	case 3, Common_EventScript_LabResearchCall
	goto Common_EventScript_SelectMenuClose

Common_EventScript_SelectMenuToggleRepel::
	special SelectMenu_ToggleRepel
	setvar VAR_0x8004, 0
	goto Common_EventScript_SelectMenuShow

Common_EventScript_SelectMenuToggleExpShare::
	special SelectMenu_ToggleExpShare
	setvar VAR_0x8004, 1
	goto Common_EventScript_SelectMenuShow

Common_EventScript_SelectMenuKeyItem::
	special SelectMenu_QueueRegisteredItem
	goto Common_EventScript_SelectMenuClose

Common_EventScript_SelectMenuClose::
	releaseall
	end

gText_SelectMenuRow1::
	.string "{STR_VAR_1}$"

gText_SelectMenuRow2::
	.string "{STR_VAR_2}$"

gText_SelectMenuKeyItem::
	.string "KEY ITEM$"

gText_SelectMenuLabResearch::
	.string "LAB RESEARCH$"

gText_SelectMenuCancel::
	.string "CANCEL$"
