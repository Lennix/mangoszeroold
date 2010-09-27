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

#include "InstanceSaveMgr.h"

#include "Database/SQLStorage.h"
#include "Player.h"
#include "GridNotifiers.h"
#include "Log.h"
#include "GridStates.h"
#include "CellImpl.h"
#include "Map.h"
#include "MapManager.h"
#include "MapInstanced.h"
#include "Timer.h"
#include "GridNotifiersImpl.h"
#include "Transports.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Group.h"
#include "InstanceData.h"
#include "ProgressBar.h"

INSTANTIATE_SINGLETON_1( InstanceSaveManager );

static uint32 resetEventTypeDelay[MAX_RESET_EVENT_TYPE] = { 0, 3600, 900, 300, 60 };

//== InstanceSave functions ================================

InstanceSave::InstanceSave(uint16 MapId, uint32 InstanceId, time_t resetTime, bool canReset)
: m_resetTime(resetTime), m_instanceid(InstanceId), m_mapid(MapId),
  m_canReset(canReset), m_usedByMap(false)
{
}

InstanceSave::~InstanceSave()
{
    while(!m_playerList.empty())
    {
        Player *player = *(m_playerList.begin());
        player->UnbindInstance(GetMapId(), true);
    }
    while(!m_groupList.empty())
    {
        Group *group = *(m_groupList.begin());
        group->UnbindInstance(GetMapId(), true);
    }
}

/*
    Called from AddInstanceSave
*/
void InstanceSave::SaveToDB()
{
    // save instance data too
    std::string data;

    Map *map = sMapMgr.FindMap(GetMapId(),m_instanceid);
    if(map)
    {
        MANGOS_ASSERT(map->IsDungeon());
        InstanceData *iData = ((InstanceMap *)map)->GetInstanceData();
        if(iData && iData->Save())
        {
            data = iData->Save();
            CharacterDatabase.escape_string(data);
        }
    }

    CharacterDatabase.PExecute("INSERT INTO instance VALUES ('%u', '%u', '"UI64FMTD"', '%s')", m_instanceid, GetMapId(), (uint64)GetResetTimeForDB(), data.c_str());
}

time_t InstanceSave::GetResetTimeForDB() const
{
    // only save the reset time for normal instances
    const MapEntry *entry = sMapStore.LookupEntry(GetMapId());
    if(!entry || entry->map_type == MAP_RAID)
        return 0;
    else
        return GetResetTime();
}

// to cache or not to cache, that is the question
InstanceTemplate const* InstanceSave::GetTemplate() const
{
    return ObjectMgr::GetInstanceTemplate(m_mapid);
}

MapEntry const* InstanceSave::GetMapEntry() const
{
    return sMapStore.LookupEntry(m_mapid);
}

void InstanceSave::DeleteFromDB()
{
    InstanceSaveManager::DeleteInstanceFromDB(GetInstanceId());
}

/* true if the instance save is still valid */
bool InstanceSave::UnloadIfEmpty()
{
    if (m_playerList.empty() && m_groupList.empty() && !m_usedByMap)
    {
        sInstanceSaveMgr.RemoveInstanceSave(GetInstanceId());
        return false;
    }
    else
        return true;
}

//== InstanceResetScheduler functions ======================

uint32 InstanceResetScheduler::GetMaxResetTimeFor(InstanceTemplate const* temp)
{
    if (!temp)
        return 0;

    return temp->reset_delay * DAY;
}

time_t InstanceResetScheduler::CalculateNextResetTime(InstanceTemplate const* temp, time_t prevResetTime)
{
    uint32 diff = sWorld.getConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR) * HOUR;
    uint32 period = GetMaxResetTimeFor(temp);
    return ((prevResetTime + MINUTE) / DAY * DAY) + period + diff;
}

