MauvilleCity_GameCorner_MapScripts::
	.byte 0

	@ Game Corner prices
	.set DOLL_COINS, 1000

	.set COINS_PRICE_50,  1000
	.set COINS_PRICE_500, 10000

MauvilleCity_GameCorner_EventScript_CoinsClerk::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_ThisIsMauvilleGameCorner, MSGBOX_DEFAULT
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NeedCoinCase
	message MauvilleCity_GameCorner_Text_WereYouLookingForCoins
	waitmessage
	showmoneybox 0, 0
	showcoinsbox 0, 5
	goto MauvilleCity_GameCorner_EventScript_ChooseCoinsDefault50

MauvilleCity_GameCorner_EventScript_ChooseCoinsDefault50::
	multichoicedefault 15, 0, MULTI_GAME_CORNER_COINS, 0, FALSE
	switch VAR_RESULT
	case 0, MauvilleCity_GameCorner_EventScript_Buy50Coins
	case 1, MauvilleCity_GameCorner_EventScript_Buy500Coins
	goto MauvilleCity_GameCorner_EventScript_CancelBuyCoins
	end

@ Unused
MauvilleCity_GameCorner_EventScript_ChooseCoinsDefault500::
	multichoicedefault 15, 0, MULTI_GAME_CORNER_COINS, 1, FALSE
	switch VAR_RESULT
	case 0, MauvilleCity_GameCorner_EventScript_Buy50Coins
	case 1, MauvilleCity_GameCorner_EventScript_Buy500Coins
	goto MauvilleCity_GameCorner_EventScript_CancelBuyCoins
	end

MauvilleCity_GameCorner_EventScript_Buy50Coins::
	checkcoins VAR_TEMP_1
	goto_if_ge VAR_TEMP_1, (MAX_COINS + 1 - 50), MauvilleCity_GameCorner_EventScript_NoRoomForCoins
	checkmoney COINS_PRICE_50
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NotEnoughMoney
	addcoins 50
	removemoney COINS_PRICE_50
	updatemoneybox
	updatecoinsbox
	playse SE_SHOP
	msgbox MauvilleCity_GameCorner_Text_ThankYouHereAreYourCoins, MSGBOX_DEFAULT
	hidemoneybox
	hidecoinsbox
	release
	end

MauvilleCity_GameCorner_EventScript_Buy500Coins::
	checkcoins VAR_TEMP_1
	goto_if_ge VAR_TEMP_1, (MAX_COINS + 1 - 500), MauvilleCity_GameCorner_EventScript_NoRoomForCoins
	checkmoney COINS_PRICE_500
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NotEnoughMoney
	addcoins 500
	removemoney COINS_PRICE_500
	updatemoneybox
	updatecoinsbox
	playse SE_SHOP
	msgbox MauvilleCity_GameCorner_Text_ThankYouHereAreYourCoins, MSGBOX_DEFAULT
	hidemoneybox
	hidecoinsbox
	release
	end

MauvilleCity_GameCorner_EventScript_NeedCoinCase::
	msgbox MauvilleCity_GameCorner_Text_NeedCoinCaseForCoins, MSGBOX_DEFAULT
	release
	end

MauvilleCity_GameCorner_EventScript_NotEnoughMoney::
	msgbox MauvilleCity_GameCorner_Text_DontHaveEnoughMoney, MSGBOX_DEFAULT
	hidemoneybox
	hidecoinsbox
	release
	end

MauvilleCity_GameCorner_EventScript_CancelBuyCoins::
	msgbox MauvilleCity_GameCorner_Text_DontNeedCoinsThen, MSGBOX_DEFAULT
	hidemoneybox
	hidecoinsbox
	release
	end

MauvilleCity_GameCorner_EventScript_NoRoomForCoins::
	msgbox MauvilleCity_GameCorner_Text_CoinCaseIsFull, MSGBOX_DEFAULT
	hidemoneybox
	hidecoinsbox
	release
	end

MauvilleCity_GameCorner_EventScript_PrizeCornerDolls::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_ExchangeCoinsForPrizes, MSGBOX_DEFAULT
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, TRUE, MauvilleCity_GameCorner_EventScript_ChooseDollPrizeMessage
	release
	end

MauvilleCity_GameCorner_EventScript_ChooseDollPrizeMessage::
	special GameCorner_Restock
	message MauvilleCity_GameCorner_Text_WhichPrize
	waitmessage
	setvar VAR_TEMP_1, 0
	showcoinsbox 0, 0
	goto MauvilleCity_GameCorner_EventScript_ChooseDollPrize

MauvilleCity_GameCorner_EventScript_ReturnToChooseDollPrize::
	message MauvilleCity_GameCorner_Text_WhichPrize
	waitmessage
	goto MauvilleCity_GameCorner_EventScript_ChooseDollPrize

MauvilleCity_GameCorner_EventScript_ChooseDollPrize::
	multichoice 12, 0, MULTI_GAME_CORNER_DOLLS, FALSE
	switch VAR_RESULT
	case 0, MauvilleCity_GameCorner_EventScript_TreeckoDoll
	case 1, MauvilleCity_GameCorner_EventScript_TorchicDoll
	case 2, MauvilleCity_GameCorner_EventScript_MudkipDoll
	case 3, MauvilleCity_GameCorner_EventScript_MegaStonePrize
	case 4, MauvilleCity_GameCorner_EventScript_ZCrystalPrize
	case 5, MauvilleCity_GameCorner_EventScript_CancelDollSelect
	goto MauvilleCity_GameCorner_EventScript_CancelDollSelect
	end

