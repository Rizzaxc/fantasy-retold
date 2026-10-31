@ Fantasy Retold: Birch's Aide research trade (design.md §10).
@ The player hands over two party Pokémon and gets back one stronger, non-Hoenn Lv5 Pokémon.
@ One request after the Route 103 rival, then one more after every badge or Elite Four clear.
@ Reached from LittlerootTown_ProfessorBirchsLab_EventScript_Aide, which has already locked the player.
Common_EventScript_LabResearch::
	specialvar VAR_RESULT, LabResearch_GetState
	goto_if_eq VAR_RESULT, LAB_RESEARCH_CLOSED, Common_EventScript_LabResearchClosed
	goto_if_eq VAR_RESULT, LAB_RESEARCH_OPEN_TYPED, Common_EventScript_LabResearchAskTyped
	msgbox gText_LabResearchRequest, MSGBOX_YESNO
	goto Common_EventScript_LabResearchAnswer

Common_EventScript_LabResearchAskTyped::
	msgbox gText_LabResearchRequestTyped, MSGBOX_YESNO
Common_EventScript_LabResearchAnswer::
	goto_if_ne VAR_RESULT, YES, Common_EventScript_LabResearchDecline
Common_EventScript_LabResearchChooseFirst::
	msgbox gText_LabResearchChooseFirst, MSGBOX_DEFAULT
	special ChoosePartyMon
	lock
	faceplayer
	goto_if_ge VAR_0x8004, PARTY_SIZE, Common_EventScript_LabResearchDecline
	special IsSelectedMonEgg
	goto_if_eq VAR_RESULT, TRUE, Common_EventScript_LabResearchFirstIsEgg
	copyvar VAR_0x8005, VAR_0x8004
Common_EventScript_LabResearchChooseSecond::
	msgbox gText_LabResearchChooseSecond, MSGBOX_DEFAULT
	special ChoosePartyMon
	lock
	faceplayer
	goto_if_ge VAR_0x8004, PARTY_SIZE, Common_EventScript_LabResearchDecline
	goto_if_eq VAR_0x8004, VAR_0x8005, Common_EventScript_LabResearchSameMon
	special IsSelectedMonEgg
	goto_if_eq VAR_RESULT, TRUE, Common_EventScript_LabResearchSecondIsEgg
	copyvar VAR_0x8006, VAR_0x8004
	specialvar VAR_RESULT, LabResearch_CheckPair
	goto_if_eq VAR_RESULT, LAB_RESEARCH_PAIR_NEEDS_TYPE, Common_EventScript_LabResearchNeedsType
	goto_if_eq VAR_RESULT, LAB_RESEARCH_PAIR_NO_REWARD, Common_EventScript_LabResearchNoReward
	bufferpartymonnick STR_VAR_2, VAR_0x8005
	bufferpartymonnick STR_VAR_3, VAR_0x8006
	msgbox gText_LabResearchConfirm, MSGBOX_YESNO
	goto_if_ne VAR_RESULT, YES, Common_EventScript_LabResearchDecline
	specialvar VAR_RESULT, LabResearch_Trade
	goto_if_ge VAR_RESULT, PARTY_SIZE, Common_EventScript_LabResearchNoReward
	msgbox gText_LabResearchTakeThisOne, MSGBOX_DEFAULT
	playfanfare MUS_OBTAIN_ITEM
	message gText_LabResearchReceivedMon
	waitmessage
	waitfanfare
	msgbox gText_LabResearchNicknameMon, MSGBOX_YESNO
	call_if_eq VAR_RESULT, YES, Common_EventScript_NameReceivedPartyMon
	msgbox gText_LabResearchThanks, MSGBOX_DEFAULT
	release
	end

Common_EventScript_LabResearchFirstIsEgg::
	msgbox gText_LabResearchNoEggs, MSGBOX_DEFAULT
	goto Common_EventScript_LabResearchChooseFirst

Common_EventScript_LabResearchSecondIsEgg::
	msgbox gText_LabResearchNoEggs, MSGBOX_DEFAULT
	goto Common_EventScript_LabResearchChooseSecond

Common_EventScript_LabResearchSameMon::
	msgbox gText_LabResearchSameMon, MSGBOX_DEFAULT
	goto Common_EventScript_LabResearchChooseSecond

Common_EventScript_LabResearchNeedsType::
	specialvar VAR_RESULT, LabResearch_GetState
	msgbox gText_LabResearchNeedsType, MSGBOX_DEFAULT
	release
	end