void InstanceResetScheduler::LoadResetTimes()
{
    time_t now = time(NULL);
    time_t today = (now / DAY) * DAY;

    // NOTE: Use DirectPExecute for tables that will be queried later

    // get the current reset times for normal instances (these may need to be updated)
    // these are only kept in memory for InstanceSaves that are loaded later
    // resettime = 0 in the DB for raid instances so those are skipped
    typedef std::map<uint32, std::pair<uint32, time_t> > ResetTimeMapType;
    ResetTimeMapType InstResetTime;

    QueryResult *result = CharacterDatabase.Query("SELECT id, map, resettime FROM instance WHERE resettime > 0");
    if( result )
    {
        do
        {
            if(time_t resettime = time_t((*result)[2].GetUInt64()))
            {
                uint32 id = (*result)[0].GetUInt32();
                uint32 mapid = (*result)[1].GetUInt32();
                InstResetTime[id] = std::pair<uint32, uint64>(mapid, resettime);
            }
        }
        while (result->NextRow());
        delete result;

        // update reset time for normal instances with the max creature respawn time + X hours
        result = CharacterDatabase.Query("SELECT MAX(respawntime), instance FROM creature_respawn WHERE instance > 0 GROUP BY instance");
        if( result )
        {
            do
            {
                Field *fields = result->Fetch();
                uint32 instance = fields[1].GetUInt32();
                time_t resettime = time_t(fields[0].GetUInt64() + 2 * HOUR);
                ResetTimeMapType::iterator itr = InstResetTime.find(instance);
                if(itr != InstResetTime.end() && itr->second.second != resettime)
                {
                    CharacterDatabase.DirectPExecute("UPDATE instance SET resettime = '"UI64FMTD"' WHERE id = '%u'", uint64(resettime), instance);
                    itr->second.second = resettime;
                }
            }
            while (result->NextRow());
            delete result;
        }

        // schedule the reset times
        for(ResetTimeMapType::iterator itr = InstResetTime.begin(); itr != InstResetTime.end(); ++itr)
            if(itr->second.second > now)
                ScheduleReset(true, itr->second.second, InstanceResetEvent(RESET_EVENT_DUNGEON, itr->second.first, itr->first));
    }

    // load the global respawn times for raid instances
    uint32 diff = sWorld.getConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR) * HOUR;
    m_resetTimeByMapId.resize(sMapStore.GetNumRows()+1);
    result = CharacterDatabase.Query("SELECT mapid, resettime FROM instance_reset");
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 mapid = fields[0].GetUInt32();
            if(!ObjectMgr::GetInstanceTemplate(mapid))
            {
                sLog.outError("InstanceSaveManager::LoadResetTimes: invalid mapid %u in instance_reset!", mapid);
                CharacterDatabase.DirectPExecute("DELETE FROM instance_reset WHERE mapid = '%u'", mapid);
                continue;
            }

            // update the reset time if the hour in the configs changes
            uint64 oldresettime = fields[1].GetUInt64();
            uint64 newresettime = (oldresettime / DAY) * DAY + diff;
            if(oldresettime != newresettime)
                CharacterDatabase.DirectPExecute("UPDATE instance_reset SET resettime = '"UI64FMTD"' WHERE mapid = '%u'", newresettime, mapid);

            SetResetTimeFor(mapid, newresettime);
        } while(result->NextRow());
        delete result;
    }

    // clean expired instances, references to them will be deleted in CleanupInstances
    // must be done before calculating new reset times
    m_InstanceSaves._CleanupExpiredInstancesAtTime(now);

    // calculate new global reset times for expired instances and those that have never been reset yet
    // add the global reset times to the priority queue
    for(uint32 i = 0; i < sInstanceTemplate.MaxEntry; i++)
    {
        // only raid maps have a global reset time
        InstanceTemplate const* temp = ObjectMgr::GetInstanceTemplate(i);
        if(!temp || !temp->reset_delay)
            continue;

        uint32 period = GetMaxResetTimeFor(temp);
        time_t t = GetResetTimeFor(temp->map);
        if(!t)
        {
            // initialize the reset time
            t = today + period + diff;
            CharacterDatabase.DirectPExecute("INSERT INTO instance_reset VALUES ('%u','"UI64FMTD"')", temp->map, (uint64)t);
        }

        if(t < now)
        {
            // assume that expired instances have already been cleaned
            // calculate the next reset time
            t = (t / DAY) * DAY;
            t += ((today - t) / period + 1) * period + diff;
            CharacterDatabase.DirectPExecute("UPDATE instance_reset SET resettime = '"UI64FMTD"' WHERE mapid = '%u'", (uint64)t, temp->map);
        }

        SetResetTimeFor(temp->map, t);

        // schedule the global reset/warning
        ResetEventType type = RESET_EVENT_INFORM_1;
        for(; type < RESET_EVENT_INFORM_LAST; type = ResetEventType(type+1))
            if(t - resetEventTypeDelay[type] > now)
                break;

        ScheduleReset(true, t - resetEventTypeDelay[type], InstanceResetEvent(type, temp->map, 0));
    }
}

