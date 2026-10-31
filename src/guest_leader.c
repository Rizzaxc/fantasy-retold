#include "global.h"
#include "battle_setup.h"
#include "data.h"
#include "event_data.h"
#include "guest_leader.h"
#include "item.h"
#include "malloc.h"
#include "pokemon.h"
#include "random.h"
#include "string_util.h"
#include "trainer_see.h"
#include "trainer_util.h"
#include "constants/guest_leader.h"
#include "constants/hold_effects.h"
#include "constants/items.h"
#include "constants/opponents.h"
#include "constants/pokemon.h"

// Fantasy Retold: guest gym leaders (design.md §10). From the 4th gym on, the first fight with a leader may host a
// guest leader from another region (Kanto for now). The guest is rolled per gym (20%) and rerolled on every whiteout
// until that gym is beaten; the format (tag team / doubles / guest only) is rolled when the player engages and stays
// locked until a win or a whiteout. Guest parties are authored at their home-region levels and shifted at battle time
// so their ace matches the home leader's ace; EVs follow the level. In tag team and doubles each leader only brings
// their strongest GUEST_LEADER_TRIM_SIZE mons. Scripts pass the gym number (1-8) in VAR_0x8008.

// VAR_GUEST_LEADER: guest index + 1 (0 = no guest), the locked format, and the gym the roll is for.
#define GUEST_VAR_GUEST_MASK   0x000F
#define GUEST_VAR_FORMAT_SHIFT 4
#define GUEST_VAR_FORMAT_MASK  (0x3 << GUEST_VAR_FORMAT_SHIFT)
#define GUEST_VAR_GYM_SHIFT    8
#define GUEST_VAR_GYM_MASK     (0xF << GUEST_VAR_GYM_SHIFT)

#define GYM_COUNT  8
#define NO_GUEST   0xFF

struct GuestLeader
{
    u16 trainerId;
    u16 beatenFlag;
    enum Type type;
    enum Item tm; // given on top of the home rewards; ITEM_NONE gives nothing
    const u8 *intro;
    const u8 *defeat; // in battle
    const u8 *postBattle;
};

struct HomeLeader
{
    u16 trainerId;
    enum Type type;
    bool8 allowDoubles; // FALSE where the leader already fights a double battle
    const u8 *defeat; // in battle, for guest formats
};

