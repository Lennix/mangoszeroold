/*
 * Copyright (C) 2005-2010 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2009-2010 MaNGOSZero <http://github.com/mangoszero/mangoszero/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _SPELLMGR_H
#define _SPELLMGR_H

// For static or at-server-startup loaded spell data
// For more high level function for sSpellStore data

#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "DBCStructure.h"
#include "DBCStores.h"
#include "Database/SQLStorage.h"
#include "DBCStores.h"

#include "Utilities/UnorderedMap.h"

#include <map>

class Player;
class Spell;
struct SpellModifier;

// only used in code
enum SpellCategories
{
    SPELLCATEGORY_HEALTH_MANA_POTIONS = 4,
    SPELLCATEGORY_DEVOUR_MAGIC        = 12
};

enum SpellFamilyNames
{
    SPELLFAMILY_GENERIC     = 0,
    SPELLFAMILY_UNK1        = 1,                            // events, holidays
    // 2 - unused
    SPELLFAMILY_MAGE        = 3,
    SPELLFAMILY_WARRIOR     = 4,
    SPELLFAMILY_WARLOCK     = 5,
    SPELLFAMILY_PRIEST      = 6,
    SPELLFAMILY_DRUID       = 7,
    SPELLFAMILY_ROGUE       = 8,
    SPELLFAMILY_HUNTER      = 9,
    SPELLFAMILY_PALADIN     = 10,
    SPELLFAMILY_SHAMAN      = 11,
    SPELLFAMILY_UNK2        = 12,
    SPELLFAMILY_POTION      = 13,
    // 14 - unused
    SPELLFAMILY_DEATHKNIGHT = 15,
    // 16 - unused
    SPELLFAMILY_UNK3        = 17
};

//Some SpellFamilyFlags
#define SPELLFAMILYFLAG_ROGUE_VANISH            UI64LIT(0x0000000000000800)
#define SPELLFAMILYFLAG_ROGUE_STEALTH           UI64LIT(0x0000000000400000)
#define SPELLFAMILYFLAG_ROGUE_BACKSTAB          UI64LIT(0x0000000000800004)
#define SPELLFAMILYFLAG_ROGUE_SAP               UI64LIT(0x0000000000000080)
#define SPELLFAMILYFLAG_ROGUE_FEINT             UI64LIT(0x0000000008000000)
#define SPELLFAMILYFLAG_ROGUE_KIDNEYSHOT        UI64LIT(0x0000000000200000)
#define SPELLFAMILYFLAG_ROGUE__FINISHING_MOVE   UI64LIT(0x00000009003E0000)

#define SPELLFAMILYFLAG_PALADIN_SEALS           UI64LIT(0x000000000A000200)

// Spell clasification
enum SpellSpecific
{
    SPELL_NORMAL            = 0,
    SPELL_SEAL              = 1,
    SPELL_BLESSING          = 2,
    SPELL_AURA              = 3,
    SPELL_STING             = 4,
    SPELL_CURSE             = 5,
    SPELL_ASPECT            = 6,
    SPELL_TRACKER           = 7,
    SPELL_WARLOCK_ARMOR     = 8,
    SPELL_MAGE_ARMOR        = 9,
    SPELL_ELEMENTAL_SHIELD  = 10,
    SPELL_MAGE_POLYMORPH    = 11,
    SPELL_POSITIVE_SHOUT    = 12,
    SPELL_JUDGEMENT         = 13,
    SPELL_BATTLE_ELIXIR     = 14,
    SPELL_GUARDIAN_ELIXIR   = 15,
    SPELL_FLASK_ELIXIR      = 16,
    //SPELL_PRESENCE          = 17,                         // used in 3.x
    //SPELL_HAND              = 18,                         // used in 3.x
    SPELL_WELL_FED          = 19,
    SPELL_FOOD              = 20,
    SPELL_DRINK             = 21,
    SPELL_FOOD_AND_DRINK    = 22,
};

SpellSpecific GetSpellSpecific(uint32 spellId);

// Different spell properties
inline float GetSpellRadius(SpellRadiusEntry const *radius) { return (radius ? radius->Radius : 0); }
uint32 GetSpellCastTime(SpellEntry const* spellInfo, Spell const* spell = NULL);
inline float GetSpellMinRange(SpellRangeEntry const *range) { return (range ? range->minRange : 0); }
inline float GetSpellMaxRange(SpellRangeEntry const *range) { return (range ? range->maxRange : 0); }
inline uint32 GetSpellRecoveryTime(SpellEntry const *spellInfo) { return spellInfo->RecoveryTime > spellInfo->CategoryRecoveryTime ? spellInfo->RecoveryTime : spellInfo->CategoryRecoveryTime; }
int32 GetSpellDuration(SpellEntry const *spellInfo);
int32 GetSpellMaxDuration(SpellEntry const *spellInfo);

inline bool IsSpellHaveEffect(SpellEntry const *spellInfo, SpellEffects effect)
{
    for(int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if(SpellEffects(spellInfo->Effect[i])==effect)
            return true;
    return false;
}

inline bool IsSpellHaveAura(SpellEntry const *spellInfo, AuraType aura)
{
    for(int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if(AuraType(spellInfo->EffectApplyAuraName[i])==aura)
            return true;
    return false;
}

inline bool IsSpellLastAuraEffect(SpellEntry const *spellInfo, SpellEffectIndex effecIdx)
{
    for(int i = effecIdx+1; i < MAX_EFFECT_INDEX; ++i)
        if(spellInfo->EffectApplyAuraName[i])
            return false;
    return true;
}

bool IsNoStackAuraDueToAura(uint32 spellId_1, SpellEffectIndex effIndex_1, uint32 spellId_2, SpellEffectIndex effIndex_2);

inline bool IsSealSpell(SpellEntry const *spellInfo)
{
    //Collection of all the seal family flags. No other paladin spell has any of those.
    return spellInfo->SpellFamilyName == SPELLFAMILY_PALADIN &&
        ( spellInfo->SpellFamilyFlags & SPELLFAMILYFLAG_PALADIN_SEALS );
}

inline bool IsElementalShield(SpellEntry const *spellInfo)
{
    // family flags 10 (Lightning), 42 (Earth), 37 (Water), proc shield from T2 8 pieces bonus
    return (spellInfo->SpellFamilyFlags & UI64LIT(0x00000000400)) || spellInfo->Id == 23552;
}

int32 CompareAuraRanks(uint32 spellId_1, SpellEffectIndex effIndex_1, uint32 spellId_2, SpellEffectIndex effIndex_2);

// order from less to more strict
bool IsSingleFromSpellSpecificPerTargetPerCaster(SpellSpecific spellSpec1,SpellSpecific spellSpec2);
bool IsSingleFromSpellSpecificSpellRanksPerTarget(SpellSpecific spellSpec1,SpellSpecific spellSpec2);
bool IsSingleFromSpellSpecificPerTarget(SpellSpecific spellSpec1,SpellSpecific spellSpec2);

bool IsPassiveSpell(uint32 spellId);

inline bool IsDeathOnlySpell(SpellEntry const *spellInfo)
{
    return spellInfo->AttributesEx3 & SPELL_ATTR_EX3_CAST_ON_DEAD
        || spellInfo->Id == 2584;
}

inline bool IsDeathPersistentSpell(SpellEntry const *spellInfo)
{
    return spellInfo->AttributesEx3 & SPELL_ATTR_EX3_DEATH_PERSISTENT;
}

inline bool IsNonCombatSpell(SpellEntry const *spellInfo)
{
    return (spellInfo->Attributes & SPELL_ATTR_CANT_USED_IN_COMBAT) != 0;
}

bool IsPositiveSpell(uint32 spellId);
bool IsPositiveEffect(uint32 spellId, SpellEffectIndex effIndex);
bool IsPositiveTarget(uint32 targetA, uint32 targetB);

bool IsExplicitPositiveTarget(uint32 targetA);
bool IsExplicitNegativeTarget(uint32 targetA);

bool IsSingleTargetSpell(SpellEntry const *spellInfo);
bool IsSingleTargetSpells(SpellEntry const *spellInfo1, SpellEntry const *spellInfo2);

inline bool IsCasterSourceTarget(uint32 target)
{
    switch (target )
    {
        case TARGET_SELF:
        case TARGET_PET:
        case TARGET_ALL_PARTY_AROUND_CASTER:
        case TARGET_IN_FRONT_OF_CASTER:
        case TARGET_MASTER:
        case TARGET_MINION:
        case TARGET_ALL_PARTY:
        case TARGET_ALL_PARTY_AROUND_CASTER_2:
        case TARGET_SELF_FISHING:
        case TARGET_TOTEM_EARTH:
        case TARGET_TOTEM_WATER:
        case TARGET_TOTEM_AIR:
        case TARGET_TOTEM_FIRE:
        case TARGET_AREAEFFECT_CUSTOM_2:
        case TARGET_RANDOM_RAID_MEMBER:
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsSpellWithCasterSourceTargetsOnly(SpellEntry const* spellInfo)
{
    for(int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint32 targetA = spellInfo->EffectImplicitTargetA[i];
        if(targetA && !IsCasterSourceTarget(targetA))
            return false;

        uint32 targetB = spellInfo->EffectImplicitTargetB[i];
        if(targetB && !IsCasterSourceTarget(targetB))
            return false;

        if(!targetA && !targetB)
            return false;
    }
    return true;
}

inline bool IsPointEffectTarget( Targets target )
{
    switch (target )
    {
        case TARGET_INNKEEPER_COORDINATES:
        case TARGET_TABLE_X_Y_Z_COORDINATES:
        case TARGET_CASTER_COORDINATES:
        case TARGET_SCRIPT_COORDINATES:
        case TARGET_CURRENT_ENEMY_COORDINATES:
        case TARGET_DUELVSPLAYER_COORDINATES:
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsAreaEffectTarget( Targets target )
{
    switch (target )
    {
        case TARGET_AREAEFFECT_INSTANT:
        case TARGET_AREAEFFECT_CUSTOM:
        case TARGET_ALL_ENEMY_IN_AREA:
        case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
        case TARGET_ALL_PARTY_AROUND_CASTER:
        case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
        case TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER:
        case TARGET_ALL_PARTY:
        case TARGET_ALL_PARTY_AROUND_CASTER_2:
        case TARGET_AREAEFFECT_PARTY:
        case TARGET_AREAEFFECT_CUSTOM_2:
        case TARGET_AREAEFFECT_PARTY_AND_CLASS:
        case TARGET_IN_FRONT_OF_CASTER:
        case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsAreaOfEffectSpell(SpellEntry const *spellInfo)
{
    if(IsAreaEffectTarget(Targets(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_0])) || IsAreaEffectTarget(Targets(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_0])))
        return true;
    if(IsAreaEffectTarget(Targets(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_1])) || IsAreaEffectTarget(Targets(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_1])))
        return true;
    if(IsAreaEffectTarget(Targets(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_2])) || IsAreaEffectTarget(Targets(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_2])))
        return true;
    return false;
}

inline bool IsAreaAuraEffect(uint32 effect)
{
    if( effect == SPELL_EFFECT_APPLY_AREA_AURA_PARTY    ||
        effect == SPELL_EFFECT_APPLY_AREA_AURA_PET)
        return true;
    return false;
}

inline bool IsDispelSpell(SpellEntry const *spellInfo)
{
    if (spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_DISPEL ||
        spellInfo->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_DISPEL ||
        spellInfo->Effect[EFFECT_INDEX_2] == SPELL_EFFECT_DISPEL )
        return true;
    return false;
}
inline bool isSpellBreakStealth(SpellEntry const* spellInfo)
{
    return !(spellInfo->AttributesEx & SPELL_ATTR_EX_NOT_BREAK_STEALTH);
}

SpellCastResult GetErrorAtShapeshiftedCast (SpellEntry const *spellInfo, uint32 form);

inline bool IsChanneledSpell(SpellEntry const* spellInfo)
{
    return (spellInfo->AttributesEx & (SPELL_ATTR_EX_CHANNELED_1 | SPELL_ATTR_EX_CHANNELED_2));
}

inline bool NeedsComboPoints(SpellEntry const* spellInfo)
{
    return (spellInfo->AttributesEx & (SPELL_ATTR_EX_REQ_COMBO_POINTS1 | SPELL_ATTR_EX_REQ_COMBO_POINTS2));
}

inline SpellSchoolMask GetSpellSchoolMask(SpellEntry const* spellInfo)
{
    return GetSchoolMask(spellInfo->School);
}

inline uint32 GetSpellMechanicMask(SpellEntry const* spellInfo, int32 effect)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
        mask |= 1 << (spellInfo->Mechanic - 1);
    if (spellInfo->EffectMechanic[effect])
        mask |= 1 << (spellInfo->EffectMechanic[effect] - 1);
    return mask;
}

inline uint32 GetAllSpellMechanicMask(SpellEntry const* spellInfo)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
        mask |= 1 << (spellInfo->Mechanic - 1);
    for (int i=0; i< MAX_EFFECT_INDEX; ++i)
        if (spellInfo->EffectMechanic[i])
            mask |= 1 << (spellInfo->EffectMechanic[i]-1);
    return mask;
}

inline Mechanics GetEffectMechanic(SpellEntry const* spellInfo, SpellEffectIndex effect)
{
    if (spellInfo->EffectMechanic[effect])
        return Mechanics(spellInfo->EffectMechanic[effect]);
    if (spellInfo->Mechanic)
        return Mechanics(spellInfo->Mechanic);
    return MECHANIC_NONE;
}

inline uint32 GetDispellMask(DispelType dispel)
{
    // If dispell all
    if (dispel == DISPEL_ALL)
        return DISPEL_ALL_MASK;
    else
        return (1 << dispel);
}

// Diminishing Returns interaction with spells
DiminishingGroup GetDiminishingReturnsGroupForSpell(SpellEntry const* spellproto, bool triggered);
bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group);
DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group);

// Spell affects related declarations (accessed using SpellMgr functions)
typedef std::map<uint32, uint64> SpellAffectMap;

// Spell proc event related declarations (accessed using SpellMgr functions)
enum ProcFlags
{
    PROC_FLAG_NONE                  = 0x00000000,           // None
    PROC_FLAG_HIT_MELEE             = 0x00000001,           // On melee hit
    PROC_FLAG_STRUCK_MELEE          = 0x00000002,           // On being struck melee
    PROC_FLAG_KILL_XP_GIVER         = 0x00000004,           // On kill target giving XP or honor
    PROC_FLAG_SPECIAL_DROP          = 0x00000008,           //
    PROC_FLAG_DODGE                 = 0x00000010,           // On dodge melee attack
    PROC_FLAG_PARRY                 = 0x00000020,           // On parry melee attack
    PROC_FLAG_BLOCK                 = 0x00000040,           // On block attack
    PROC_FLAG_TOUCH                 = 0x00000080,           // On being touched (for bombs, probably?)
    PROC_FLAG_TARGET_LOW_HEALTH     = 0x00000100,           // On deal damage to enemy with 20% or less health
    PROC_FLAG_LOW_HEALTH            = 0x00000200,           // On health dropped below 20%
    PROC_FLAG_STRUCK_RANGED         = 0x00000400,           // On being struck ranged
    PROC_FLAG_PERIODIC_TICK         = 0x00000800,           // On periodic tick
    PROC_FLAG_CRIT_MELEE            = 0x00001000,           // On crit melee
    PROC_FLAG_STRUCK_CRIT_MELEE     = 0x00002000,           // On being critically struck in melee
    PROC_FLAG_CAST_SPELL            = 0x00004000,           // On cast spell
    PROC_FLAG_TAKE_DAMAGE           = 0x00008000,           // On take damage
    PROC_FLAG_CRIT_SPELL            = 0x00010000,           // On crit spell
    PROC_FLAG_HIT_SPELL             = 0x00020000,           // On hit spell
    PROC_FLAG_STRUCK_CRIT_SPELL     = 0x00040000,           // On being critically struck by a spell
    PROC_FLAG_HIT_RANGED            = 0x00080000,           // On getting ranged hit
    PROC_FLAG_STRUCK_SPELL          = 0x00100000,           // On being struck by a spell
    PROC_FLAG_TRAP                  = 0x00200000,           // On trap activation (?)
    PROC_FLAG_CRIT_RANGED           = 0x00400000,           // On getting ranged crit
    PROC_FLAG_STRUCK_CRIT_RANGED    = 0x00800000,           // On being critically struck by a ranged attack
    PROC_FLAG_RESIST_SPELL          = 0x01000000,           // On resist enemy spell
    PROC_FLAG_TARGET_RESISTS        = 0x02000000,           // On enemy resisted spell
    PROC_FLAG_TARGET_DODGE_OR_PARRY = 0x04000000,           // On enemy dodges/parries
    PROC_FLAG_HEAL                  = 0x08000000,           // On heal
    PROC_FLAG_CRIT_HEAL             = 0x10000000,           // On critical healing effect
    PROC_FLAG_HEALED                = 0x20000000,           // On healing
    PROC_FLAG_TARGET_BLOCK          = 0x40000000,           // On enemy blocks
    PROC_FLAG_MISS                  = 0x80000000            // On miss melee attack
};

struct SpellProcEventEntry
{
    uint32      schoolMask;                                 // if nonzero - bit mask for matching proc condition based on spell candidate's school: Fire=2, Mask=1<<(2-1)=2
    uint32      category;                                   // if nonzero - match proc condition based on candidate spell's category
    uint32      skillId;                                    // if nonzero - for matching proc condition based on candidate spell's skillId from SkillLineAbility.dbc (Shadow Bolt = Destruction)
    uint32      spellFamilyName;                            // if nonzero - for matching proc condition based on candidate spell's SpellFamilyNamer value
    uint64      spellFamilyMask;                            // if nonzero - for matching proc condition based on candidate spell's SpellFamilyFlags (like auras 107 and 108 do)
    uint32      procFlags;                                  // bitmask for matching proc event
    float       ppmRate;                                    // for melee (ranged?) damage spells - proc rate per minute. if zero, falls back to flat chance from Spell.dbc
    uint32      cooldown;                                   // hidden cooldown used for some spell proc events, applied to _triggered_spell_
};

typedef UNORDERED_MAP<uint32, SpellProcEventEntry> SpellProcEventMap;

#define ELIXIR_FLASK_MASK     0x03                          // 2 bit mask for batter compatibility with more recent client version, flaks must have both bits set
#define ELIXIR_WELL_FED       0x10                          // Some foods have SPELLFAMILY_POTION

typedef std::map<uint32, uint8> SpellElixirMap;
typedef std::map<uint32, uint16> SpellThreatMap;

// Spell script target related declarations (accessed using SpellMgr functions)
enum SpellTargetType
{
    SPELL_TARGET_TYPE_GAMEOBJECT = 0,
    SPELL_TARGET_TYPE_CREATURE   = 1,
    SPELL_TARGET_TYPE_DEAD       = 2
};

#define MAX_SPELL_TARGET_TYPE 3

struct SpellTargetEntry
{
    SpellTargetEntry(SpellTargetType type_,uint32 targetEntry_) : type(type_), targetEntry(targetEntry_) {}
    SpellTargetType type;
    uint32 targetEntry;
};

typedef std::multimap<uint32,SpellTargetEntry> SpellScriptTarget;
typedef std::pair<SpellScriptTarget::const_iterator,SpellScriptTarget::const_iterator> SpellScriptTargetBounds;

// coordinates for spells (accessed using SpellMgr functions)
struct SpellTargetPosition
{
    uint32 target_mapId;
    float  target_X;
    float  target_Y;
    float  target_Z;
    float  target_Orientation;
};

typedef UNORDERED_MAP<uint32, SpellTargetPosition> SpellTargetPositionMap;

// Spell pet auras
class PetAura
{
    public:
        PetAura()
        {
            auras.clear();
        }

        PetAura(uint32 petEntry, uint32 aura, bool _removeOnChangePet, int _damage) :
        removeOnChangePet(_removeOnChangePet), damage(_damage)
        {
            auras[petEntry] = aura;
        }

        uint32 GetAura(uint32 petEntry) const
        {
            std::map<uint32, uint32>::const_iterator itr = auras.find(petEntry);
            if(itr != auras.end())
                return itr->second;
            else
            {
                std::map<uint32, uint32>::const_iterator itr2 = auras.find(0);
                if(itr2 != auras.end())
                    return itr2->second;
                else
                    return 0;
            }
        }

        void AddAura(uint32 petEntry, uint32 aura)
        {
            auras[petEntry] = aura;
        }

        bool IsRemovedOnChangePet() const
        {
            return removeOnChangePet;
        }

        int32 GetDamage() const
        {
            return damage;
        }

    private:
        std::map<uint32, uint32> auras;
        bool removeOnChangePet;
        int32 damage;
};
typedef std::map<uint16, PetAura> SpellPetAuraMap;

struct SpellArea
{
    uint32 spellId;
    uint32 areaId;                                          // zone/subzone/or 0 is not limited to zone
    uint32 questStart;                                      // quest start (quest must be active or rewarded for spell apply)
    uint32 questEnd;                                        // quest end (quest don't must be rewarded for spell apply)
    int32  auraSpell;                                       // spell aura must be applied for spell apply )if possitive) and it don't must be applied in other case
    uint32 raceMask;                                        // can be applied only to races
    Gender gender;                                          // can be applied only to gender
    bool questStartCanActive;                               // if true then quest start can be active (not only rewarded)
    bool autocast;                                          // if true then auto applied at area enter, in other case just allowed to cast

    // helpers
    bool IsFitToRequirements(Player const* player, uint32 newZone, uint32 newArea) const;
};

typedef std::multimap<uint32,SpellArea> SpellAreaMap;
typedef std::multimap<uint32,SpellArea const*> SpellAreaForQuestMap;
typedef std::multimap<uint32,SpellArea const*> SpellAreaForAuraMap;
typedef std::multimap<uint32,SpellArea const*> SpellAreaForAreaMap;
typedef std::pair<SpellAreaMap::const_iterator,SpellAreaMap::const_iterator> SpellAreaMapBounds;
typedef std::pair<SpellAreaForQuestMap::const_iterator,SpellAreaForQuestMap::const_iterator> SpellAreaForQuestMapBounds;
typedef std::pair<SpellAreaForAuraMap::const_iterator, SpellAreaForAuraMap::const_iterator>  SpellAreaForAuraMapBounds;
typedef std::pair<SpellAreaForAreaMap::const_iterator, SpellAreaForAreaMap::const_iterator>  SpellAreaForAreaMapBounds;


// Spell rank chain  (accessed using SpellMgr functions)
struct SpellChainNode
{
    uint32 prev;
    uint32 first;
    uint32 req;
    uint8  rank;
};

typedef UNORDERED_MAP<uint32, SpellChainNode> SpellChainMap;
typedef std::multimap<uint32, uint32> SpellChainMapNext;

// Spell learning properties (accessed using SpellMgr functions)
struct SpellLearnSkillNode
{
    uint16 skill;
    uint16 step;
    uint16 value;                                           // 0  - max skill value for player level
    uint16 maxvalue;                                        // 0  - max skill value for player level
};

typedef std::map<uint32, SpellLearnSkillNode> SpellLearnSkillMap;

struct SpellLearnSpellNode
{
    uint32 spell;
    bool active;                                            // show in spellbook or not
    bool autoLearned;
};

typedef std::multimap<uint32, SpellLearnSpellNode> SpellLearnSpellMap;
typedef std::pair<SpellLearnSpellMap::const_iterator,SpellLearnSpellMap::const_iterator> SpellLearnSpellMapBounds;

typedef std::multimap<uint32, SkillLineAbilityEntry const*> SkillLineAbilityMap;
typedef std::pair<SkillLineAbilityMap::const_iterator,SkillLineAbilityMap::const_iterator> SkillLineAbilityMapBounds;

inline bool IsPrimaryProfessionSkill(uint32 skill)
{
    SkillLineEntry const *pSkill = sSkillLineStore.LookupEntry(skill);
    if(!pSkill)
        return false;

    if(pSkill->categoryId != SKILL_CATEGORY_PROFESSION)
        return false;

    return true;
}

inline bool IsProfessionSkill(uint32 skill)
{
    return  IsPrimaryProfessionSkill(skill) || skill == SKILL_FISHING || skill == SKILL_COOKING || skill == SKILL_FIRST_AID;
}

inline bool IsProfessionOrRidingSkill(uint32 skill)
{
    return  IsProfessionSkill(skill) || skill == SKILL_RIDING;
}

typedef std::map<uint32, uint32> SpellFacingFlagMap;

class SpellMgr
{
    // Constructors
    public:
        SpellMgr();
        ~SpellMgr();

    // Accessors (const or static functions)
    public:
        // Spell affects
        uint64 GetSpellAffectMask(uint32 spellId, SpellEffectIndex effectId) const
        {
            SpellAffectMap::const_iterator itr = mSpellAffectMap.find((spellId<<8) + effectId);
            if( itr != mSpellAffectMap.end( ) )
                return itr->second;
            return 0;
        }

        SpellElixirMap const& GetSpellElixirMap() const { return mSpellElixirs; }

        uint32 GetSpellElixirMask(uint32 spellid) const
        {
            SpellElixirMap::const_iterator itr = mSpellElixirs.find(spellid);
            if(itr==mSpellElixirs.end())
                return 0x0;

            return itr->second;
        }

        SpellSpecific GetSpellElixirSpecific(uint32 spellid) const
        {
            uint32 mask = GetSpellElixirMask(spellid);

            // flasks must have all bits set from ELIXIR_FLASK_MASK
            if((mask & ELIXIR_FLASK_MASK)==ELIXIR_FLASK_MASK)
                return SPELL_FLASK_ELIXIR;
            else if(mask & ELIXIR_WELL_FED)
                return SPELL_WELL_FED;
            else
                return SPELL_NORMAL;
        }

        uint16 GetSpellThreat(uint32 spellid) const
        {
            SpellThreatMap::const_iterator itr = mSpellThreatMap.find(spellid);
            if(itr==mSpellThreatMap.end())
                return 0;

            return itr->second;
        }

        // Spell proc events
        SpellProcEventEntry const* GetSpellProcEvent(uint32 spellId) const
        {
            SpellProcEventMap::const_iterator itr = mSpellProcEventMap.find(spellId);
            if( itr != mSpellProcEventMap.end( ) )
                return &itr->second;
            return NULL;
        }

        uint32 GetSpellFacingFlag(uint32 spellId) const
        {
            SpellFacingFlagMap::const_iterator itr =  mSpellFacingFlagMap.find(spellId);
            if(itr != mSpellFacingFlagMap.end())
                return itr->second;
            return 0x0;
        }

        static bool IsSpellProcEventCanTriggeredBy( SpellProcEventEntry const * spellProcEvent, SpellEntry const * procSpell, uint32 procFlags );

        // Spell target coordinates
        SpellTargetPosition const* GetSpellTargetPosition(uint32 spell_id) const
        {
            SpellTargetPositionMap::const_iterator itr = mSpellTargetPositions.find( spell_id );
            if( itr != mSpellTargetPositions.end( ) )
                return &itr->second;
            return NULL;
        }

        // Spell ranks chains
        SpellChainNode const* GetSpellChainNode(uint32 spell_id) const
        {
            SpellChainMap::const_iterator itr = mSpellChains.find(spell_id);
            if(itr == mSpellChains.end())
                return NULL;

            return &itr->second;
        }

        uint32 GetFirstSpellInChain(uint32 spell_id) const
        {
            if(SpellChainNode const* node = GetSpellChainNode(spell_id))
                return node->first;

            return spell_id;
        }

        uint32 GetPrevSpellInChain(uint32 spell_id) const
        {
            if(SpellChainNode const* node = GetSpellChainNode(spell_id))
                return node->prev;

            return 0;
        }

        SpellChainMapNext const& GetSpellChainNext() const { return mSpellChainsNext; }

        // Note: not use rank for compare to spell ranks: spell chains isn't linear order
        // Use IsHighRankOfSpell instead
        uint8 GetSpellRank(uint32 spell_id) const
        {
            if(SpellChainNode const* node = GetSpellChainNode(spell_id))
                return node->rank;

            return 0;
        }

        uint8 IsHighRankOfSpell(uint32 spell1,uint32 spell2) const
        {
            SpellChainMap::const_iterator itr = mSpellChains.find(spell1);

            uint32 rank2 = GetSpellRank(spell2);

            // not ordered correctly by rank value
            if(itr == mSpellChains.end() || !rank2 || itr->second.rank <= rank2)
                return false;

            // check present in same rank chain
            for(; itr != mSpellChains.end(); itr = mSpellChains.find(itr->second.prev))
                if(itr->second.prev==spell2)
                    return true;

            return false;
        }

        bool IsRankSpellDueToSpell(SpellEntry const *spellInfo_1,uint32 spellId_2) const;
        static bool canStackSpellRanks(SpellEntry const *spellInfo);
        bool IsNoStackSpellDueToSpell(uint32 spellId_1, uint32 spellId_2) const;

        SpellEntry const* SelectAuraRankForPlayerLevel(SpellEntry const* spellInfo, uint32 playerLevel) const;

        // Spell learning
        SpellLearnSkillNode const* GetSpellLearnSkill(uint32 spell_id) const
        {
            SpellLearnSkillMap::const_iterator itr = mSpellLearnSkills.find(spell_id);
            if(itr != mSpellLearnSkills.end())
                return &itr->second;
            else
                return NULL;
        }

        bool IsSpellLearnSpell(uint32 spell_id) const
        {
            return mSpellLearnSpells.find(spell_id) != mSpellLearnSpells.end();
        }

        SpellLearnSpellMapBounds GetSpellLearnSpellMapBounds(uint32 spell_id) const
        {
            return SpellLearnSpellMapBounds(mSpellLearnSpells.lower_bound(spell_id),mSpellLearnSpells.upper_bound(spell_id));
        }

        bool IsSpellLearnToSpell(uint32 spell_id1,uint32 spell_id2) const
        {
            SpellLearnSpellMapBounds bounds = GetSpellLearnSpellMapBounds(spell_id1);
            for(SpellLearnSpellMap::const_iterator i = bounds.first; i != bounds.second; ++i)
                if (i->second.spell==spell_id2)
                    return true;
            return false;
        }

        static bool IsProfessionOrRidingSpell(uint32 spellId);
        static bool IsProfessionSpell(uint32 spellId);
        static bool IsPrimaryProfessionSpell(uint32 spellId);
        bool IsPrimaryProfessionFirstRankSpell(uint32 spellId) const;

        // Spell script targets
        SpellScriptTargetBounds GetSpellScriptTargetBounds(uint32 spell_id) const
        {
            return SpellScriptTargetBounds(mSpellScriptTarget.lower_bound(spell_id),mSpellScriptTarget.upper_bound(spell_id));
        }

        // Spell correctess for client using
        static bool IsSpellValid(SpellEntry const * spellInfo, Player* pl = NULL, bool msg = true);

        SkillLineAbilityMapBounds GetSkillLineAbilityMapBounds(uint32 spell_id) const
        {
            return SkillLineAbilityMapBounds(mSkillLineAbilityMap.lower_bound(spell_id),mSkillLineAbilityMap.upper_bound(spell_id));
        }

        PetAura const* GetPetAura(uint32 spell_id)
        {
            SpellPetAuraMap::const_iterator itr = mSpellPetAuraMap.find(spell_id);
            if(itr != mSpellPetAuraMap.end())
                return &itr->second;
            else
                return NULL;
        }

        SpellCastResult GetSpellAllowedInLocationError(SpellEntry const *spellInfo, uint32 map_id, uint32 zone_id, uint32 area_id, Player const* player = NULL);

        SpellAreaMapBounds GetSpellAreaMapBounds(uint32 spell_id) const
        {
            return SpellAreaMapBounds(mSpellAreaMap.lower_bound(spell_id),mSpellAreaMap.upper_bound(spell_id));
        }

        SpellAreaForQuestMapBounds GetSpellAreaForQuestMapBounds(uint32 quest_id, bool active) const
        {
            if(active)
                return SpellAreaForQuestMapBounds(mSpellAreaForActiveQuestMap.lower_bound(quest_id),mSpellAreaForActiveQuestMap.upper_bound(quest_id));
            else
                return SpellAreaForQuestMapBounds(mSpellAreaForQuestMap.lower_bound(quest_id),mSpellAreaForQuestMap.upper_bound(quest_id));
        }

        SpellAreaForQuestMapBounds GetSpellAreaForQuestEndMapBounds(uint32 quest_id) const
        {
            return SpellAreaForQuestMapBounds(mSpellAreaForQuestEndMap.lower_bound(quest_id),mSpellAreaForQuestEndMap.upper_bound(quest_id));
        }

        SpellAreaForAuraMapBounds GetSpellAreaForAuraMapBounds(uint32 spell_id) const
        {
            return SpellAreaForAuraMapBounds(mSpellAreaForAuraMap.lower_bound(spell_id),mSpellAreaForAuraMap.upper_bound(spell_id));
        }

        SpellAreaForAreaMapBounds GetSpellAreaForAreaMapBounds(uint32 area_id) const
        {
            return SpellAreaForAreaMapBounds(mSpellAreaForAreaMap.lower_bound(area_id),mSpellAreaForAreaMap.upper_bound(area_id));
        }

    // Modifiers
    public:
        static SpellMgr& Instance();

        void CheckUsedSpells(char const* table);

        // Loading data at server startup
        void LoadSpellChains();
        void LoadSpellLearnSkills();
        void LoadSpellLearnSpells();
        void LoadSpellScriptTarget();
        void LoadSpellAffects();
        void LoadSpellElixirs();
        void LoadSpellProcEvents();
        void LoadSpellTargetPositions();
        void LoadSpellThreats();
        void LoadSkillLineAbilityMap();
        void LoadSpellPetAuras();
        void LoadSpellAreas();
        void LoadFacingCasterFlags();

    private:
        SpellScriptTarget  mSpellScriptTarget;
        SpellChainMap      mSpellChains;
        SpellChainMapNext  mSpellChainsNext;
        SpellLearnSkillMap mSpellLearnSkills;
        SpellLearnSpellMap mSpellLearnSpells;
        SpellTargetPositionMap mSpellTargetPositions;
        SpellAffectMap     mSpellAffectMap;
        SpellElixirMap     mSpellElixirs;
        SpellThreatMap     mSpellThreatMap;
        SpellProcEventMap  mSpellProcEventMap;
        SkillLineAbilityMap mSkillLineAbilityMap;
        SpellPetAuraMap     mSpellPetAuraMap;
        SpellAreaMap         mSpellAreaMap;
        SpellAreaForQuestMap mSpellAreaForQuestMap;
        SpellAreaForQuestMap mSpellAreaForActiveQuestMap;
        SpellAreaForQuestMap mSpellAreaForQuestEndMap;
        SpellAreaForAuraMap  mSpellAreaForAuraMap;
        SpellAreaForAreaMap  mSpellAreaForAreaMap;
        SpellFacingFlagMap  mSpellFacingFlagMap;
};

#define sSpellMgr SpellMgr::Instance()
#endif