void InstanceResetScheduler::ScheduleReset(bool add, time_t time, InstanceResetEvent event)
{
    if(add) m_resetTimeQueue.insert(std::pair<time_t, InstanceResetEvent>(time, event));
    else
    {
        // find the event in the queue and remove it
        ResetTimeQueue::iterator itr;
        std::pair<ResetTimeQueue::iterator, ResetTimeQueue::iterator> range;
        range = m_resetTimeQueue.equal_range(time);
        for(itr = range.first; itr != range.second; ++itr)
            if(itr->second == event) { m_resetTimeQueue.erase(itr); return; }
            // in case the reset time changed (should happen very rarely), we search the whole queue
            if(itr == range.second)
            {
                for(itr = m_resetTimeQueue.begin(); itr != m_resetTimeQueue.end(); ++itr)
                    if(itr->second == event) { m_resetTimeQueue.erase(itr); return; }
                    if(itr == m_resetTimeQueue.end())
                        sLog.outError("InstanceSaveManager::ScheduleReset: cannot cancel the reset, the event(%d,%d,%d) was not found!", event.type, event.mapid, event.instanceId);
            }
    }
}

void InstanceResetScheduler::Update()
{
    time_t now = time(NULL), t;
    while(!m_resetTimeQueue.empty() && (t = m_resetTimeQueue.begin()->first) < now)
    {
        InstanceResetEvent &event = m_resetTimeQueue.begin()->second;
        if (event.type == RESET_EVENT_DUNGEON)
        {
            // for individual normal instances, max creature respawn + X hours
            m_InstanceSaves._ResetInstance(event.mapid, event.instanceId);
        }
        else
        {
            // global reset/warning for a certain map
            time_t resetTime = GetResetTimeFor(event.mapid);
            m_InstanceSaves._ResetOrWarnAll(event.mapid, event.type != RESET_EVENT_INFORM_LAST, uint32(resetTime - now));
            if (event.type != RESET_EVENT_INFORM_LAST)
            {
                // schedule the next warning/reset
                event.type = ResetEventType(event.type+1);
                ScheduleReset(true, resetTime - resetEventTypeDelay[event.type], event);
            }
            else
            {
                // re-schedule the next/new global reset/warning
                // calculate the next reset time
                InstanceTemplate const* instanceTemplate = ObjectMgr::GetInstanceTemplate(event.mapid);
                MANGOS_ASSERT(instanceTemplate);

                time_t next_reset = InstanceResetScheduler::CalculateNextResetTime(instanceTemplate, resetTime);

                CharacterDatabase.DirectPExecute("UPDATE instance_reset SET resettime = '"UI64FMTD"' WHERE mapid = '%u'", uint64(next_reset), uint32(event.mapid));

                SetResetTimeFor(event.mapid, next_reset);

                ResetEventType type = RESET_EVENT_INFORM_1;
                for (; type < RESET_EVENT_INFORM_LAST; type = ResetEventType(type+1))
                    if (next_reset - resetEventTypeDelay[type] > now)
                        break;

                // add new scheduler event to the queue
                event.type = type;
                ScheduleReset(true, next_reset - resetEventTypeDelay[event.type], event);
            }
        }
        m_resetTimeQueue.erase(m_resetTimeQueue.begin());
    }
}

//== InstanceSaveManager functions =========================

InstanceSaveManager::InstanceSaveManager() : lock_instLists(false), m_Scheduler(*this)
{
}

InstanceSaveManager::~InstanceSaveManager()
{
    // it is undefined whether this or objectmgr will be unloaded first
    // so we must be prepared for both cases
    lock_instLists = true;
    for (InstanceSaveHashMap::iterator itr = m_instanceSaveById.begin(); itr != m_instanceSaveById.end(); ++itr)
        delete  itr->second;
}