MauvilleCity_GameCorner_EventScript_TreeckoDoll::
	setvar VAR_TEMP_1, 1
	bufferdecorationname STR_VAR_1, DECOR_TREECKO_DOLL
	goto MauvilleCity_GameCorner_EventScript_ConfirmDollPrize

MauvilleCity_GameCorner_EventScript_TorchicDoll::
	setvar VAR_TEMP_1, 2
	bufferdecorationname STR_VAR_1, DECOR_TORCHIC_DOLL
	goto MauvilleCity_GameCorner_EventScript_ConfirmDollPrize

MauvilleCity_GameCorner_EventScript_MudkipDoll::
	setvar VAR_TEMP_1, 3
	bufferdecorationname STR_VAR_1, DECOR_MUDKIP_DOLL
	goto MauvilleCity_GameCorner_EventScript_ConfirmDollPrize

MauvilleCity_GameCorner_EventScript_ConfirmDollPrize::
	msgbox MauvilleCity_GameCorner_Text_SoYourChoiceIsX, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, MauvilleCity_GameCorner_EventScript_CancelDollSelect
	switch VAR_TEMP_1
	case 1, MauvilleCity_GameCorner_EventScript_BuyTreeckoDoll
	case 2, MauvilleCity_GameCorner_EventScript_BuyTorchicDoll
	case 3, MauvilleCity_GameCorner_EventScript_BuyMudkipDoll
	end

MauvilleCity_GameCorner_EventScript_BuyTreeckoDoll::
	checkcoins VAR_TEMP_2
	goto_if_lt VAR_TEMP_2, DOLL_COINS, MauvilleCity_GameCorner_EventScript_NotEnoughCoinsForDoll
	bufferdecorationname STR_VAR_2, DECOR_TREECKO_DOLL
	checkdecorspace DECOR_TREECKO_DOLL
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoRoomForDoll
	removecoins DOLL_COINS
	adddecoration DECOR_TREECKO_DOLL
	updatecoinsbox
	playse SE_SHOP
	msgbox MauvilleCity_GameCorner_Text_SendToYourHomePC, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ReturnToChooseDollPrize
	end

MauvilleCity_GameCorner_EventScript_BuyTorchicDoll::
	checkcoins VAR_TEMP_2
	goto_if_lt VAR_TEMP_2, DOLL_COINS, MauvilleCity_GameCorner_EventScript_NotEnoughCoinsForDoll
	bufferdecorationname STR_VAR_2, DECOR_TORCHIC_DOLL
	checkdecorspace DECOR_TORCHIC_DOLL
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoRoomForDoll
	removecoins DOLL_COINS
	adddecoration DECOR_TORCHIC_DOLL
	updatecoinsbox
	playse SE_SHOP
	msgbox MauvilleCity_GameCorner_Text_SendToYourHomePC, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ReturnToChooseDollPrize
	end

MauvilleCity_GameCorner_EventScript_BuyMudkipDoll::
	checkcoins VAR_TEMP_2
	goto_if_lt VAR_TEMP_2, DOLL_COINS, MauvilleCity_GameCorner_EventScript_NotEnoughCoinsForDoll
	bufferdecorationname STR_VAR_2, DECOR_MUDKIP_DOLL
	checkdecorspace DECOR_MUDKIP_DOLL
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoRoomForDoll
	removecoins DOLL_COINS
	adddecoration DECOR_MUDKIP_DOLL
	updatecoinsbox
	playse SE_SHOP
	msgbox MauvilleCity_GameCorner_Text_SendToYourHomePC, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ReturnToChooseDollPrize
	end

MauvilleCity_GameCorner_EventScript_NotEnoughCoinsForDoll::
	msgbox MauvilleCity_GameCorner_Text_NotEnoughCoins, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ReturnToChooseDollPrize
	end

MauvilleCity_GameCorner_EventScript_NoRoomForDoll::
	call Common_EventScript_NoRoomForDecor
	goto MauvilleCity_GameCorner_EventScript_ReturnToChooseDollPrize
	end

@ Fantasy Retold: three Mega Stones and three Z-Crystals the player doesn't have, restocked after every badge,
@ rival battle and Elite Four clear (design.md §2). VAR_0x8004 = category, VAR_0x8005 = row, VAR_0x8006 = price.
MauvilleCity_GameCorner_EventScript_MegaStonePrize::
	setvar VAR_0x8004, GAME_CORNER_STOCK_MEGA_STONE
	setvar VAR_0x8006, GAME_CORNER_MEGA_STONE_COINS
	goto MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize
	end

MauvilleCity_GameCorner_EventScript_ZCrystalPrize::
	setvar VAR_0x8004, GAME_CORNER_STOCK_Z_CRYSTAL
	setvar VAR_0x8006, GAME_CORNER_Z_CRYSTAL_COINS
	goto MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize
	end

MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize::
	special GameCorner_BufferStock
	message MauvilleCity_GameCorner_Text_WhichPrize
	waitmessage
	dynmultichoice 29, 1, FALSE, 4, 0, DYN_MULTICHOICE_CB_NONE, gText_EggShopRow1, gText_EggShopRow2, gText_EggShopRow3, gText_EggShopChoiceCancel
	goto_if_ge VAR_RESULT, GAME_CORNER_STOCK_SLOTS, MauvilleCity_GameCorner_EventScript_ReturnToChooseDollPrize
	copyvar VAR_0x8005, VAR_RESULT
	specialvar VAR_0x8007, GameCorner_BufferOffer
	goto_if_eq VAR_0x8007, ITEM_NONE, MauvilleCity_GameCorner_EventScript_GimmickPrizeSoldOut
	msgbox MauvilleCity_GameCorner_Text_SoYourChoiceIsX, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize
	checkcoins VAR_TEMP_2
	goto_if_lt VAR_TEMP_2, VAR_0x8006, MauvilleCity_GameCorner_EventScript_NotEnoughCoinsForGimmickPrize
	checkitemspace VAR_0x8007
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoRoomForGimmickPrize
	removecoins VAR_0x8006
	additem VAR_0x8007
	special GameCorner_SetSold
	updatecoinsbox
	playse SE_SHOP
	msgbox MauvilleCity_GameCorner_Text_HereYouGo, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize
	end

MauvilleCity_GameCorner_EventScript_GimmickPrizeSoldOut::
	msgbox MauvilleCity_GameCorner_Text_GimmickPrizeSoldOut, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize
	end

MauvilleCity_GameCorner_EventScript_NotEnoughCoinsForGimmickPrize::
	msgbox MauvilleCity_GameCorner_Text_NotEnoughCoins, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize
	end

MauvilleCity_GameCorner_EventScript_NoRoomForGimmickPrize::
	call Common_EventScript_BagIsFull
	goto MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize
	end

MauvilleCity_GameCorner_EventScript_CancelDollSelect::
	msgbox MauvilleCity_GameCorner_Text_OhIsThatSo, MSGBOX_DEFAULT
	hidecoinsbox
	release
	end

MauvilleCity_GameCorner_EventScript_PrizeCornerTMs::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_ExchangeCoinsForPrizes, MSGBOX_DEFAULT
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, TRUE, MauvilleCity_GameCorner_EventScript_ChooseTMPrizeMessage
	release
	end

MauvilleCity_GameCorner_EventScript_ChooseTMPrizeMessage::
	special GameCornerPrize_Restock
	showcoinsbox 0, 0
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize

@ Fantasy Retold: five prize rows (species item / held item / stat item / Powerhouse Egg) replace the TMs,
@ restocked daily and after every badge, rival battle and Elite Four clear unless the player locks them
@ (design.md §2, Game Corner Prizes).
@ VAR_0x8005 = row, VAR_0x8006 = price, VAR_0x8007 = item (ITEM_NONE for an Egg).
MauvilleCity_GameCorner_EventScript_ChoosePrize::
	message MauvilleCity_GameCorner_Text_WhichPrize
	waitmessage
	setvar VAR_0x8005, 0
MauvilleCity_GameCorner_EventScript_PushPrizeRow::
	special GameCornerPrize_BufferRow
	dynmultipush gText_HyperTrainingStatRow, VAR_0x8005
	addvar VAR_0x8005, 1
	goto_if_lt VAR_0x8005, GAME_CORNER_PRIZE_ROWS, MauvilleCity_GameCorner_EventScript_PushPrizeRow
	call_if_unset FLAG_GAME_CORNER_PRIZES_LOCKED, MauvilleCity_GameCorner_EventScript_PushLockPrizesRow
	call_if_set FLAG_GAME_CORNER_PRIZES_LOCKED, MauvilleCity_GameCorner_EventScript_PushUnlockPrizesRow
	dynmultipush gText_EggShopChoiceCancel, GAME_CORNER_PRIZE_MENU_CANCEL
	dynmultistack 15, 1, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, GAME_CORNER_PRIZE_MENU_LOCK, MauvilleCity_GameCorner_EventScript_TogglePrizeLock
	goto_if_ge VAR_RESULT, GAME_CORNER_PRIZE_ROWS, MauvilleCity_GameCorner_EventScript_CancelTMSelect
	copyvar VAR_0x8005, VAR_RESULT
	specialvar VAR_0x8008, GameCornerPrize_BufferOffer
	goto_if_eq VAR_0x8008, GAME_CORNER_OFFER_SOLD, MauvilleCity_GameCorner_EventScript_PrizeSoldOut
	buffernumberstring STR_VAR_2, VAR_0x8006
	msgbox MauvilleCity_GameCorner_Text_PrizeForCoins, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, MauvilleCity_GameCorner_EventScript_ChoosePrize
	checkcoins VAR_TEMP_2
	goto_if_lt VAR_TEMP_2, VAR_0x8006, MauvilleCity_GameCorner_EventScript_NotEnoughCoinsForPrize
	goto_if_eq VAR_0x8008, GAME_CORNER_OFFER_EGG, MauvilleCity_GameCorner_EventScript_BuyEggPrize
	checkitemspace VAR_0x8007
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoRoomForPrize
	removecoins VAR_0x8006
	additem VAR_0x8007
	special GameCornerPrize_SetSold
	updatecoinsbox
	playse SE_SHOP
	msgbox MauvilleCity_GameCorner_Text_HereYouGo, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize
	end

