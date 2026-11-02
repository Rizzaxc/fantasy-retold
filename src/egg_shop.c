#include "global.h"
#include "battle_setup.h"
#include "egg_shop.h"
#include "event_data.h"
#include "gym_reward.h"
#include "money.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "script_pokemon_util.h"
#include "string_util.h"
#include "constants/characters.h"
#include "constants/game_stat.h"
#include "constants/opponents.h"
#include "constants/pokemon.h"
#include "constants/regions.h"
#include "constants/species.h"

// Fantasy Retold: the daycare lady's egg shop (design.md §10). One Egg per tier, sold as "<REGION> <TIER> EGG"; the species
// is rolled when bought (hidden until it hatches) from that tier's non-Hoenn species of that region. Restocked after
// every badge, rival battle and Elite Four clear. Priced as a money sink. Scripts pass the slot (0-2) in VAR_0x8004.

enum EggShopTier
{
    EGG_SHOP_STARTER,
    EGG_SHOP_STRONG,
    EGG_SHOP_POWERHOUSE,
    EGG_SHOP_TIER_COUNT,
};

// Slot var: in-stock bit, region, tier. 0 = sold.
#define EGG_SHOP_SLOTS         EGG_SHOP_TIER_COUNT
#define EGG_SHOP_IN_STOCK      (1 << 15)
#define EGG_SHOP_REGION_SHIFT  4
#define EGG_SHOP_TIER_MASK     ((1 << EGG_SHOP_REGION_SHIFT) - 1)
#define EGG_SHOP_REGION_MASK   0xFF

static const u32 sEggShopPrices[EGG_SHOP_TIER_COUNT] =
{
    [EGG_SHOP_STARTER]    = 30000,
    [EGG_SHOP_STRONG]     = 60000,
    [EGG_SHOP_POWERHOUSE] = 120000,
};

static const u8 sText_SoldOut[] = _("SOLD OUT");
static const u8 sText_Egg[] = _(" EGG");

static const u8 *const sEggShopTierNames[EGG_SHOP_TIER_COUNT] =
{
    [EGG_SHOP_STARTER]    = COMPOUND_STRING("STARTER"),
    [EGG_SHOP_STRONG]     = COMPOUND_STRING("STRONG"),
    [EGG_SHOP_POWERHOUSE] = COMPOUND_STRING("POWERHOUSE"),
};

static const u8 *const sEggShopRegionNames[REGIONS_COUNT] =
{
    [REGION_NONE]   = COMPOUND_STRING("FARAWAY"),
    [REGION_KANTO]  = COMPOUND_STRING("KANTO"),
    [REGION_JOHTO]  = COMPOUND_STRING("JOHTO"),
    [REGION_HOENN]  = COMPOUND_STRING("HOENN"),
    [REGION_SINNOH] = COMPOUND_STRING("SINNOH"),
    [REGION_UNOVA]  = COMPOUND_STRING("UNOVA"),
    [REGION_KALOS]  = COMPOUND_STRING("KALOS"),
    [REGION_ALOLA]  = COMPOUND_STRING("ALOLA"),
    [REGION_GALAR]  = COMPOUND_STRING("GALAR"),
    [REGION_HISUI]  = COMPOUND_STRING("HISUI"),
    [REGION_PALDEA] = COMPOUND_STRING("PALDEA"),
};

// Every starter, plus Eevee.
static const u16 sEggShopStarters[] =
{
    SPECIES_BULBASAUR, SPECIES_CHARMANDER, SPECIES_SQUIRTLE,
    SPECIES_CHIKORITA, SPECIES_CYNDAQUIL, SPECIES_TOTODILE,
    SPECIES_TURTWIG, SPECIES_CHIMCHAR, SPECIES_PIPLUP,
    SPECIES_SNIVY, SPECIES_TEPIG, SPECIES_OSHAWOTT,
    SPECIES_CHESPIN, SPECIES_FENNEKIN, SPECIES_FROAKIE,
    SPECIES_ROWLET, SPECIES_LITTEN, SPECIES_POPPLIO,
    SPECIES_GROOKEY, SPECIES_SCORBUNNY, SPECIES_SOBBLE,
    SPECIES_SPRIGATITO, SPECIES_FUECOCO, SPECIES_QUAXLY,
    SPECIES_EEVEE,
};