/*
- adding instance into manager
- called from InstanceMap::Add, _LoadBoundInstances, LoadGroups
*/
InstanceSave* InstanceSaveManager::AddInstanceSave(uint32 mapId, uint32 instanceId, time_t resetTime, bool canReset, bool load)
{
    InstanceSave *save = GetInstanceSave(instanceId);
    if(save) return save;

    const MapEntry* entry = sMapStore.LookupEntry(mapId);
    if(!entry || instanceId == 0)
    {
        sLog.outError("InstanceSaveManager::AddInstanceSave: wrong mapid = %d, instanceid = %d!", mapId, instanceId);
        return NULL;
    }

    if(!resetTime)
    {
        // initialize reset time
        // for normal instances if no creatures are killed the instance will reset in two hours
        if(entry->map_type == MAP_RAID)
            resetTime = m_Scheduler.GetResetTimeFor(mapId);
        else
        {
            resetTime = time(NULL) + 2 * HOUR;
            // normally this will be removed soon after in InstanceMap::Add, prevent error
            m_Scheduler.ScheduleReset(true, resetTime, InstanceResetEvent(RESET_EVENT_DUNGEON, mapId, instanceId));
        }
    }

    DEBUG_LOG("InstanceSaveManager::AddInstanceSave: mapid = %d, instanceid = %d", mapId, instanceId);

    save = new InstanceSave(mapId, instanceId, resetTime, canReset);
    if(!load) save->SaveToDB();

    m_instanceSaveById[instanceId] = save;
    return save;
}

InstanceSave *InstanceSaveManager::GetInstanceSave(uint32 InstanceId)
{
    InstanceSaveHashMap::iterator itr = m_instanceSaveById.find(InstanceId);
    return itr != m_instanceSaveById.end() ? itr->second : NULL;
}

void InstanceSaveManager::DeleteInstanceFromDB(uint32 instanceid)
{
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM instance WHERE id = '%u'", instanceid);
    CharacterDatabase.PExecute("DELETE FROM character_instance WHERE instance = '%u'", instanceid);
    CharacterDatabase.PExecute("DELETE FROM group_instance WHERE instance = '%u'", instanceid);
    CharacterDatabase.CommitTransaction();
    // respawn times should be deleted only when the map gets unloaded
}

void InstanceSaveManager::RemoveInstanceSave(uint32 InstanceId)
{
    if (lock_instLists)
        return;

    InstanceSaveHashMap::iterator itr = m_instanceSaveById.find( InstanceId );
    if(itr != m_instanceSaveById.end())
    {
        // save the resettime for normal instances only when they get unloaded
        if(time_t resettime = itr->second->GetResetTimeForDB())
            CharacterDatabase.PExecute("UPDATE instance SET resettime = '"UI64FMTD"' WHERE id = '%u'", (uint64)resettime, InstanceId);

        _ResetSave(itr);
    }
}

void InstanceSaveManager::_DelHelper(DatabaseType &db, const char *fields, const char *table, const char *queryTail,...)
{
    Tokens fieldTokens = StrSplit(fields, ", ");
    MANGOS_ASSERT(fieldTokens.size() != 0);

    va_list ap;
    char szQueryTail [MAX_QUERY_LEN];
    va_start(ap, queryTail);
    vsnprintf( szQueryTail, MAX_QUERY_LEN, queryTail, ap );
    va_end(ap);

    QueryResult *result = db.PQuery("SELECT %s FROM %s %s", fields, table, szQueryTail);
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            std::ostringstream ss;
            for(size_t i = 0; i < fieldTokens.size(); i++)
            {
                std::string fieldValue = fields[i].GetCppString();
                db.escape_string(fieldValue);
                ss << (i != 0 ? " AND " : "") << fieldTokens[i] << " = '" << fieldValue << "'";
            }
            db.DirectPExecute("DELETE FROM %s WHERE %s", table, ss.str().c_str());
        } while (result->NextRow());
        delete result;
    }
}