MauvilleCity_GameCorner_EventScript_PushLockPrizesRow::
	dynmultipush MauvilleCity_GameCorner_Text_LockPrizesRow, GAME_CORNER_PRIZE_MENU_LOCK
	return

MauvilleCity_GameCorner_EventScript_PushUnlockPrizesRow::
	dynmultipush MauvilleCity_GameCorner_Text_UnlockPrizesRow, GAME_CORNER_PRIZE_MENU_LOCK
	return

@ Locked rows keep their stock (sold rows stay sold) until unlocked; a refresh that came due meanwhile happens on
@ the next visit after unlocking.
MauvilleCity_GameCorner_EventScript_TogglePrizeLock::
	goto_if_set FLAG_GAME_CORNER_PRIZES_LOCKED, MauvilleCity_GameCorner_EventScript_UnlockPrizes
	setflag FLAG_GAME_CORNER_PRIZES_LOCKED
	msgbox MauvilleCity_GameCorner_Text_PrizesLocked, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize
	end

MauvilleCity_GameCorner_EventScript_UnlockPrizes::
	clearflag FLAG_GAME_CORNER_PRIZES_LOCKED
	msgbox MauvilleCity_GameCorner_Text_PrizesUnlocked, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize
	end

MauvilleCity_GameCorner_EventScript_BuyEggPrize::
	specialvar VAR_0x8009, GameCornerPrize_GiveEgg
	goto_if_eq VAR_0x8009, MON_CANT_GIVE, MauvilleCity_GameCorner_EventScript_NoRoomForEggPrize
	removecoins VAR_0x8006
	special GameCornerPrize_SetSold
	updatecoinsbox
	playfanfare MUS_OBTAIN_ITEM
	msgbox MauvilleCity_GameCorner_Text_HereYouGo, MSGBOX_DEFAULT
	waitfanfare
	call_if_eq VAR_0x8009, MON_GIVEN_TO_PC, MauvilleCity_GameCorner_EventScript_EggSentToPC
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize
	end

MauvilleCity_GameCorner_EventScript_EggSentToPC::
	msgbox MauvilleCity_GameCorner_Text_EggSentToPC, MSGBOX_DEFAULT
	return

MauvilleCity_GameCorner_EventScript_PrizeSoldOut::
	msgbox MauvilleCity_GameCorner_Text_GimmickPrizeSoldOut, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize
	end

MauvilleCity_GameCorner_EventScript_NotEnoughCoinsForPrize::
	msgbox MauvilleCity_GameCorner_Text_NotEnoughCoins, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize
	end

MauvilleCity_GameCorner_EventScript_NoRoomForPrize::
	call Common_EventScript_BagIsFull
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize
	end

MauvilleCity_GameCorner_EventScript_NoRoomForEggPrize::
	msgbox MauvilleCity_GameCorner_Text_NoRoomForEgg, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_ChoosePrize
	end

MauvilleCity_GameCorner_EventScript_CancelTMSelect::
	msgbox MauvilleCity_GameCorner_Text_OhIsThatSo, MSGBOX_DEFAULT
	hidecoinsbox
	release
	end

MauvilleCity_GameCorner_EventScript_Woman2::
	msgbox MauvilleCity_GameCorner_Text_CoinsAreNeededToPlay, MSGBOX_NPC
	end

MauvilleCity_GameCorner_EventScript_Gentleman::
	msgbox MauvilleCity_GameCorner_Text_RouletteOnlyLuck, MSGBOX_NPC
	end

MauvilleCity_GameCorner_EventScript_Girl::
	lock
	faceplayer
	goto_if_set FLAG_RECEIVED_STARTER_DOLL, MauvilleCity_GameCorner_EventScript_ReceivedStarterDoll
	msgbox MauvilleCity_GameCorner_Text_GotTwoOfSameDollWantOne, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, MauvilleCity_GameCorner_EventScript_DeclineStarterDoll
	switch VAR_STARTER_MON
	case 0, MauvilleCity_GameCorner_EventScript_GiveTreeckoDoll
	case 1, MauvilleCity_GameCorner_EventScript_GiveTorchicDoll
	case 2, MauvilleCity_GameCorner_EventScript_GiveMudkipDoll
	end

MauvilleCity_GameCorner_EventScript_GiveTreeckoDoll::
	bufferdecorationname STR_VAR_2, DECOR_TREECKO_DOLL
	checkdecorspace DECOR_TREECKO_DOLL
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoRoomForStarterDoll
	msgbox MauvilleCity_GameCorner_Text_HereYouGo2, MSGBOX_DEFAULT
	givedecoration DECOR_TREECKO_DOLL
	setflag FLAG_RECEIVED_STARTER_DOLL
	goto MauvilleCity_GameCorner_EventScript_ReceivedStarterDoll
	end

MauvilleCity_GameCorner_EventScript_GiveTorchicDoll::
	bufferdecorationname STR_VAR_2, DECOR_TORCHIC_DOLL
	checkdecorspace DECOR_TORCHIC_DOLL
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoRoomForStarterDoll
	msgbox MauvilleCity_GameCorner_Text_HereYouGo2, MSGBOX_DEFAULT
	givedecoration DECOR_TORCHIC_DOLL
	setflag FLAG_RECEIVED_STARTER_DOLL
	goto MauvilleCity_GameCorner_EventScript_ReceivedStarterDoll
	end