// Lines where some member (any form) has been Smogon OU or Ubers in any generation. The Egg is what breeding
// gives (no incense babies). Starters, Powerhouse lines, legendaries, mythicals, Ultra Beasts, Paradoxes and lines
// that can't come from an Egg in vanilla (fossil chimeras, Gimmighoul) are left out.
static const u16 sEggShopStrong[] =
{
    // Gen 1-2 lines
    SPECIES_BELLSPROUT,     // Victreebel
    SPECIES_SHELLDER,       // Cloyster
    SPECIES_GASTLY,         // Gengar
    SPECIES_DROWZEE,        // Hypno
    SPECIES_CUBONE,         // Marowak
    SPECIES_EXEGGCUTE,      // Exeggutor
    SPECIES_CHANSEY,        // Chansey, Blissey
    SPECIES_KANGASKHAN,
    SPECIES_SMOOCHUM,       // Jynx
    SPECIES_SCYTHER,        // Scizor
    SPECIES_TAUROS,
    SPECIES_LAPRAS,
    SPECIES_PORYGON,        // Porygon2, Porygon-Z
    SPECIES_AERODACTYL,
    SPECIES_SNORLAX,
    SPECIES_SLOWPOKE,       // Slowbro, Slowking
    SPECIES_SLOWPOKE_GALAR, // Galarian Slowking
    SPECIES_MEOWTH,         // Persian
    SPECIES_NIDORAN_M,      // Nidoking
    SPECIES_DIGLETT,        // Dugtrio
    SPECIES_CLEFFA,         // Clefable
    SPECIES_POLIWAG,        // Politoed
    SPECIES_TANGELA,        // Tangrowth
    SPECIES_ONIX,           // Steelix
    SPECIES_TOGEPI,         // Togekiss
    SPECIES_PINECO,         // Forretress
    SPECIES_GLIGAR,         // Gliscor
    SPECIES_SNEASEL,        // Weavile
    SPECIES_SWINUB,         // Mamoswine
    SPECIES_HOUNDOUR,       // Houndoom
    SPECIES_MILTANK,
    SPECIES_SMEARGLE,
    SPECIES_MURKROW,        // Honchkrow
    SPECIES_YANMA,          // Yanmega
    SPECIES_AIPOM,          // Ambipom
    SPECIES_WOOPER_PALDEA,  // Clodsire
    SPECIES_TEDDIURSA,      // Ursaluna
    // Gen 4 lines
    SPECIES_RIOLU,          // Lucario
    SPECIES_BRONZOR,        // Bronzong
    SPECIES_HIPPOPOTAS,     // Hippowdon
    SPECIES_SNOVER,         // Abomasnow
    SPECIES_CROAGUNK,       // Toxicroak
    SPECIES_SHELLOS,        // Gastrodon
    SPECIES_ROTOM,
    SPECIES_BUNEARY,        // Lopunny
    // Gen 5 lines
    SPECIES_FERROSEED,      // Ferrothorn
    SPECIES_DRILBUR,        // Excadrill
    SPECIES_FRILLISH,       // Jellicent
    SPECIES_TIMBURR,        // Conkeldurr
    SPECIES_SOLOSIS,        // Reuniclus
    SPECIES_GOTHITA,        // Gothitelle
    SPECIES_LITWICK,        // Chandelure
    SPECIES_AXEW,           // Haxorus
    SPECIES_MIENFOO,        // Mienshao
    SPECIES_PAWNIARD,       // Bisharp, Kingambit
    SPECIES_FOONGUS,        // Amoonguss
    SPECIES_DARUMAKA,       // Darmanitan
    SPECIES_DARUMAKA_GALAR, // Galarian Darmanitan
    SPECIES_VULLABY,        // Mandibuzz
    SPECIES_TYMPOLE,        // Seismitoad
    // Gen 6-7 lines
    SPECIES_FLETCHLING,     // Talonflame
    SPECIES_HONEDGE,        // Aegislash
    SPECIES_KLEFKI,
    SPECIES_HAWLUCHA,
    SPECIES_MAREANIE,       // Toxapex
    SPECIES_MIMIKYU,
    // Gen 8-9 lines
    SPECIES_ROOKIDEE,       // Corviknight
    SPECIES_ARROKUDA,       // Barraskewda
    SPECIES_IMPIDIMP,       // Grimmsnarl
    SPECIES_HATENNA,        // Hatterene
    SPECIES_NACLI,          // Garganacl
    SPECIES_GLIMMET,        // Glimmora
    SPECIES_DONDOZO,
    SPECIES_TINKATINK,      // Tinkaton
    SPECIES_TANDEMAUS,      // Maushold
    SPECIES_DURALUDON,      // Archaludon
    SPECIES_APPLIN,         // Hydrapple
};