static const struct GuestLeader sGuestLeaders[] =
{
    {
        .trainerId = TRAINER_GUEST_BROCK,
        .beatenFlag = FLAG_GUEST_BEATEN_BROCK,
        .type = TYPE_ROCK,
        .tm = ITEM_NONE,
        .intro = COMPOUND_STRING("BROCK: I'm BROCK of PEWTER CITY!\pMy rock-hard willpower came all the\nway from KANTO to test you!"),
        .defeat = COMPOUND_STRING("Your POKéMON cracked my defense.\nWell earned!"),
        .postBattle = COMPOUND_STRING("BROCK: Strong trainers grow on\nevery kind of terrain, huh?\pTake this. It's a KANTO keepsake."),
    },
    {
        .trainerId = TRAINER_GUEST_MISTY,
        .beatenFlag = FLAG_GUEST_BEATEN_MISTY,
        .type = TYPE_WATER,
        .tm = ITEM_NONE,
        .intro = COMPOUND_STRING("MISTY: I'm MISTY, CERULEAN's\nGYM LEADER!\pMy policy is an all-out offensive\nwith WATER-type POKéMON!"),
        .defeat = COMPOUND_STRING("Wow! You're too much!"),
        .postBattle = COMPOUND_STRING("MISTY: Hmph! Fine, you win.\pHere, take this. Don't let it\ngo to your head!"),
    },
    {
        .trainerId = TRAINER_GUEST_LT_SURGE,
        .beatenFlag = FLAG_GUEST_BEATEN_LT_SURGE,
        .type = TYPE_ELECTRIC,
        .tm = ITEM_NONE,
        .intro = COMPOUND_STRING("LT. SURGE: Hey, kid! Lightning\nstrikes twice in this GYM today!\pI'll zap you like I did my\nenemies in war!"),
        .defeat = COMPOUND_STRING("Whoa!\nYou're the real deal, kid!"),
        .postBattle = COMPOUND_STRING("LT. SURGE: You've got guts, kid!\nHere, a souvenir from VERMILION!"),
    },
    {
        .trainerId = TRAINER_GUEST_ERIKA,
        .beatenFlag = FLAG_GUEST_BEATEN_ERIKA,
        .type = TYPE_GRASS,
        .tm = ITEM_NONE,
        .intro = COMPOUND_STRING("ERIKA: Oh, how lovely… I'm ERIKA\nof CELADON CITY.\pI study the art of flower\narranging. Shall we battle?"),
        .defeat = COMPOUND_STRING("Oh! I concede defeat.\nYou are remarkably strong."),
        .postBattle = COMPOUND_STRING("ERIKA: What a refreshing battle.\nPlease accept this, with my thanks."),
    },
    {
        .trainerId = TRAINER_GUEST_KOGA,
        .beatenFlag = FLAG_GUEST_BEATEN_KOGA,
        .type = TYPE_POISON,
        .tm = ITEM_NONE,
        .intro = COMPOUND_STRING("KOGA: Fwahahaha! A mere child\ndares challenge me?\pI, KOGA of FUCHSIA, shall show you\nthe true terror of a ninja!"),
        .defeat = COMPOUND_STRING("Humph!\nYou have proven your worth!"),
        .postBattle = COMPOUND_STRING("KOGA: Take this as proof that\nyou bested a ninja master."),
    },
    {
        .trainerId = TRAINER_GUEST_SABRINA,
        .beatenFlag = FLAG_GUEST_BEATEN_SABRINA,
        .type = TYPE_PSYCHIC,
        .tm = ITEM_NONE,
        .intro = COMPOUND_STRING("SABRINA: I am SABRINA of SAFFRON.\nI foresaw your arrival.\pI also foresee your defeat."),
        .defeat = COMPOUND_STRING("This loss shocks me!\nBut a loss is a loss."),
        .postBattle = COMPOUND_STRING("SABRINA: I did not foresee this.\nTake it. You have earned it."),
    },
    {
        .trainerId = TRAINER_GUEST_BLAINE,
        .beatenFlag = FLAG_GUEST_BEATEN_BLAINE,
        .type = TYPE_FIRE,
        .tm = ITEM_NONE,
        .intro = COMPOUND_STRING("BLAINE: Hah! I am BLAINE, the\nhotheaded quiz master of CINNABAR!\pMy fiery POKéMON will incinerate\nall challengers!"),
        .defeat = COMPOUND_STRING("I have burned down to nothing!\nNot even ashes remain!"),
        .postBattle = COMPOUND_STRING("BLAINE: You've earned this!\nCome visit CINNABAR sometime!"),
    },
    {
        .trainerId = TRAINER_GUEST_GIOVANNI,
        .beatenFlag = FLAG_GUEST_BEATEN_GIOVANNI,
        .type = TYPE_GROUND,
        .tm = ITEM_NONE,
        .intro = COMPOUND_STRING("GIOVANNI: So you are the trainer\nHOENN is talking about.\pI am GIOVANNI of VIRIDIAN.\nShow me what you're made of."),
        .defeat = COMPOUND_STRING("Ha!\nThat was a truly intense fight!"),
        .postBattle = COMPOUND_STRING("GIOVANNI: I underestimated you.\nTake this. You will need it."),
    },
};

