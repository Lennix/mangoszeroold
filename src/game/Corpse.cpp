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

#include "Corpse.h"
#include "Player.h"
#include "UpdateMask.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "GossipDef.h"
#include "World.h"
#include "ObjectMgr.h"

Corpse::Corpse(CorpseType type) : WorldObject()
{
    m_objectType |= TYPEMASK_CORPSE;
    m_objectTypeId = TYPEID_CORPSE;
    m_updateFlag = (UPDATEFLAG_TRANSPORT | UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);

    m_valuesCount = CORPSE_END;

    m_type = type;

    m_time = time(NULL);

    lootForBody = false;
}

Corpse::~Corpse()
{
}

void Corpse::AddToWorld()
{
    ///- Register the corpse for guid lookup
    if(!IsInWorld())
        sObjectAccessor.AddObject(this);

    Object::AddToWorld();
}

void Corpse::RemoveFromWorld()
{
    ///- Remove the corpse from the accessor
    if(IsInWorld())
        sObjectAccessor.RemoveObject(this);

    Object::RemoveFromWorld();
}

bool Corpse::Create( uint32 guidlow )
{
    Object::_Create(guidlow, 0, HIGHGUID_CORPSE);
    return true;
}

bool Corpse::Create( uint32 guidlow, Player *owner)
{
    MANGOS_ASSERT(owner);

    WorldObject::_Create(guidlow, HIGHGUID_CORPSE);
    Relocate(owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ(), owner->GetOrientation());

    //we need to assign owner's map for corpse
    //in other way we will get a crash in Corpse::SaveToDB()
    SetMap(owner->GetMap());

    if(!IsPositionValid())
    {
        sLog.outError("Corpse (guidlow %d, owner %s) not created. Suggested coordinates isn't valid (X: %f Y: %f)",
            guidlow, owner->GetName(), owner->GetPositionX(), owner->GetPositionY());
        return false;
    }

    SetObjectScale(DEFAULT_OBJECT_SCALE);
    SetFloatValue( CORPSE_FIELD_POS_X, GetPositionX() );
    SetFloatValue( CORPSE_FIELD_POS_Y, GetPositionY() );
    SetFloatValue( CORPSE_FIELD_POS_Z, GetPositionZ() );
    SetFloatValue( CORPSE_FIELD_FACING, GetOrientation() );
    SetUInt64Value( CORPSE_FIELD_OWNER, owner->GetGUID() );

    m_grid = MaNGOS::ComputeGridPair(GetPositionX(), GetPositionY());

    return true;
}

void Corpse::SaveToDB()
{
    // bones should not be saved to DB (would be deleted on startup anyway)
    MANGOS_ASSERT(GetType() != CORPSE_BONES);

    // prevent DB data inconsistence problems and duplicates
    CharacterDatabase.BeginTransaction();
    DeleteFromDB();

    std::ostringstream ss;
    ss  << "INSERT INTO corpse (guid,player,position_x,position_y,position_z,orientation,zone,map,data,time,corpse_type,instance) VALUES ("
        << GetGUIDLow() << ", "
        << GUID_LOPART(GetOwnerGUID()) << ", "
        << GetPositionX() << ", "
        << GetPositionY() << ", "
        << GetPositionZ() << ", "
        << GetOrientation() << ", "
        << GetZoneId() << ", "
        << GetMapId() << ", '";
    for(uint16 i = 0; i < m_valuesCount; ++i)
        ss << GetUInt32Value(i) << " ";
    ss  << "',"
        << uint64(m_time) <<", "
        << uint32(GetType()) << ", "
        << int(GetInstanceId()) << ")";
    CharacterDatabase.Execute( ss.str().c_str() );
    CharacterDatabase.CommitTransaction();
}

void Corpse::DeleteBonesFromWorld()
{
    MANGOS_ASSERT(GetType() == CORPSE_BONES);
    Corpse* corpse = GetMap()->GetCorpse(GetGUID());

    if (!corpse)
    {
        sLog.outError("Bones %u not found in world.", GetGUIDLow());
        return;
    }

    AddObjectToRemoveList();
}