void InstanceSaveManager::CleanupInstances()
{
    barGoLink bar(2);
    bar.step();

    // load reset times and clean expired instances
    m_Scheduler.LoadResetTimes();

    // clean character/group - instance binds with invalid group/characters
    _DelHelper(CharacterDatabase, "character_instance.guid, instance", "character_instance", "LEFT JOIN characters ON character_instance.guid = characters.guid WHERE characters.guid IS NULL");
    _DelHelper(CharacterDatabase, "group_instance.leaderGuid, instance", "group_instance", "LEFT JOIN characters ON group_instance.leaderGuid = characters.guid LEFT JOIN groups ON group_instance.leaderGuid = groups.leaderGuid WHERE characters.guid IS NULL OR groups.leaderGuid IS NULL");

    // clean instances that do not have any players or groups bound to them
    _DelHelper(CharacterDatabase, "id, map", "instance", "LEFT JOIN character_instance ON character_instance.instance = id LEFT JOIN group_instance ON group_instance.instance = id WHERE character_instance.instance IS NULL AND group_instance.instance IS NULL");

    // clean invalid instance references in other tables
    _DelHelper(CharacterDatabase, "character_instance.guid, instance", "character_instance", "LEFT JOIN instance ON character_instance.instance = instance.id WHERE instance.id IS NULL");
    _DelHelper(CharacterDatabase, "group_instance.leaderGuid, instance", "group_instance", "LEFT JOIN instance ON group_instance.instance = instance.id WHERE instance.id IS NULL");

    // clean unused respawn data
    CharacterDatabase.DirectExecute("DELETE FROM creature_respawn WHERE instance <> 0 AND instance NOT IN (SELECT id FROM instance)");
    CharacterDatabase.DirectExecute("DELETE FROM gameobject_respawn WHERE instance <> 0 AND instance NOT IN (SELECT id FROM instance)");

    bar.step();
    sLog.outString();
    sLog.outString( ">> Instances cleaned up");
}

void InstanceSaveManager::PackInstances()
{
    // this routine renumbers player instance associations in such a way so they start from 1 and go up
    // TODO: this can be done a LOT more efficiently

    // obtain set of all associations
    std::set<uint32> InstanceSet;

    // all valid ids are in the instance table
    // any associations to ids not in this table are assumed to be
    // cleaned already in CleanupInstances
    QueryResult *result = CharacterDatabase.Query("SELECT id FROM instance");
    if( result )
    {
        do
        {
            Field *fields = result->Fetch();
            InstanceSet.insert(fields[0].GetUInt32());
        }
        while (result->NextRow());
        delete result;
    }

    barGoLink bar( InstanceSet.size() + 1);
    bar.step();

    uint32 InstanceNumber = 1;
    // we do assume std::set is sorted properly on integer value
    for (std::set<uint32>::iterator i = InstanceSet.begin(); i != InstanceSet.end(); ++i)
    {
        if (*i != InstanceNumber)
        {
            // remap instance id
            CharacterDatabase.PExecute("UPDATE creature_respawn SET instance = '%u' WHERE instance = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE gameobject_respawn SET instance = '%u' WHERE instance = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE corpse SET instance = '%u' WHERE instance = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE character_instance SET instance = '%u' WHERE instance = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE instance SET id = '%u' WHERE id = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE group_instance SET instance = '%u' WHERE instance = '%u'", InstanceNumber, *i);
        }

        ++InstanceNumber;
        bar.step();
    }

    sLog.outString( ">> Instance numbers remapped, next instance id is %u", InstanceNumber );
    sLog.outString();
}

void InstanceSaveManager::_ResetSave(InstanceSaveHashMap::iterator &itr)
{
    // unbind all players bound to the instance
    // do not allow UnbindInstance to automatically unload the InstanceSaves
    lock_instLists = true;
    delete itr->second;
    m_instanceSaveById.erase(itr++);
    lock_instLists = false;
}