// Pseudo-legendaries, plus Volcarona, Houndstone, Basculegion, Espathra, Annihilape, Palafin and Sneasler.
// Hoenn lines (Bagon, Beldum) are listed for completeness; IsEggShopCandidate drops them.
static const u16 sEggShopPowerhouses[] =
{
    SPECIES_DRATINI, SPECIES_LARVITAR, SPECIES_BAGON, SPECIES_BELDUM, SPECIES_GIBLE,
    SPECIES_DEINO, SPECIES_GOOMY, SPECIES_JANGMO_O, SPECIES_DREEPY, SPECIES_FRIGIBAX,
    SPECIES_LARVESTA,               // Volcarona
    SPECIES_GREAVARD,               // Houndstone
    SPECIES_BASCULIN_WHITE_STRIPED, // Basculegion
    SPECIES_FLITTLE,                // Espathra
    SPECIES_MANKEY,                 // Annihilape
    SPECIES_FINIZEN,                // Palafin
    SPECIES_SNEASEL_HISUI,          // Sneasler
};

struct EggShopPool
{
    const u16 *species;
    u32 count;
};

static const struct EggShopPool sEggShopPools[EGG_SHOP_TIER_COUNT] =
{
    [EGG_SHOP_STARTER]    = {sEggShopStarters, ARRAY_COUNT(sEggShopStarters)},
    [EGG_SHOP_STRONG]     = {sEggShopStrong, ARRAY_COUNT(sEggShopStrong)},
    [EGG_SHOP_POWERHOUSE] = {sEggShopPowerhouses, ARRAY_COUNT(sEggShopPowerhouses)},
};

// One battle each; any gender / starter variant counts.
static const u16 sRivalBattles[][6] =
{
    {TRAINER_MAY_ROUTE_103_TREECKO, TRAINER_MAY_ROUTE_103_TORCHIC, TRAINER_MAY_ROUTE_103_MUDKIP,
     TRAINER_BRENDAN_ROUTE_103_TREECKO, TRAINER_BRENDAN_ROUTE_103_TORCHIC, TRAINER_BRENDAN_ROUTE_103_MUDKIP},
    {TRAINER_MAY_RUSTBORO_TREECKO, TRAINER_MAY_RUSTBORO_TORCHIC, TRAINER_MAY_RUSTBORO_MUDKIP,
     TRAINER_BRENDAN_RUSTBORO_TREECKO, TRAINER_BRENDAN_RUSTBORO_TORCHIC, TRAINER_BRENDAN_RUSTBORO_MUDKIP},
    {TRAINER_MAY_ROUTE_110_TREECKO, TRAINER_MAY_ROUTE_110_TORCHIC, TRAINER_MAY_ROUTE_110_MUDKIP,
     TRAINER_BRENDAN_ROUTE_110_TREECKO, TRAINER_BRENDAN_ROUTE_110_TORCHIC, TRAINER_BRENDAN_ROUTE_110_MUDKIP},
    {TRAINER_MAY_ROUTE_119_TREECKO, TRAINER_MAY_ROUTE_119_TORCHIC, TRAINER_MAY_ROUTE_119_MUDKIP,
     TRAINER_BRENDAN_ROUTE_119_TREECKO, TRAINER_BRENDAN_ROUTE_119_TORCHIC, TRAINER_BRENDAN_ROUTE_119_MUDKIP},
    {TRAINER_MAY_LILYCOVE_TREECKO, TRAINER_MAY_LILYCOVE_TORCHIC, TRAINER_MAY_LILYCOVE_MUDKIP,
     TRAINER_BRENDAN_LILYCOVE_TREECKO, TRAINER_BRENDAN_LILYCOVE_TORCHIC, TRAINER_BRENDAN_LILYCOVE_MUDKIP},
    {TRAINER_WALLY_MAUVILLE, TRAINER_WALLY_MAUVILLE, TRAINER_WALLY_MAUVILLE,
     TRAINER_WALLY_MAUVILLE, TRAINER_WALLY_MAUVILLE, TRAINER_WALLY_MAUVILLE},
    {TRAINER_WALLY_VR_1, TRAINER_WALLY_VR_1, TRAINER_WALLY_VR_1,
     TRAINER_WALLY_VR_1, TRAINER_WALLY_VR_1, TRAINER_WALLY_VR_1},
};

static const u16 sEggShopSlotVars[EGG_SHOP_SLOTS] =
{
    VAR_EGG_SHOP_SLOT_1,
    VAR_EGG_SHOP_SLOT_2,
    VAR_EGG_SHOP_SLOT_3,
};