static const struct HomeLeader sHomeLeaders[GYM_COUNT - GUEST_LEADER_FIRST_GYM + 1] =
{
    { TRAINER_FLANNERY_1,     TYPE_FIRE,    TRUE,  COMPOUND_STRING("Oh…\nWe both got burned…") },
    { TRAINER_NORMAN_1,       TYPE_NORMAL,  TRUE,  COMPOUND_STRING("… … …\nI can't believe it.") },
    { TRAINER_WINONA_1,       TYPE_FLYING,  TRUE,  COMPOUND_STRING("Never before have I seen\nskies so clear!") },
    { TRAINER_TATE_AND_LIZA_1, TYPE_PSYCHIC, FALSE, COMPOUND_STRING("TATE: Huh?\nLIZA: Huh?") },
    { TRAINER_JUAN_1,         TYPE_WATER,   TRUE,  COMPOUND_STRING("Ahahaha, excellent!\nVery well, you are the winner.") },
};

static bool32 IsGuestGym(u32 gym)
{
    return gym >= GUEST_LEADER_FIRST_GYM && gym <= GYM_COUNT;
}

static const struct HomeLeader *GetHomeLeader(u32 gym)
{
    return &sHomeLeaders[gym - GUEST_LEADER_FIRST_GYM];
}

static u32 GetStoredGym(void)
{
    return (VarGet(VAR_GUEST_LEADER) & GUEST_VAR_GYM_MASK) >> GUEST_VAR_GYM_SHIFT;
}

static u32 GetStoredGuest(void)
{
    u32 guest = VarGet(VAR_GUEST_LEADER) & GUEST_VAR_GUEST_MASK;

    if (guest == 0 || guest > ARRAY_COUNT(sGuestLeaders))
        return NO_GUEST;
    return guest - 1;
}

static u32 GetStoredFormat(void)
{
    return (VarGet(VAR_GUEST_LEADER) & GUEST_VAR_FORMAT_MASK) >> GUEST_VAR_FORMAT_SHIFT;
}

static void SetStoredFormat(u32 format)
{
    u16 value = VarGet(VAR_GUEST_LEADER) & ~GUEST_VAR_FORMAT_MASK;
    VarSet(VAR_GUEST_LEADER, value | (format << GUEST_VAR_FORMAT_SHIFT));
}

// Rolls whether a guest visits this gym and who, and clears the locked format.
static void RollGuest(u32 gym)
{
    u8 pool[ARRAY_COUNT(sGuestLeaders)];
    u32 i, count = 0, guest = 0;

    if (Random() % 100 < GUEST_LEADER_CHANCE)
    {
        for (i = 0; i < ARRAY_COUNT(sGuestLeaders); i++)
        {
            if (sGuestLeaders[i].type != GetHomeLeader(gym)->type && !FlagGet(sGuestLeaders[i].beatenFlag))
                pool[count++] = i;
        }
        if (count != 0)
            guest = pool[Random() % count] + 1;
    }
    VarSet(VAR_GUEST_LEADER, guest | (gym << GUEST_VAR_GYM_SHIFT));
}

// The gym whose leader fight is waiting on the stored roll, or 0 once that leader is beaten.
static u32 GetActiveGym(void)
{
    u32 gym = GetStoredGym();

    if (!IsGuestGym(gym) || HasTrainerBeenFought(GetHomeLeader(gym)->trainerId))
        return 0;
    return gym;
}

static u32 GetGuestIndexByTrainer(u16 trainerId)
{
    for (u32 i = 0; i < ARRAY_COUNT(sGuestLeaders); i++)
    {
        if (sGuestLeaders[i].trainerId == trainerId)
            return i;
    }
    return NO_GUEST;
}

static u32 GetPartyMaxLevel(u16 trainerId)
{
    const struct Trainer *trainer = GetTrainerStructFromId(trainerId);
    u32 i, maxLevel = 0;

    for (i = 0; i < trainer->partySize; i++)
    {
        if (trainer->party[i].lvl > maxLevel)
            maxLevel = trainer->party[i].lvl;
    }
    return maxLevel;
}

// Levels added to a guest's authored party so their ace matches the home leader's ace.
static s32 GetLevelShift(u16 trainerId)
{
    u32 gym = GetStoredGym();

    if (GetGuestIndexByTrainer(trainerId) == NO_GUEST || !IsGuestGym(gym))
        return 0;
    return (s32)GetPartyMaxLevel(GetHomeLeader(gym)->trainerId) - (s32)GetPartyMaxLevel(trainerId);
}