@ Nothing beats the pair: offer a Rare Candy, which uses up this request.
Common_EventScript_LabResearchNoReward::
	msgbox gText_LabResearchNoReward, MSGBOX_YESNO
	goto_if_ne VAR_RESULT, YES, Common_EventScript_LabResearchDecline
	giveitem ITEM_RARE_CANDY
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_LabResearchSkipBagFull
	special LabResearch_Skip
	msgbox gText_LabResearchSkipped, MSGBOX_DEFAULT
	release
	end

Common_EventScript_LabResearchSkipBagFull::
	msgbox gText_LabResearchSkipBagFull, MSGBOX_DEFAULT
	release
	end

Common_EventScript_LabResearchDecline::
	msgbox gText_LabResearchDecline, MSGBOX_DEFAULT
	release
	end

Common_EventScript_LabResearchClosed::
	msgbox gText_LabResearchClosed, MSGBOX_DEFAULT
	release
	end

gText_LabResearchIntro::
	.string "Oh, {PLAYER}! Congratulations on\n"
	.string "your POKéDEX!\p"
	.string "I'm the PROF's AIDE. While he's out\n"
	.string "in the field, I study POKéMON here.\p"
	.string "Here's my request: bring me any two\n"
	.string "POKéMON to research.\p"
	.string "In exchange, I'll give you one back.\n"
	.string "It'll be a foreign POKéMON that\l"
	.string "grows stronger than both of yours,\l"
	.string "and shares one of their types.\p"
	.string "I'll have a new request after every\n"
	.string "GYM BADGE you earn.\p"
	.string "Talk to me once you've caught\n"
	.string "a few POKéMON!$"

gText_LabResearchRequest::
	.string "{PLAYER}! Have you come to help\n"
	.string "with my research?\p"
	.string "Lend me any two POKéMON, and I'll\n"
	.string "give you a stronger one in return.$"

gText_LabResearchRequestTyped::
	.string "{PLAYER}! Have you come to help\n"
	.string "with my research?\p"
	.string "This time I'm studying the\n"
	.string "{STR_VAR_1} type.\p"
	.string "Lend me any two POKéMON, as long as\n"
	.string "one of them is {STR_VAR_1} type.$"

gText_LabResearchChooseFirst::
	.string "Which POKéMON will you lend me\n"
	.string "first?$"

gText_LabResearchChooseSecond::
	.string "And the second one?$"

gText_LabResearchNoEggs::
	.string "An EGG? I can't study a POKéMON\n"
	.string "that hasn't hatched yet!$"

gText_LabResearchSameMon::
	.string "You've already picked that one.\n"
	.string "I need two different POKéMON.$"

gText_LabResearchNeedsType::
	.string "Hmm… Neither of these is\n"
	.string "{STR_VAR_1} type.\p"
	.string "Come back with a {STR_VAR_1}-type\n"
	.string "POKéMON, please!$"

gText_LabResearchNoReward::
	.string "Wow… These two are already as\n"
	.string "strong as POKéMON get.\p"
	.string "I don't have anything stronger\n"
	.string "to give you in return.\p"
	.string "How about a RARE CANDY for your\n"
	.string "trouble instead?$"

gText_LabResearchSkipped::
	.string "I'll wait for my next request,\n"
	.string "then. Keep it up, {PLAYER}!$"

gText_LabResearchSkipBagFull::
	.string "Your BAG is full… Come back once\n"
	.string "you've made some room!$"

gText_LabResearchConfirm::
	.string "I'll take {STR_VAR_2} and\n"
	.string "{STR_VAR_3} for my research.\p"
	.string "Is that okay?$"

gText_LabResearchTakeThisOne::
	.string "Thanks a lot!\n"
	.string "Here, take this one in exchange!$"

gText_LabResearchReceivedMon::
	.string "{PLAYER} received {STR_VAR_2}!$"

gText_LabResearchNicknameMon::
	.string "Want to give a nickname to\n"
	.string "the {STR_VAR_2} you received?$"

gText_LabResearchThanks::
	.string "Come back with another two after\n"
	.string "your next GYM BADGE!$"

gText_LabResearchDecline::
	.string "Oh, okay. Come back anytime!$"

gText_LabResearchClosed::
	.string "Thanks to you, my research is\n"
	.string "going well!\p"
	.string "I'll have a new request after your\n"
	.string "next GYM BADGE.$"