MauvilleCity_GameCorner_EventScript_GiveMudkipDoll::
	bufferdecorationname STR_VAR_2, DECOR_MUDKIP_DOLL
	checkdecorspace DECOR_MUDKIP_DOLL
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoRoomForStarterDoll
	msgbox MauvilleCity_GameCorner_Text_HereYouGo2, MSGBOX_DEFAULT
	givedecoration DECOR_MUDKIP_DOLL
	setflag FLAG_RECEIVED_STARTER_DOLL
	goto MauvilleCity_GameCorner_EventScript_ReceivedStarterDoll
	end

MauvilleCity_GameCorner_EventScript_NoRoomForStarterDoll::
	call Common_EventScript_NoRoomForDecor
	msgbox MauvilleCity_GameCorner_Text_YouWantItButNotNow, MSGBOX_DEFAULT
	release
	end

MauvilleCity_GameCorner_EventScript_DeclineStarterDoll::
	msgbox MauvilleCity_GameCorner_Text_DontBeNegative, MSGBOX_DEFAULT
	release
	end

MauvilleCity_GameCorner_EventScript_ReceivedStarterDoll::
	msgbox MauvilleCity_GameCorner_Text_CantWinJackpot, MSGBOX_DEFAULT
	release
	end

MauvilleCity_GameCorner_EventScript_PokefanM::
	lock
	faceplayer
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, TRUE, MauvilleCity_GameCorner_EventScript_TryGive20Coins
	msgbox MauvilleCity_GameCorner_Text_NeedCoinCaseGoNextDoor, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_TryGive20Coins::
	goto_if_set FLAG_RECEIVED_20_COINS, MauvilleCity_GameCorner_EventScript_PokefanMNormal
	checkcoins VAR_TEMP_1
	goto_if_ge VAR_TEMP_1, 1, MauvilleCity_GameCorner_EventScript_PokefanMNormal @ Only give 20 coins if player has no coins
	setflag FLAG_RECEIVED_20_COINS
	addcoins 20
	msgbox MauvilleCity_GameCorner_Text_LuckOnlyLastSoLongTakeCoins, MSGBOX_DEFAULT
	playse SE_SHOP
	goto MauvilleCity_GameCorner_EventScript_PokefanMNormal
	end

MauvilleCity_GameCorner_EventScript_PokefanMNormal::
	msgbox MauvilleCity_GameCorner_Text_MauvilleSomethingForEveryone, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_OldMan::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_RouletteTablesDifferentRates, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_Cook::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_EasyToLoseTrackOfTime, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_Man::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_UpTo3CoinsCanBeUsed, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_NPCReturnToSlots::
	closemessage
	applymovement VAR_LAST_TALKED, Common_Movement_FaceOriginalDirection
	waitmovement 0
	release
	end

MauvilleCity_GameCorner_EventScript_Maniac::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_DifficultToStopOn7, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_Woman::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_HeresSomeSlotsInfo, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_SlotMachine0::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 0
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine1::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 1
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine2::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 2
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine3::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 3
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine4::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 4
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine5::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 5
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine6::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 6
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine7::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 7
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine8::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 8
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine9::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 9
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine10::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 10
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_SlotMachine11::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	setvar VAR_0x8004, 11
	specialvar VAR_RESULT, GetSlotMachineId
	playslotmachine VAR_RESULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_NoCoinCase::
	msgbox MauvilleCity_GameCorner_Text_CantPlayWithNoCoinCase, MSGBOX_DEFAULT
	releaseall
	end

MauvilleCity_GameCorner_Text_ThisIsMauvilleGameCorner:
	.string "This is MAUVILLE GAME CORNER.$"

MauvilleCity_GameCorner_Text_NeedCoinCaseForCoins:
	.string "Okay, you wanted some COINS for\n"
	.string "the games?\p"
	.string "But you don't have a COIN CASE for\n"
	.string "stowing the COINS.$"

MauvilleCity_GameCorner_Text_WereYouLookingForCoins:
	.string "Were you looking for COINS?\p"
	.string "It's ¥1000 for 50 COINS.\n"
	.string "Would you like some?$"

MauvilleCity_GameCorner_Text_ThankYouHereAreYourCoins:
	.string "Thank you very much!\n"
	.string "Here are your COINS!$"

MauvilleCity_GameCorner_Text_DontHaveEnoughMoney:
	.string "Um… You don't appear to have\n"
	.string "enough money…$"

MauvilleCity_GameCorner_Text_CoinCaseIsFull:
	.string "Oh?\n"
	.string "Your COIN CASE is full.$"

MauvilleCity_GameCorner_Text_DontNeedCoinsThen:
	.string "Oh… You don't need COINS, then?\n"
	.string "Good luck on your adventure!$"

MauvilleCity_GameCorner_Text_ExchangeCoinsForPrizes:
	.string "Welcome.\p"
	.string "You can exchange your COINS for\n"
	.string "prizes here.$"

MauvilleCity_GameCorner_Text_WhichPrize:
	.string "Which prize would you like?$"