static u32 ShiftLevel(u32 level, s32 shift)
{
    s32 newLevel = (s32)level + shift;

    if (newLevel < 1)
        return 1;
    if (newLevel > MAX_LEVEL)
        return MAX_LEVEL;
    return newLevel;
}

// EVs follow the level (ev * newLevel / authoredLevel). If a stat goes over the per-stat cap or the total over the
// overall cap, all six are scaled down together so the ratio between them holds.
static void ScaleEvs(const u8 *evs, u32 oldLevel, u32 newLevel, u8 *out)
{
    u32 i, scaled[NUM_STATS], sum = 0, max = 0, num = 1, den = 1;

    for (i = 0; i < NUM_STATS; i++)
    {
        scaled[i] = evs[i] * newLevel / (oldLevel ?: 1);
        sum += scaled[i];
        if (scaled[i] > max)
            max = scaled[i];
    }
    if (max > MAX_PER_STAT_EVS || sum > MAX_TOTAL_EVS)
    {
        if (MAX_PER_STAT_EVS * sum <= MAX_TOTAL_EVS * max)
            num = MAX_PER_STAT_EVS, den = max;
        else
            num = MAX_TOTAL_EVS, den = sum;
    }
    for (i = 0; i < NUM_STATS; i++)
        out[i] = scaled[i] * num / den;
}

static bool32 HasGimmick(const struct TrainerMon *mon)
{
    enum HoldEffect holdEffect = GetItemHoldEffect(mon->heldItem);

    return mon->teraType != TYPE_NONE
        || mon->gigantamaxFactor
        || mon->shouldUseDynamax
        || holdEffect == HOLD_EFFECT_MEGA_STONE
        || holdEffect == HOLD_EFFECT_Z_CRYSTAL
        || holdEffect == HOLD_EFFECT_PRIMAL_ORB;
}

static u32 SumIvs(u32 ivs)
{
    u32 i, sum = 0;

    for (i = 0; i < NUM_STATS; i++)
        sum += (ivs >> (5 * i)) & MAX_IV_MASK;
    return sum;
}

// The trainer is the guest or home leader of a pending tag team / doubles fight, so only brings their strongest mons.
static bool32 IsTrimmedLeader(u16 trainerId)
{
    u32 gym = GetActiveGym();
    u32 guest = GetStoredGuest();
    u32 format = GetStoredFormat();

    if (gym == 0 || guest == NO_GUEST || (format != GUEST_FORMAT_TAG && format != GUEST_FORMAT_DOUBLES))
        return FALSE;
    return trainerId == sGuestLeaders[guest].trainerId || trainerId == GetHomeLeader(gym)->trainerId;
}

struct StrengthKey
{
    u8 index;
    u8 level;
    bool8 gimmick;
    bool8 item;
    u16 evs;
    u16 ivs;
    u16 random;
};

static bool32 IsStronger(const struct StrengthKey *a, const struct StrengthKey *b)
{
    if (a->level != b->level)
        return a->level > b->level;
    if (a->gimmick != b->gimmick)
        return a->gimmick;
    if (a->item != b->item)
        return a->item;
    if (a->evs != b->evs)
        return a->evs > b->evs;
    if (a->ivs != b->ivs)
        return a->ivs > b->ivs;
    return a->random > b->random;
}