// Badges + rival battles + Hall of Fame entries. The stock rolls again whenever this changes, so it is
// refreshed lazily the next time the shop is opened rather than hooked into every one of those scripts.
u32 GetEggShopMilestones(void)
{
    u32 count = 0;
    u32 i, j;

    for (i = 0; i < NUM_BADGES; i++)
    {
        if (FlagGet(FLAG_BADGE01_GET + i))
            count++;
    }
    for (i = 0; i < ARRAY_COUNT(sRivalBattles); i++)
    {
        for (j = 0; j < ARRAY_COUNT(sRivalBattles[0]); j++)
        {
            if (HasTrainerBeenFought(sRivalBattles[i][j]))
            {
                count++;
                break;
            }
        }
    }
    count += min(GetGameStat(GAME_STAT_ENTERED_HOF), 0xF000);
    return count;
}

// Regional forms belong to their form's region; everything else to the region its National Dex number debuted in.
static enum Region GetEggShopRegion(enum Species species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    u32 dexNum = info->natDexNum;

    if (info->isAlolanForm)
        return REGION_ALOLA;
    if (info->isGalarianForm)
        return REGION_GALAR;
    if (info->isHisuianForm || species == SPECIES_BASCULIN_WHITE_STRIPED)
        return REGION_HISUI;
    if (info->isPaldeanForm)
        return REGION_PALDEA;
    if (dexNum >= NATIONAL_DEX_SPRIGATITO)
        return REGION_PALDEA;
    if (dexNum >= NATIONAL_DEX_WYRDEER)
        return REGION_HISUI;
    if (dexNum >= NATIONAL_DEX_GROOKEY)
        return REGION_GALAR;
    if (dexNum >= NATIONAL_DEX_ROWLET)
        return REGION_ALOLA;
    if (dexNum >= NATIONAL_DEX_CHESPIN)
        return REGION_KALOS;
    if (dexNum >= NATIONAL_DEX_VICTINI)
        return REGION_UNOVA;
    if (dexNum >= NATIONAL_DEX_TURTWIG)
        return REGION_SINNOH;
    if (dexNum >= NATIONAL_DEX_TREECKO)
        return REGION_HOENN;
    if (dexNum >= NATIONAL_DEX_CHIKORITA)
        return REGION_JOHTO;
    return REGION_KANTO;
}

// An Egg can only hold what vanilla breeding gives: a species in a real Egg Group, or a baby that evolves into one.
static bool32 CanHatchFromEgg(enum Species species)
{
    if (gSpeciesInfo[species].eggGroups[0] != EGG_GROUP_NO_EGGS_DISCOVERED)
        return TRUE;

    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    if (evolutions == NULL)
        return FALSE;
    for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
    {
        enum Species target = SanitizeSpeciesId(evolutions[i].targetSpecies);
        if (gSpeciesInfo[target].eggGroups[0] != EGG_GROUP_NO_EGGS_DISCOVERED)
            return TRUE;
    }
    return FALSE;
}

static bool32 IsEggShopCandidate(enum Species species, enum Region region)
{
    if (!IsSpeciesEnabled(species) || !CanHatchFromEgg(species) || IsInHoennFamily(species, 3))
        return FALSE;
    return region == REGIONS_COUNT || GetEggShopRegion(species) == region;
}

// REGIONS_COUNT = any region.
static u32 CountEggShopCandidates(enum EggShopTier tier, enum Region region)
{
    u32 count = 0;

    for (u32 i = 0; i < sEggShopPools[tier].count; i++)
    {
        if (IsEggShopCandidate(sEggShopPools[tier].species[i], region))
            count++;
    }
    return count;
}

static enum Species PickEggShopSpecies(enum EggShopTier tier, enum Region region)
{
    u32 count = CountEggShopCandidates(tier, region);

    if (count == 0)
        return SPECIES_NONE;
    count = Random() % count;
    for (u32 i = 0; i < sEggShopPools[tier].count; i++)
    {
        if (IsEggShopCandidate(sEggShopPools[tier].species[i], region) && count-- == 0)
            return sEggShopPools[tier].species[i];
    }
    return SPECIES_NONE;
}

static bool32 IsEggShopSlotInStock(u32 slot)
{
    return (VarGet(sEggShopSlotVars[slot]) & EGG_SHOP_IN_STOCK) != 0;
}

static enum EggShopTier GetEggShopSlotTier(u32 slot)
{
    return VarGet(sEggShopSlotVars[slot]) & EGG_SHOP_TIER_MASK;
}

static enum Region GetEggShopSlotRegion(u32 slot)
{
    return (VarGet(sEggShopSlotVars[slot]) >> EGG_SHOP_REGION_SHIFT) & EGG_SHOP_REGION_MASK;
}