void Corpse::DeleteFromDB()
{
    // bones should not be saved to DB (would be deleted on startup anyway)
    MANGOS_ASSERT(GetType() != CORPSE_BONES);

    // all corpses (not bones)
    CharacterDatabase.PExecute("DELETE FROM corpse WHERE player = '%u' AND corpse_type <> '0'",  GUID_LOPART(GetOwnerGUID()));
}

bool Corpse::LoadFromDB(uint32 lowguid, QueryResult *result)
{
    bool external = (result != NULL);
    if (!external)
        //                                        0          1          2          3           4   5    6    7           8
        result = CharacterDatabase.PQuery("SELECT position_x,position_y,position_z,orientation,map,data,time,corpse_type,instance FROM corpse WHERE guid = '%u'", lowguid);

    if( !result )
    {
        sLog.outError("Corpse (GUID: %u) not found in table `corpse`, can't load. ", lowguid);
        return false;
    }

    Field *fields = result->Fetch();

    if (!LoadFromDB(lowguid, fields))
    {
        if (!external)
            delete result;

        return false;
    }

    if (!external)
        delete result;

    return true;
}

bool Corpse::LoadFromDB(uint32 lowguid, Field *fields)
{
    //                                          0          1          2          3           4   5    6    7           8
    //result = CharacterDatabase.PQuery("SELECT position_x,position_y,position_z,orientation,map,data,time,corpse_type,instance FROM corpse WHERE guid = '%u'",lowguid);
    float positionX = fields[0].GetFloat();
    float positionY = fields[1].GetFloat();
    float positionZ = fields[2].GetFloat();
    float ort       = fields[3].GetFloat();
    uint32 mapid    = fields[4].GetUInt32();

    Object::_Create(lowguid, 0, HIGHGUID_CORPSE);

    if(!LoadValues( fields[5].GetString() ))
    {
        sLog.outError("Corpse #%d have broken data in `data` field. Can't be loaded.", lowguid);
        return false;
    }

    m_time = time_t(fields[6].GetUInt64());
    m_type = CorpseType(fields[7].GetUInt32());

    if(m_type >= MAX_CORPSE_TYPE)
    {
        sLog.outError("Corpse (guidlow %d, owner %d) have wrong corpse type, not load.",GetGUIDLow(),GUID_LOPART(GetOwnerGUID()));
        return false;
    }

    uint32 instanceid  = fields[8].GetUInt32();

    ObjectGuid guid = ObjectGuid(HIGHGUID_CORPSE, lowguid);

    // overwrite possible wrong/corrupted guid
    SetGuidValue(OBJECT_FIELD_GUID, guid);

    SetObjectScale(DEFAULT_OBJECT_SCALE);

    // place
    SetLocationInstanceId(instanceid);
    SetLocationMapId(mapid);
    Relocate(positionX, positionY, positionZ, ort);

    if(!IsPositionValid())
    {
        sLog.outError("Corpse (guidlow %d, owner %d) not created. Suggested coordinates isn't valid (X: %f Y: %f)",
            GetGUIDLow(), GUID_LOPART(GetOwnerGUID()), GetPositionX(), GetPositionY());
        return false;
    }

    m_grid = MaNGOS::ComputeGridPair(GetPositionX(), GetPositionY());

    return true;
}

bool Corpse::isVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    return IsInWorld() && u->IsInWorld() && IsWithinDistInMap(viewPoint, World::GetMaxVisibleDistanceForObject() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f), false);
}

bool Corpse::IsHostileTo( Unit const* unit ) const
{
    if (Player* owner = sObjectMgr.GetPlayer(GetOwnerGUID()))
        return owner->IsHostileTo(unit);
    else
        return false;
}

bool Corpse::IsFriendlyTo( Unit const* unit ) const
{
    if (Player* owner = sObjectMgr.GetPlayer(GetOwnerGUID()))
        return owner->IsFriendlyTo(unit);
    else
        return true;
}

bool Corpse::IsExpired(time_t t) const
{
    if(m_type == CORPSE_BONES)
        return m_time < t - 60*MINUTE;
    else
        return m_time < t - 3*DAY;
}