void InstanceSaveManager::_ResetInstance(uint32 mapid, uint32 instanceId)
{
    DEBUG_LOG("InstanceSaveMgr::_ResetInstance %u, %u", mapid, instanceId);
    Map *map = (MapInstanced*)sMapMgr.CreateBaseMap(mapid);
    if(!map->Instanceable())
        return;

    InstanceSaveHashMap::iterator itr = m_instanceSaveById.find(instanceId);
    if(itr != m_instanceSaveById.end()) _ResetSave(itr);
    DeleteInstanceFromDB(instanceId);                       // even if save not loaded

    Map* iMap = ((MapInstanced*)map)->FindMap(instanceId);
    if(iMap && iMap->IsDungeon()) ((InstanceMap*)iMap)->Reset(INSTANCE_RESET_RESPAWN_DELAY);
    else sObjectMgr.DeleteRespawnTimeForInstance(instanceId);   // even if map is not loaded
}

void InstanceSaveManager::_ResetOrWarnAll(uint32 mapid, bool warn, uint32 timeLeft)
{
    // global reset for all instances of the given map
    // note: this isn't fast but it's meant to be executed very rarely
    Map const *map = sMapMgr.CreateBaseMap(mapid);
    if(!map->Instanceable())
        return;

    time_t now = time(NULL);

    if(!warn)
    {
        // this is called one minute before the reset time
        InstanceTemplate const* temp = ObjectMgr::GetInstanceTemplate(mapid);
        if(!temp || !temp->reset_delay)
        {
            sLog.outError("InstanceSaveManager::ResetOrWarnAll: no instance template or reset delay for map %d", mapid);
            return;
        }

        // remove all binds to instances of the given map
        for(InstanceSaveHashMap::iterator itr = m_instanceSaveById.begin(); itr != m_instanceSaveById.end();)
        {
            if(itr->second->GetMapId() == mapid)
                _ResetSave(itr);
            else
                ++itr;
        }

        // delete them from the DB, even if not loaded
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM character_instance USING character_instance LEFT JOIN instance ON character_instance.instance = id WHERE map = '%u'", mapid);
        CharacterDatabase.PExecute("DELETE FROM group_instance USING group_instance LEFT JOIN instance ON group_instance.instance = id WHERE map = '%u'", mapid);
        CharacterDatabase.PExecute("DELETE FROM instance WHERE map = '%u'", mapid);
        CharacterDatabase.CommitTransaction();

        // calculate the next reset time
        time_t next_reset = InstanceResetScheduler::CalculateNextResetTime(temp, now + timeLeft);
        // update it in the DB
        CharacterDatabase.PExecute("UPDATE instance_reset SET resettime = '"UI64FMTD"' WHERE mapid = '%d'", (uint64)next_reset, mapid);
    }

    MapInstanced::InstancedMaps &instMaps = ((MapInstanced*)map)->GetInstancedMaps();
    MapInstanced::InstancedMaps::iterator mitr;
    for(mitr = instMaps.begin(); mitr != instMaps.end(); ++mitr)
    {
        Map *map2 = mitr->second;
        if (!map2->IsDungeon())
            continue;

        if (warn)
            ((InstanceMap*)map2)->SendResetWarnings(timeLeft);
        else
            ((InstanceMap*)map2)->Reset(INSTANCE_RESET_GLOBAL);
    }

    // TODO: delete creature/gameobject respawn times even if the maps are not loaded
}

uint32 InstanceSaveManager::GetNumBoundPlayersTotal()
{
    uint32 ret = 0;
    for(InstanceSaveHashMap::iterator itr = m_instanceSaveById.begin(); itr != m_instanceSaveById.end(); ++itr)
        ret += itr->second->GetPlayerCount();
    return ret;
}

uint32 InstanceSaveManager::GetNumBoundGroupsTotal()
{
    uint32 ret = 0;
    for(InstanceSaveHashMap::iterator itr = m_instanceSaveById.begin(); itr != m_instanceSaveById.end(); ++itr)
        ret += itr->second->GetGroupCount();
    return ret;
}

void InstanceSaveManager::_CleanupExpiredInstancesAtTime( time_t t )
{
    _DelHelper(CharacterDatabase, "id, map", "instance", "LEFT JOIN instance_reset ON mapid = map WHERE (instance.resettime < '"UI64FMTD"' AND instance.resettime > '0') OR (NOT instance_reset.resettime IS NULL AND instance_reset.resettime < '"UI64FMTD"')",  (uint64)t, (uint64)t);
}