// One Egg per tier, slot = tier. Only the region is rolled, evenly among the regions with something in the tier;
// the species is rolled evenly within that region when the Egg is bought.
static void RestockEggShop(void)
{
    for (enum EggShopTier tier = 0; tier < EGG_SHOP_TIER_COUNT; tier++)
    {
        enum Region regions[REGIONS_COUNT];
        u32 count = 0;

        for (enum Region region = 0; region < REGIONS_COUNT; region++)
        {
            if (CountEggShopCandidates(tier, region) != 0)
                regions[count++] = region;
        }
        if (count == 0)
            VarSet(sEggShopSlotVars[tier], 0);
        else
            VarSet(sEggShopSlotVars[tier], EGG_SHOP_IN_STOCK | (regions[Random() % count] << EGG_SHOP_REGION_SHIFT) | tier);
    }
}

static u32 GetEggShopSlot(void)
{
    return gSpecialVar_0x8004 < EGG_SHOP_SLOTS ? gSpecialVar_0x8004 : 0;
}

// Buffers "<REGION> <TIER> EGG".
static void BufferEggShopSlotName(u8 *dest, u32 slot)
{
    enum Region region = GetEggShopSlotRegion(slot);

    if (region >= REGIONS_COUNT)
        region = REGION_NONE;
    dest = StringCopy(dest, sEggShopRegionNames[region]);
    *dest++ = CHAR_SPACE;
    dest = StringCopy(dest, sEggShopTierNames[GetEggShopSlotTier(slot)]);
    StringCopy(dest, sText_Egg);
}

// Restocks if a badge / rival battle / Elite Four clear happened since the last stock. Returns how many Eggs are left.
u16 EggShop_Restock(void)
{
    u32 key = GetEggShopMilestones() + 1;
    u32 count = 0;

    if (VarGet(VAR_EGG_SHOP_STOCK_KEY) != key)
    {
        RestockEggShop();
        VarSet(VAR_EGG_SHOP_STOCK_KEY, key);
    }
    for (u32 slot = 0; slot < EGG_SHOP_SLOTS; slot++)
    {
        if (IsEggShopSlotInStock(slot))
            count++;
    }
    return count;
}

// Buffers the three menu rows ("<REGION> <TIER> EGG" or "SOLD OUT") in STR_VAR_1-3.
void EggShop_BufferMenu(void)
{
    u8 *const rows[EGG_SHOP_SLOTS] = {gStringVar1, gStringVar2, gStringVar3};

    for (u32 slot = 0; slot < EGG_SHOP_SLOTS; slot++)
    {
        if (IsEggShopSlotInStock(slot))
            BufferEggShopSlotName(rows[slot], slot);
        else
            StringCopy(rows[slot], sText_SoldOut);
    }
}

// Buffers the slot's name (STR_VAR_1) and price (STR_VAR_2). FALSE if it is sold out.
bool16 EggShop_BufferOffer(void)
{
    u32 slot = GetEggShopSlot();

    if (!IsEggShopSlotInStock(slot))
        return FALSE;
    BufferEggShopSlotName(gStringVar1, slot);
    ConvertIntToDecimalStringN(gStringVar2, sEggShopPrices[GetEggShopSlotTier(slot)], STR_CONV_MODE_LEFT_ALIGN, 6);
    return TRUE;
}

bool16 EggShop_CanAfford(void)
{
    return IsEnoughMoney(&gSaveBlock1Ptr->money, sEggShopPrices[GetEggShopSlotTier(GetEggShopSlot())]);
}

// Rolls the species now (so it stays hidden until it hatches), gives the Egg (party, or PC when the party is full)
// and takes the money only if it was given. Returns MON_GIVEN_TO_PARTY / MON_GIVEN_TO_PC / MON_CANT_GIVE.
u16 EggShop_Buy(void)
{
    u32 slot = GetEggShopSlot();
    enum EggShopTier tier = GetEggShopSlotTier(slot);
    enum Species species;
    u32 result;

    if (!IsEggShopSlotInStock(slot) || !IsEnoughMoney(&gSaveBlock1Ptr->money, sEggShopPrices[tier]))
        return MON_CANT_GIVE;
    species = PickEggShopSpecies(tier, GetEggShopSlotRegion(slot));
    if (species == SPECIES_NONE)
        species = PickEggShopSpecies(tier, REGIONS_COUNT);
    if (species == SPECIES_NONE)
        return MON_CANT_GIVE;
    result = ScriptGiveEgg(species);
    if (result != MON_CANT_GIVE)
    {
        RemoveMoney(&gSaveBlock1Ptr->money, sEggShopPrices[tier]);
        VarSet(sEggShopSlotVars[slot], 0);
    }
    return result;
}
