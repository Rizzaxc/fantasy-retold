@ Fantasy Retold: guest gym leaders (design.md §2).
@ Call at the start of a gym 4-8 leader's first fight with VAR_0x8008 = gym number. Returns GUEST_LEADER_RESULT_* in
@ VAR_RESULT: NONE (no guest: run the vanilla battle), WON (run the gym's badge script) or BLOCKED (not enough
@ Pokémon for the locked format: release). A loss whites out as usual, which rerolls the guest.
Common_EventScript_GuestLeader::
	specialvar VAR_RESULT, GuestLeader_GetState
	goto_if_eq VAR_RESULT, FALSE, Common_EventScript_GuestLeaderNone
	lock
	faceplayer
	msgbox gText_GuestLeaderArrives, MSGBOX_DEFAULT
	special GuestLeader_BufferIntro
	msgbox gStringVar4, MSGBOX_DEFAULT
	specialvar VAR_0x8009, GuestLeader_RollFormat
	specialvar VAR_RESULT, GuestLeader_NeedsTwoMons
	goto_if_eq VAR_RESULT, TRUE, Common_EventScript_GuestLeaderBlocked
	switch VAR_0x8009
	case GUEST_FORMAT_TAG, Common_EventScript_GuestLeaderTag
	case GUEST_FORMAT_DOUBLES, Common_EventScript_GuestLeaderDoubles
	goto Common_EventScript_GuestLeaderSolo

@ Guest first, then the home leader, with no heal in between.
Common_EventScript_GuestLeaderTag::
	msgbox gText_GuestLeaderTag, MSGBOX_DEFAULT
	setvar VAR_0x8005, GUEST_BATTLE_GUEST
	special GuestLeader_ConfigureBattle
	dotrainerbattle
	special GuestLeader_BufferNames
	msgbox gText_GuestLeaderTagNext, MSGBOX_DEFAULT
	setvar VAR_0x8005, GUEST_BATTLE_HOME
	special GuestLeader_ConfigureBattle
	dotrainerbattle
	goto Common_EventScript_GuestLeaderWon

Common_EventScript_GuestLeaderDoubles::
	msgbox gText_GuestLeaderDoubles, MSGBOX_DEFAULT
	setvar VAR_0x8005, GUEST_BATTLE_DOUBLES
	special GuestLeader_ConfigureBattle
	dotrainerbattle
	goto Common_EventScript_GuestLeaderWon

Common_EventScript_GuestLeaderSolo::
	msgbox gText_GuestLeaderSolo, MSGBOX_DEFAULT
	setvar VAR_0x8005, GUEST_BATTLE_GUEST
	special GuestLeader_ConfigureBattle
	dotrainerbattle
	goto Common_EventScript_GuestLeaderWon

Common_EventScript_GuestLeaderWon::
	specialvar VAR_0x8009, GuestLeader_OnWin
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto_if_eq VAR_0x8009, ITEM_NONE, Common_EventScript_GuestLeaderWonDone
	giveitem VAR_0x8009
	call_if_eq VAR_RESULT, FALSE, Common_EventScript_GuestLeaderBagFull
Common_EventScript_GuestLeaderWonDone::
	setvar VAR_RESULT, GUEST_LEADER_RESULT_WON
	return

Common_EventScript_GuestLeaderBagFull::
	msgbox gText_TooBadBagIsFull, MSGBOX_DEFAULT
	return

Common_EventScript_GuestLeaderBlocked::
	msgbox gText_GuestLeaderNeedTwoMons, MSGBOX_DEFAULT
	setvar VAR_RESULT, GUEST_LEADER_RESULT_BLOCKED
	return

Common_EventScript_GuestLeaderNone::
	setvar VAR_RESULT, GUEST_LEADER_RESULT_NONE
	return

Common_EventScript_GuestLeaderEnd::
	release
	end

gText_GuestLeaderArrives::
	.string "{STR_VAR_2}: Welcome, challenger!\p"
	.string "Today this GYM has a special\n"
	.string "visitor from KANTO…$"

gText_GuestLeaderTag::
	.string "{STR_VAR_2}: {STR_VAR_1} will battle\n"
	.string "you first. Win, and you face me!\p"
	.string "And no rest in between!$"

gText_GuestLeaderTagNext::
	.string "{STR_VAR_2}: You beat {STR_VAR_1}!\n"
	.string "But I'm next. Get ready!$"

gText_GuestLeaderDoubles::
	.string "{STR_VAR_2}: {STR_VAR_1} and I will\n"
	.string "face you together!$"

gText_GuestLeaderSolo::
	.string "{STR_VAR_2}: Today, {STR_VAR_1} takes\n"
	.string "my place.\p"
	.string "Win, and the BADGE is yours!$"

gText_GuestLeaderNeedTwoMons::
	.string "{STR_VAR_2}: This battle needs at\n"
	.string "least two POKéMON that can fight.\p"
	.string "Come back when you're ready.$"