// Ranks by level, gimmick, held item, total EVs, total IVs, then at random. The picked mons keep their party order.
static u32 PickStrongest(const struct TrainerMon *party, u32 partySize, const struct TrainerMon *scaled, u8 *indices)
{
    struct StrengthKey keys[PARTY_SIZE], tmp;
    u32 i, j, count;

    for (i = 0; i < partySize; i++)
    {
        keys[i].index = i;
        keys[i].level = scaled[i].lvl;
        keys[i].gimmick = HasGimmick(&party[i]);
        keys[i].item = party[i].heldItem != ITEM_NONE;
        keys[i].evs = 0;
        if (scaled[i].ev != NULL)
        {
            for (j = 0; j < NUM_STATS; j++)
                keys[i].evs += scaled[i].ev[j];
        }
        keys[i].ivs = SumIvs(party[i].iv);
        keys[i].random = Random();
    }
    for (i = 1; i < partySize; i++)
    {
        for (j = i; j > 0 && IsStronger(&keys[j], &keys[j - 1]); j--)
            SWAP(keys[j], keys[j - 1], tmp);
    }

    count = min(partySize, GUEST_LEADER_TRIM_SIZE);
    for (i = 0; i < count; i++)
        indices[i] = keys[i].index;
    for (i = 1; i < count; i++)
    {
        for (j = i; j > 0 && indices[j] < indices[j - 1]; j--)
            SWAP(indices[j], indices[j - 1], tmp.index);
    }
    return count;
}

// Builds a guest's party (level shift, scaled EVs) and trims a tag team / doubles leader to their strongest mons.
// Returns FALSE for every other trainer, which then takes the normal path.
bool32 GuestLeader_TryCreateParty(struct Pokemon *party, u16 trainerId)
{
    const struct Trainer *trainer = GetTrainerStructFromId(trainerId);
    bool32 isGuest = GetGuestIndexByTrainer(trainerId) != NO_GUEST;
    bool32 trim = IsTrimmedLeader(trainerId);
    struct TrainerMon scaled[PARTY_SIZE];
    u8 evs[PARTY_SIZE][NUM_STATS];
    u8 indices[PARTY_SIZE];
    u32 i, count, partySize;
    s32 shift;
    struct TrainerGenerator *trainerGen;

    if (!isGuest && !trim)
        return FALSE;

    partySize = min(trainer->partySize, PARTY_SIZE);
    shift = GetLevelShift(trainerId);
    for (i = 0; i < partySize; i++)
    {
        scaled[i] = trainer->party[i];
        scaled[i].lvl = ShiftLevel(trainer->party[i].lvl, shift);
        if (isGuest && trainer->party[i].ev != NULL)
        {
            ScaleEvs(trainer->party[i].ev, trainer->party[i].lvl, scaled[i].lvl, evs[i]);
            scaled[i].ev = evs[i];
        }
    }

    if (trim)
    {
        count = PickStrongest(trainer->party, partySize, scaled, indices);
    }
    else
    {
        count = partySize;
        for (i = 0; i < count; i++)
            indices[i] = i;
    }

    ZeroPartyMons(party);
    trainerGen = AllocZeroed(sizeof(struct TrainerGenerator));
    MakeTrainerGenerator(trainerGen, trainer);
    for (i = 0; i < count; i++)
        GenerateMonFromTrainerMon(&party[i], &scaled[indices[i]], trainerGen);
    Free(trainerGen);
    return TRUE;
}

// Prize money is based on the last authored mon's level; a guest pays at its shifted level.
u32 GuestLeader_GetPrizeLevel(u16 trainerId, u32 level)
{
    return ShiftLevel(level, GetLevelShift(trainerId));
}

// Every whiteout rerolls the pending gym's guest (and unlocks its format) until that gym is beaten.
void GuestLeader_OnWhiteOut(void)
{
    u32 gym = GetActiveGym();

    if (gym != 0)
        RollGuest(gym);
}

// The guest's name in STR_VAR_1 and the home leader's in STR_VAR_2 (again after a battle, which reuses them).
void GuestLeader_BufferNames(void)
{
    StringCopy(gStringVar1, GetTrainerNameFromId(sGuestLeaders[GetStoredGuest()].trainerId));
    StringCopy(gStringVar2, GetTrainerNameFromId(GetHomeLeader(GetStoredGym())->trainerId));
}