MauvilleCity_GameCorner_Text_PrizeForCoins:
	.string "So your choice is {STR_VAR_1}?\n"
	.string "That's {STR_VAR_2} COINS.$"

MauvilleCity_GameCorner_Text_LockPrizesRow:
	.string "LOCK PRIZES$"

MauvilleCity_GameCorner_Text_UnlockPrizesRow:
	.string "UNLOCK PRIZES$"

MauvilleCity_GameCorner_Text_PrizesLocked:
	.string "Got it. I'll hold these prizes\n"
	.string "for you until you say otherwise.$"

MauvilleCity_GameCorner_Text_PrizesUnlocked:
	.string "All right. New prizes will come\n"
	.string "in as usual again.$"

MauvilleCity_GameCorner_Text_EggSentToPC:
	.string "Your party is full, so the EGG\n"
	.string "was sent to your PC.$"

MauvilleCity_GameCorner_Text_NoRoomForEgg:
	.string "Oh, there's no room for an EGG in\n"
	.string "your party or your PC.$"

MauvilleCity_GameCorner_Text_SendToYourHomePC:
	.string "Thank you!\n"
	.string "We'll send it to your PC at home.$"

MauvilleCity_GameCorner_Text_NotEnoughCoins:
	.string "You don't have enough COINS.$"

@ Unused
MauvilleCity_GameCorner_Text_NoRoomForPlacingDecor:
	.string "There isn't any room available for\n"
	.string "placing {STR_VAR_1}.$"

MauvilleCity_GameCorner_Text_OhIsThatSo:
	.string "Oh, is that so? \n"
	.string "You need to save some COINS before\l"
	.string "coming back here.$"

MauvilleCity_GameCorner_Text_SoYourChoiceIsX:
	.string "So your choice is {STR_VAR_1}?$"

MauvilleCity_GameCorner_Text_HereYouGo:
	.string "Here you go!$"

MauvilleCity_GameCorner_Text_GimmickPrizeSoldOut:
	.string "Sorry, that one's gone.\n"
	.string "New ones come in all the time!$"

@ Unused
MauvilleCity_GameCorner_Text_CantCarryAnyMore:
	.string "Oh, you can't carry any more than that.$"

MauvilleCity_GameCorner_Text_GotTwoOfSameDollWantOne:
	.string "I made a mistake and got two of\n"
	.string "the same DOLLS.\p"
	.string "Would you like one of them?$"

MauvilleCity_GameCorner_Text_HereYouGo2:
	.string "Here you go!$"

MauvilleCity_GameCorner_Text_YouWantItButNotNow:
	.string "Huh?\n"
	.string "You want it, but not right now?$"

MauvilleCity_GameCorner_Text_DontBeNegative:
	.string "Oh, don't be so negative!\n"
	.string "You can have this!$"

MauvilleCity_GameCorner_Text_CantWinJackpot:
	.string "There's a prize I want, but I can't win\n"
	.string "the jackpot.$"

MauvilleCity_GameCorner_Text_NeedCoinCaseGoNextDoor:
	.string "Hey, kid, if you want to play here,\n"
	.string "you need a COIN CASE.\p"
	.string "I think the young lady next door\n"
	.string "had one. Go see her!$"

MauvilleCity_GameCorner_Text_LuckOnlyLastSoLongTakeCoins:
	.string "My luck can only last so long.\n"
	.string "This is too much for me.\l"
	.string "Here, take some COINS!$"

MauvilleCity_GameCorner_Text_MauvilleSomethingForEveryone:
	.string "MAUVILLE has something for\n"
	.string "everyone.\p"
	.string "For me, it's the GAME CORNER.$"

MauvilleCity_GameCorner_Text_RouletteTablesDifferentRates:
	.string "The ROULETTE tables have different\n"
	.string "rates.\p"
	.string "Check your COINS if you're going to\n"
	.string "pick a table.$"

MauvilleCity_GameCorner_Text_EasyToLoseTrackOfTime:
	.string "It's easy to lose track of time in here. \n"
	.string "I should get back to work.$"

MauvilleCity_GameCorner_Text_CoinsAreNeededToPlay:
	.string "COINS are needed to play here\n"
	.string "in the GAME CORNER.$"

MauvilleCity_GameCorner_Text_RouletteOnlyLuck:
	.string "This ROULETTE thing…\n"
	.string "It's rather demanding.\p"
	.string "Win or lose, it's only by luck.$"

MauvilleCity_GameCorner_Text_UpTo3CoinsCanBeUsed:
	.string "Up to three COINS can be used to play\n"
	.string "the SLOTS.$"

MauvilleCity_GameCorner_Text_DifficultToStopOn7:
	.string "It's very difficult to make it stop\n"
	.string "right on “7.”\p"
	.string "If it stops on “7” during the REEL TIME\n"
	.string "bonus game, you'll receive extra COINS.$"

MauvilleCity_GameCorner_Text_HeresSomeSlotsInfo:
	.string "Here's some information for you\n"
	.string "about the SLOTS.\p"
	.string "The more lightning bolts you stock,\n"
	.string "the more REEL TIME chances you get.\p"
	.string "In a game with the maximum five\n"
	.string "REEL TIME chances…\p"
	.string "It is possible to receive four regular\n"
	.string "bonuses, then a big bonus.\p"
	.string "That would total 660 COINS, but it's\n"
	.string "very difficult to get.$"

