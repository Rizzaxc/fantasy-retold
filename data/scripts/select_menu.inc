@ Fantasy Retold: the Select-button menu (design.md §10). Toggles the infinite Repel and the Exp. Share, and runs the
@ registered key item (what Select did in vanilla). Labels are buffered into STR_VAR_1/2 by SelectMenu_BufferLabels.
@ A toggle reopens the menu with its row still selected.
Common_EventScript_SelectMenu::
	lockall
	special SelectMenu_BufferLabels
	dynmultichoice 29, 1, FALSE, 4, 0, DYN_MULTICHOICE_CB_NONE, gText_SelectMenuRow1, gText_SelectMenuRow2, gText_SelectMenuKeyItem, gText_SelectMenuCancel
	goto Common_EventScript_SelectMenuPicked

Common_EventScript_SelectMenuAtRepel::
	special SelectMenu_BufferLabels
	dynmultichoice 29, 1, FALSE, 4, 0, DYN_MULTICHOICE_CB_NONE, gText_SelectMenuRow1, gText_SelectMenuRow2, gText_SelectMenuKeyItem, gText_SelectMenuCancel
	goto Common_EventScript_SelectMenuPicked

Common_EventScript_SelectMenuAtExpShare::
	special SelectMenu_BufferLabels
	dynmultichoice 29, 1, FALSE, 4, 1, DYN_MULTICHOICE_CB_NONE, gText_SelectMenuRow1, gText_SelectMenuRow2, gText_SelectMenuKeyItem, gText_SelectMenuCancel
	goto Common_EventScript_SelectMenuPicked

Common_EventScript_SelectMenuPicked::
	switch VAR_RESULT
	case 0, Common_EventScript_SelectMenuToggleRepel
	case 1, Common_EventScript_SelectMenuToggleExpShare
	case 2, Common_EventScript_SelectMenuKeyItem
	goto Common_EventScript_SelectMenuClose

Common_EventScript_SelectMenuToggleRepel::
	special SelectMenu_ToggleRepel
	goto Common_EventScript_SelectMenuAtRepel

Common_EventScript_SelectMenuToggleExpShare::
	special SelectMenu_ToggleExpShare
	goto Common_EventScript_SelectMenuAtExpShare

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

gText_SelectMenuCancel::
	.string "CANCEL$"