// Engage. VAR_0x8008 = gym. Rolls the gym's guest the first time it is engaged (or after the last roll's gym was
// beaten). Returns TRUE if a guest is here, with the names buffered.
bool8 GuestLeader_GetState(void)
{
    u32 gym = gSpecialVar_0x8008;

    if (!IsGuestGym(gym) || HasTrainerBeenFought(GetHomeLeader(gym)->trainerId))
        return FALSE;
    if (GetStoredGym() != gym)
        RollGuest(gym);

    if (GetStoredGuest() == NO_GUEST)
        return FALSE;
    GuestLeader_BufferNames();
    return TRUE;
}

// Rolls the format if it isn't locked yet (doubles only where the home leader fights singles) and returns it.
u16 GuestLeader_RollFormat(void)
{
    u32 format = GetStoredFormat();

    if (format == GUEST_FORMAT_NONE)
    {
        if (GetHomeLeader(GetStoredGym())->allowDoubles)
        {
            static const u8 formats[] = { GUEST_FORMAT_TAG, GUEST_FORMAT_DOUBLES, GUEST_FORMAT_SOLO };
            format = formats[Random() % ARRAY_COUNT(formats)];
        }
        else
        {
            format = (Random() % 2) ? GUEST_FORMAT_TAG : GUEST_FORMAT_SOLO;
        }
        SetStoredFormat(format);
    }
    return format;
}

// TRUE if the locked format needs two usable Pokémon (doubles, or a tag team ending in a double battle).
bool8 GuestLeader_NeedsTwoMons(void)
{
    u32 format = GetStoredFormat();
    u16 home = GetHomeLeader(GetStoredGym())->trainerId;

    if (format != GUEST_FORMAT_DOUBLES
     && !(format == GUEST_FORMAT_TAG && GetTrainerBattleType(home) == TRAINER_BATTLE_TYPE_DOUBLES))
        return FALSE;
    return GetMonsStateToDoubles() != PLAYER_HAS_TWO_USABLE_MONS;
}

// Buffers the guest's intro line in gStringVar4.
void GuestLeader_BufferIntro(void)
{
    StringExpandPlaceholders(gStringVar4, sGuestLeaders[GetStoredGuest()].intro);
}

// Sets up the battle for `dotrainerbattle`. VAR_0x8005 = GUEST_BATTLE_*.
void GuestLeader_ConfigureBattle(void)
{
    const struct GuestLeader *guest = &sGuestLeaders[GetStoredGuest()];
    const struct HomeLeader *home = GetHomeLeader(GetStoredGym());

    InitTrainerBattleParameter();
    switch (gSpecialVar_0x8005)
    {
    case GUEST_BATTLE_GUEST:
        TRAINER_BATTLE_PARAM.opponentA = guest->trainerId;
        TRAINER_BATTLE_PARAM.defeatTextA = (u8 *)guest->defeat;
        break;
    case GUEST_BATTLE_HOME:
        TRAINER_BATTLE_PARAM.opponentA = home->trainerId;
        TRAINER_BATTLE_PARAM.defeatTextA = (u8 *)home->defeat;
        break;
    case GUEST_BATTLE_DOUBLES:
        TRAINER_BATTLE_PARAM.opponentA = guest->trainerId;
        TRAINER_BATTLE_PARAM.defeatTextA = (u8 *)guest->defeat;
        TRAINER_BATTLE_PARAM.opponentB = home->trainerId;
        TRAINER_BATTLE_PARAM.defeatTextB = (u8 *)home->defeat;
        gNoOfApproachingTrainers = 2;
        break;
    }
}

// After the win: marks the guest as beaten, marks the home leader as beaten (guest-only never fights them), buffers
// the guest's post-battle line in gStringVar4, returns the guest's TM (ITEM_NONE for none) and clears the roll.
u16 GuestLeader_OnWin(void)
{
    const struct GuestLeader *guest = &sGuestLeaders[GetStoredGuest()];
    u16 home = GetHomeLeader(GetStoredGym())->trainerId;

    FlagSet(guest->beatenFlag);
    if (!HasTrainerBeenFought(home))
        SetTrainerFlag(home);
    StringExpandPlaceholders(gStringVar4, guest->postBattle);
    VarSet(VAR_GUEST_LEADER, 0);
    return guest->tm;
}