MauvilleCity_GameCorner_Text_CantPlayWithNoCoinCase:
	.string "You can't play if you don't have\n"
	.string "a COIN CASE.$"


@ Fantasy Retold: Game Corner minigames (design.md §2, Game Corner Minigames).
@ Machines and NPCs from heyopc/pokeemerald-gamecorner-expansion's room.

MauvilleCity_GameCorner_EventScript_Blackjack::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	checkcoins VAR_TEMP_2
	goto_if_lt VAR_TEMP_2, GC_CARD_STAKE_MIN, MauvilleCity_GameCorner_EventScript_CardNotEnoughCoins
	msgbox MauvilleCity_GameCorner_Text_Blackjack, MSGBOX_YESNO
	closemessage
	goto_if_eq VAR_RESULT, NO, MauvilleCity_GameCorner_EventScript_MinigameCancel
	playbgm MUS_GAME_CORNER, FALSE
	special StartBlackJack
	waitstate
	savebgm MUS_DUMMY
	fadedefaultbgm
	releaseall
	end

MauvilleCity_GameCorner_EventScript_VideoPoker::
	lockall
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	checkcoins VAR_TEMP_2
	goto_if_lt VAR_TEMP_2, GC_CARD_STAKE_MIN, MauvilleCity_GameCorner_EventScript_CardNotEnoughCoins
	msgbox MauvilleCity_GameCorner_Text_VideoPoker, MSGBOX_YESNO
	closemessage
	goto_if_eq VAR_RESULT, NO, MauvilleCity_GameCorner_EventScript_MinigameCancel
	playbgm MUS_GAME_CORNER, FALSE
	special StartVideoPoker
	waitstate
	savebgm MUS_DUMMY
	fadedefaultbgm
	releaseall
	end

MauvilleCity_GameCorner_EventScript_CardNotEnoughCoins::
	buffernumberstring STR_VAR_1, GC_CARD_STAKE_MIN
	msgbox MauvilleCity_GameCorner_Text_CardNotEnoughCoins, MSGBOX_DEFAULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_Snake::
	lockall
	setvar VAR_0x8004, GC_ARCADE_SNAKE
	goto MauvilleCity_GameCorner_EventScript_Arcade

MauvilleCity_GameCorner_EventScript_FlappyBird::
	lockall
	setvar VAR_0x8004, GC_ARCADE_FLAPPY
	goto MauvilleCity_GameCorner_EventScript_Arcade

MauvilleCity_GameCorner_EventScript_BlockStacker::
	lockall
	setvar VAR_0x8004, GC_ARCADE_BLOCK_STACKER
	goto MauvilleCity_GameCorner_EventScript_Arcade

@ VAR_0x8004 = GC_ARCADE_*
MauvilleCity_GameCorner_EventScript_Arcade::
	checkitem ITEM_COIN_CASE
	goto_if_eq VAR_RESULT, FALSE, MauvilleCity_GameCorner_EventScript_NoCoinCase
	switch VAR_0x8004
	case GC_ARCADE_SNAKE, MauvilleCity_GameCorner_EventScript_ArcadeNameSnake
	case GC_ARCADE_FLAPPY, MauvilleCity_GameCorner_EventScript_ArcadeNameFlappy
	bufferstring STR_VAR_2, MauvilleCity_GameCorner_Text_NameBlockStacker
	goto MauvilleCity_GameCorner_EventScript_ArcadeAsk

MauvilleCity_GameCorner_EventScript_ArcadeNameSnake::
	bufferstring STR_VAR_2, MauvilleCity_GameCorner_Text_NameSnake
	goto MauvilleCity_GameCorner_EventScript_ArcadeAsk

MauvilleCity_GameCorner_EventScript_ArcadeNameFlappy::
	bufferstring STR_VAR_2, MauvilleCity_GameCorner_Text_NameFlappy
	goto MauvilleCity_GameCorner_EventScript_ArcadeAsk

MauvilleCity_GameCorner_EventScript_ArcadeAsk::
	buffernumberstring STR_VAR_1, GC_ARCADE_ENTRY_FEE
	checkcoins VAR_TEMP_2
	goto_if_lt VAR_TEMP_2, GC_ARCADE_ENTRY_FEE, MauvilleCity_GameCorner_EventScript_ArcadeNotEnoughCoins
	showcoinsbox 1, 1
	msgbox MauvilleCity_GameCorner_Text_ArcadeAsk, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, MauvilleCity_GameCorner_EventScript_ArcadeCancel
	removecoins GC_ARCADE_ENTRY_FEE
	updatecoinsbox
	playse SE_SHOP
	delay 40
	hidecoinsbox
	closemessage
	setvar VAR_GAME_CORNER_WINNINGS, 0
	switch VAR_0x8004
	case GC_ARCADE_SNAKE, MauvilleCity_GameCorner_EventScript_ArcadeStartSnake
	case GC_ARCADE_FLAPPY, MauvilleCity_GameCorner_EventScript_ArcadeStartFlappy
	playbgm MUS_GC_BLOCK_STACKER, FALSE
	special StartBlockStacker
	goto MauvilleCity_GameCorner_EventScript_ArcadePayout

MauvilleCity_GameCorner_EventScript_ArcadeStartSnake::
	playbgm MUS_NONE, FALSE
	special StartSnake
	goto MauvilleCity_GameCorner_EventScript_ArcadePayout

