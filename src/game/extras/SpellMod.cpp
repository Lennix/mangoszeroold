/*
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

#include "Mod.h"
#include "../Spell.h"
#include "../SpellMgr.h"
#include "../SpellAuras.h"


void ModClass::spellPrepare(Spell* spell,Unit* caster)
{
    // nothing to do for now
}

void ModClass::spellEffect(Spell *spell, uint8 eff, SpellEffectIndex i)
{
    // nothing to do for now
}

void ModClass::auraApplyModifier(Aura *aura, AuraType aType, bool apply, bool real)
{
    // nothing to do for now
}

void ModClass::applyDiminishingToDuration(Unit *unit, Unit *caster, int32 &duration, DiminishingGroup group)
{
    // [MOD] Duration of crowd control abilities on pvp target is limited by 10 sec. (after patch 2.2.0)
    if(duration > 16*IN_MILLISECONDS && IsDiminishingReturnsGroupDurationLimited(group))
    {
        // test pet/charm masters instead pets/charmeds
        Unit const* targetOwner = unit->GetCharmerOrOwner();
        Unit const* casterOwner = caster->GetCharmerOrOwner();

        Unit const* target = targetOwner ? targetOwner : unit;
        Unit const* source = casterOwner ? casterOwner : caster;

        if(target->GetTypeId() == TYPEID_PLAYER && source->GetTypeId() == TYPEID_PLAYER)
            duration = 16000;
    }
}

void ModClass::getSpellDuration(const SpellEntry *spellInfo, SpellEffectIndex effIndex,int32 &duration)
{
	if(spellInfo->Id == 25198)
		duration = 90000;
}

void ModClass::getSpellCastTime(const SpellEntry *spellInfo, const Spell *spell,int32 &castTime)
{
    // [workaround] holy light need script effect, but 19968 spell for it have 2.5 cast time sec
    // it should be instant instead
    if(spellInfo->Id == 19968) 
        castTime = 0;
}

bool ModClass::isAffectedOnSpell(SpellEntry const *spell, uint32 spellId, SpellEffectIndex effectId)
{
	switch (spell->Id) { // Scorch
		case 2948:
		case 8444:
		case 8445:
		case 8446:
		case 10205:
		case 10206:
		case 10207:
			switch(spellId) { // Improved Scorch
				case 11095:
				case 12872:
				case 12873:
					return true;
			}
	}
	return false;
}