MauvilleCity_GameCorner_EventScript_ArcadeStartFlappy::
	playbgm MUS_NONE, FALSE
	special StartFlappyBird
	goto MauvilleCity_GameCorner_EventScript_ArcadePayout

MauvilleCity_GameCorner_EventScript_ArcadePayout::
	waitstate
	savebgm MUS_DUMMY
	fadedefaultbgm
	goto_if_eq VAR_GAME_CORNER_WINNINGS, 0, MauvilleCity_GameCorner_EventScript_MinigameCancel
	showcoinsbox 1, 1
	buffernumberstring STR_VAR_1, VAR_GAME_CORNER_WINNINGS
	msgbox MauvilleCity_GameCorner_Text_ArcadeWon, MSGBOX_DEFAULT
	addcoins VAR_GAME_CORNER_WINNINGS
	setvar VAR_GAME_CORNER_WINNINGS, 0
	updatecoinsbox
	playse SE_SHOP
	delay 40
	hidecoinsbox
	closemessage
	releaseall
	end

MauvilleCity_GameCorner_EventScript_ArcadeNotEnoughCoins::
	msgbox MauvilleCity_GameCorner_Text_ArcadeNotEnoughCoins, MSGBOX_DEFAULT
	releaseall
	end

MauvilleCity_GameCorner_EventScript_ArcadeCancel::
	hidecoinsbox
	closemessage
	releaseall
	end

MauvilleCity_GameCorner_EventScript_MinigameCancel::
	closemessage
	releaseall
	end

MauvilleCity_GameCorner_EventScript_OutOfOrder::
	msgbox MauvilleCity_GameCorner_Text_OutOfOrder, MSGBOX_SIGN
	end

MauvilleCity_GameCorner_EventScript_Dealer::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_Dealer, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_Dealer2::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_Dealer2, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_PokerDealer1::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_PokerDealer1, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_PokerDealer2::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_PokerDealer2, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_Fisherman::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_Fisherman, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_Hiker::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_Hiker, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_Employee::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_Employee, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_EventScript_Bouncer::
	lock
	faceplayer
	msgbox MauvilleCity_GameCorner_Text_Bouncer, MSGBOX_DEFAULT
	goto MauvilleCity_GameCorner_EventScript_NPCReturnToSlots
	end

MauvilleCity_GameCorner_Text_Blackjack:
	.string "It's a BLACKJACK TABLE!\n"
	.string "Want to play?$"

MauvilleCity_GameCorner_Text_VideoPoker:
	.string "It's a VIDEO POKER machine!\n"
	.string "Want to play?$"

MauvilleCity_GameCorner_Text_CardNotEnoughCoins:
	.string "The table minimum is\n"
	.string "{STR_VAR_1} COINS…$"

MauvilleCity_GameCorner_Text_NameSnake:
	.string "ONIX$"

MauvilleCity_GameCorner_Text_NameFlappy:
	.string "FLUTTERFREE$"

MauvilleCity_GameCorner_Text_NameBlockStacker:
	.string "BLOCK BUILDER$"

MauvilleCity_GameCorner_Text_ArcadeAsk:
	.string "It's {STR_VAR_2}!\n"
	.string "Play for {STR_VAR_1} COINS?$"

MauvilleCity_GameCorner_Text_ArcadeNotEnoughCoins:
	.string "It's {STR_VAR_2}!\p"
	.string "It costs {STR_VAR_1} COINS.\n"
	.string "You don't have enough…$"

MauvilleCity_GameCorner_Text_ArcadeWon:
	.string "You won\n"
	.string "{STR_VAR_1} COINS!$"

MauvilleCity_GameCorner_Text_OutOfOrder:
	.string "“OUT OF ORDER”\n"
	.string "A sign is taped to the machine.$"

MauvilleCity_GameCorner_Text_Dealer:
	.string "Welcome to the BLACKJACK table!\p"
	.string "Wins pay 3 to 2 here, and a\n"
	.string "BLACKJACK pays double!$"

MauvilleCity_GameCorner_Text_Dealer2:
	.string "Bets run from 100 to 500 COINS.\n"
	.string "Good luck!$"

MauvilleCity_GameCorner_Text_PokerDealer1:
	.string "VIDEO POKER is simple.\p"
	.string "Hold the cards you like, draw\n"
	.string "the rest. JACKS or better pay!$"

MauvilleCity_GameCorner_Text_PokerDealer2:
	.string "A ROYAL FLUSH pays 250 times\n"
	.string "your bet! Well… up to 9,999.$"

MauvilleCity_GameCorner_Text_Fisherman:
	.string "I come here for the food.\p"
	.string "It's no CELADON CAFE…\n"
	.string "But it'll do.$"

MauvilleCity_GameCorner_Text_Hiker:
	.string "Gah! I blew it all on that\n"
	.string "ONIX game.\p"
	.string "The arcade machines cost 100\n"
	.string "COINS a go. Better be good!$"

MauvilleCity_GameCorner_Text_Employee:
	.string "I'm on the lookout for cheaters.\n"
	.string "Nobody likes a cheater!$"

MauvilleCity_GameCorner_Text_Bouncer:
	.string "Enjoy the GAME CORNER!$"
