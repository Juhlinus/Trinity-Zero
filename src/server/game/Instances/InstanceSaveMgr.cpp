/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Common.h"
#include "Player.h"
#include "GridNotifiers.h"
#include "Log.h"
#include "GridStates.h"
#include "CellImpl.h"
#include "Map.h"
#include "MapManager.h"
#include "MapInstanced.h"
#include "InstanceSaveMgr.h"
#include "Timer.h"
#include "GridNotifiersImpl.h"
#include "Config.h"
#include "Transport.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Group.h"
#include "InstanceScript.h"

uint16 InstanceSaveManager::ResetTimeDelay[] = {3600, 900, 300, 60};

InstanceSaveManager::~InstanceSaveManager()
{
    // it is undefined whether this or objectmgr will be unloaded first
    // so we must be prepared for both cases
    lock_instLists = true;
    for (InstanceSaveHashMap::iterator itr = m_instanceSaveById.begin(); itr != m_instanceSaveById.end(); ++itr)
    {
        InstanceSave* save = itr->second;

        for (InstanceSave::PlayerListType::iterator itr2 = save->m_playerList.begin(), next = itr2; itr2 != save->m_playerList.end(); itr2 = next)
        {
            ++next;
            (*itr2)->UnbindInstance(save->GetMapId(), true);
        }
        save->m_playerList.clear();

        for (InstanceSave::GroupListType::iterator itr2 = save->m_groupList.begin(), next = itr2; itr2 != save->m_groupList.end(); itr2 = next)
        {
            ++next;
            (*itr2)->UnbindInstance(save->GetMapId(), true);
        }
        save->m_groupList.clear();
        delete save;
    }
}

/*
- adding instance into manager
- called from InstanceMap::Add, _LoadBoundInstances, LoadGroups
*/
InstanceSave* InstanceSaveManager::AddInstanceSave(uint32 mapId, uint32 instanceId, time_t resetTime, bool canReset, bool load)
{
    if (InstanceSave* old_save = GetInstanceSave(instanceId))
        return old_save;

    const MapEntry* entry = sMapStore.LookupEntry(mapId);
    if (!entry)
    {
        sLog->outError("InstanceSaveManager::AddInstanceSave: wrong mapid = %d, instanceid = %d!", mapId, instanceId);
        return NULL;
    }

    if (instanceId == 0)
    {
        sLog->outError("InstanceSaveManager::AddInstanceSave: mapid = %d, wrong instanceid = %d!", mapId, instanceId);
        return NULL;
    }

    if (!resetTime)
    {
        // initialize reset time
        // for normal instances if no creatures are killed the instance will reset in two hours
        if (entry->map_type == MAP_RAID)
            resetTime = GetResetTimeFor(mapId);
        else
        {
            resetTime = time(NULL) + 2 * HOUR;
            // normally this will be removed soon after in InstanceMap::Add, prevent error
            ScheduleReset(true, resetTime, InstResetEvent(0, mapId, instanceId));
        }
    }

    sLog->outDebug(LOG_FILTER_MAPS, "InstanceSaveManager::AddInstanceSave: mapid = %d, instanceid = %d", mapId, instanceId);

    InstanceSave* save = new InstanceSave(mapId, instanceId, resetTime, canReset);
    if (!load)
        save->SaveToDB();

    m_instanceSaveById[instanceId] = save;
    return save;
}

InstanceSave* InstanceSaveManager::GetInstanceSave(uint32 InstanceId)
{
    InstanceSaveHashMap::iterator itr = m_instanceSaveById.find(InstanceId);
    return itr != m_instanceSaveById.end() ? itr->second : NULL;
}

void InstanceSaveManager::DeleteInstanceFromDB(uint32 instanceid)
{
    SQLTransaction trans = CharacterDatabase.BeginTransaction();

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INSTANCE_BY_INSTANCE);
    stmt->setUInt32(0, instanceid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INSTANCE_BY_INSTANCE);
    stmt->setUInt32(0, instanceid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GROUP_INSTANCE_BY_INSTANCE);
    stmt->setUInt32(0, instanceid);
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
    // Respawn times should be deleted only when the map gets unloaded
}

void InstanceSaveManager::RemoveInstanceSave(uint32 InstanceId)
{
    InstanceSaveHashMap::iterator itr = m_instanceSaveById.find(InstanceId);
    if (itr != m_instanceSaveById.end())
    {
        // save the resettime for normal instances only when they get unloaded
        if (time_t resettime = itr->second->GetResetTimeForDB())
        {
            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_INSTANCE_RESETTIME);

            stmt->setUInt32(0, uint32(resettime));
            stmt->setUInt32(1, InstanceId);

            CharacterDatabase.Execute(stmt);
        }

        delete itr->second;
        m_instanceSaveById.erase(itr);
    }
}

InstanceSave::InstanceSave(uint16 MapId, uint32 InstanceId, time_t resetTime, bool canReset)
: m_resetTime(resetTime), m_instanceid(InstanceId), m_mapid(MapId), m_canReset(canReset)
{
}

InstanceSave::~InstanceSave()
{
    // the players and groups must be unbound before deleting the save
    ASSERT(m_playerList.empty() && m_groupList.empty());
}

/*
    Called from AddInstanceSave
*/
void InstanceSave::SaveToDB()
{
    // save instance data too
    std::string data;
    uint32 completedEncounters = 0;

    Map* map = sMapMgr->FindMap(GetMapId(), m_instanceid);
    if (map)
    {
        ASSERT(map->IsDungeon());
        if (InstanceScript* instanceScript = ((InstanceMap*)map)->GetInstanceScript())
        {
            data = instanceScript->GetSaveData();
            completedEncounters = instanceScript->GetCompletedEncounterMask();
        }
    }

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_INSTANCE_SAVE);
    stmt->setUInt32(0, m_instanceid);
    stmt->setUInt16(1, GetMapId());
    stmt->setUInt32(2, uint32(GetResetTimeForDB()));
    stmt->setUInt32(4, completedEncounters);
    stmt->setString(5, data);
    CharacterDatabase.Execute(stmt);
}

time_t InstanceSave::GetResetTimeForDB()
{
    // only save the reset time for normal instances
    const MapEntry* entry = sMapStore.LookupEntry(GetMapId());
    if (!entry || entry->map_type == MAP_RAID)
        return 0;
    else
        return GetResetTime();
}

// to cache or not to cache, that is the question
InstanceTemplate const* InstanceSave::GetTemplate()
{
    return sObjectMgr->GetInstanceTemplate(m_mapid);
}

MapEntry const* InstanceSave::GetMapEntry()
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
    if (m_playerList.empty() && m_groupList.empty())
    {
        if (!sInstanceSaveMgr->lock_instLists)
            sInstanceSaveMgr->RemoveInstanceSave(GetInstanceId());

        return false;
    }
    else
        return true;
}

void InstanceSaveManager::LoadInstances()
{
    uint32 oldMSTime = getMSTime();

    // Delete expired instances (Instance related spawns are removed in the following cleanup queries)
    CharacterDatabase.DirectExecute("DELETE i FROM instance i LEFT JOIN instance_reset ir ON mapid = map"
                                    "WHERE (i.resettime > 0 AND i.resettime < UNIX_TIMESTAMP()) OR (ir.resettime IS NOT NULL AND ir.resettime < UNIX_TIMESTAMP())");

    // Delete invalid character_instance and group_instance references
    CharacterDatabase.DirectExecute("DELETE ci.* FROM character_instance AS ci LEFT JOIN characters AS c ON ci.guid = c.guid WHERE c.guid IS NULL");
    CharacterDatabase.DirectExecute("DELETE gi.* FROM group_instance     AS gi LEFT JOIN groups     AS g ON gi.guid = g.guid WHERE g.guid IS NULL");

    // Delete invalid instance references
    CharacterDatabase.DirectExecute("DELETE i.* FROM instance AS i LEFT JOIN character_instance AS ci ON i.id = ci.instance LEFT JOIN group_instance AS gi ON i.id = gi.instance WHERE ci.guid IS NULL AND gi.guid IS NULL");

    // Delete invalid references to instance
    CharacterDatabase.DirectExecute("DELETE FROM creature_respawn WHERE instanceId > 0 AND instanceId NOT IN (SELECT id FROM instance)");
    CharacterDatabase.DirectExecute("DELETE FROM gameobject_respawn WHERE instanceId > 0 AND instanceId NOT IN (SELECT id FROM instance)");
    CharacterDatabase.DirectExecute("DELETE tmp.* FROM character_instance AS tmp LEFT JOIN instance ON tmp.instance = instance.id WHERE tmp.instance > 0 AND instance.id IS NULL");
    CharacterDatabase.DirectExecute("DELETE tmp.* FROM group_instance     AS tmp LEFT JOIN instance ON tmp.instance = instance.id WHERE tmp.instance > 0 AND instance.id IS NULL");

    // Clean invalid references to instance
    CharacterDatabase.DirectExecute("UPDATE corpse SET instanceId = 0 WHERE instanceId > 0 AND instanceId NOT IN (SELECT id FROM instance)");
    CharacterDatabase.DirectExecute("UPDATE characters AS tmp LEFT JOIN instance ON tmp.instance_id = instance.id SET tmp.instance_id = 0 WHERE tmp.instance_id > 0 AND instance.id IS NULL");

    // Initialize instance id storage (Needs to be done after the trash has been clean out)
    sMapMgr->InitInstanceIds();

    // Load reset times and clean expired instances
    sInstanceSaveMgr->LoadResetTimes();

    sLog->outString(">> Loaded instances in %u ms", GetMSTimeDiffToNow(oldMSTime));
    sLog->outString();
}

void InstanceSaveManager::LoadResetTimes()
{
    time_t now = time(NULL);
    time_t today = (now / DAY) * DAY;

    // NOTE: Use DirectPExecute for tables that will be queried later

    // get the current reset times for normal instances (these may need to be updated)
    // these are only kept in memory for InstanceSaves that are loaded later
    // resettime = 0 in the DB for raid instances so those are skipped
    typedef std::pair<uint32, time_t> ResetTimeMapType;
    typedef std::map<uint32, ResetTimeMapType> InstResetTimeMapType;
    InstResetTimeMapType instResetTime;

    // index instance ids by map pairs for fast reset warning send
    typedef std::multimap<uint32, uint32 /*instanceid*/ > ResetTimeMapDiffInstances;
    ResetTimeMapDiffInstances mapResetInstances;

    QueryResult result = CharacterDatabase.Query("SELECT id, map, resettime FROM instance ORDER BY id ASC");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 instanceId = fields[0].GetUInt32();

            // Instances are pulled in ascending order from db and nextInstanceId is initialized with 1,
            // so if the instance id is used, increment until we find the first unused one for a potential new instance
            if (sMapMgr->GetNextInstanceId() == instanceId)
                sMapMgr->SetNextInstanceId(instanceId + 1);

            // Mark instance id as being used
            sMapMgr->RegisterInstanceId(instanceId);

            if (time_t resettime = time_t(fields[3].GetUInt32()))
            {
                uint32 mapid = fields[1].GetUInt16();
                instResetTime[instanceId] = ResetTimeMapType(mapid, resettime);
                mapResetInstances.insert(ResetTimeMapDiffInstances::value_type(mapid, instanceId));
            }
        }
        while (result->NextRow());

        // update reset time for normal instances with the max creature respawn time + X hours
        if (PreparedQueryResult result2 = CharacterDatabase.Query(CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAX_CREATURE_RESPAWNS)))
        {
            do
            {
                Field* fields = result2->Fetch();
                uint32 instance = fields[1].GetUInt32();
                time_t resettime = time_t(fields[0].GetUInt32() + 2 * HOUR);
                InstResetTimeMapType::iterator itr = instResetTime.find(instance);
                if (itr != instResetTime.end() && itr->second.second != resettime)
                {
                    CharacterDatabase.DirectPExecute("UPDATE instance SET resettime = '"UI64FMTD"' WHERE id = '%u'", uint64(resettime), instance);
                    itr->second.second = resettime;
                }
            }
            while (result->NextRow());
        }

        // schedule the reset times
        for (InstResetTimeMapType::iterator itr = instResetTime.begin(); itr != instResetTime.end(); ++itr)
            if (itr->second.second > now)
                ScheduleReset(true, itr->second.second, InstResetEvent(0, PAIR32_LOPART(itr->second.first), itr->first));
    }

    // load the global respawn times for raid instances
    uint32 diff = sWorld->getIntConfig(CONFIG_INSTANCE_RESET_TIME_HOUR) * HOUR;
    result = CharacterDatabase.Query("SELECT mapid, resettime FROM instance_reset");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 mapid = fields[0].GetUInt16();
            uint64 oldresettime = fields[2].GetUInt32();

            // update the reset time if the hour in the configs changes
            uint64 newresettime = (oldresettime / DAY) * DAY + diff;
            if (oldresettime != newresettime)
                CharacterDatabase.DirectPExecute("UPDATE instance_reset SET resettime = '%u' WHERE mapid = '%u'", uint32(newresettime), mapid);

            SetResetTimeFor(mapid, newresettime);
        } while (result->NextRow());
    }

    ResetTimeMapDiffInstances::const_iterator in_itr;
}

void InstanceSaveManager::ScheduleReset(bool add, time_t time, InstResetEvent event)
{
    if (!add)
    {
        // find the event in the queue and remove it
        ResetTimeQueue::iterator itr;
        std::pair<ResetTimeQueue::iterator, ResetTimeQueue::iterator> range;
        range = m_resetTimeQueue.equal_range(time);
        for (itr = range.first; itr != range.second; ++itr)
        {
            if (itr->second == event)
            {
                m_resetTimeQueue.erase(itr);
                return;
            }
        }

        // in case the reset time changed (should happen very rarely), we search the whole queue
        if (itr == range.second)
        {
            for (itr = m_resetTimeQueue.begin(); itr != m_resetTimeQueue.end(); ++itr)
            {
                if (itr->second == event)
                {
                    m_resetTimeQueue.erase(itr);
                    return;
                }
            }

            if (itr == m_resetTimeQueue.end())
                sLog->outError("InstanceSaveManager::ScheduleReset: cannot cancel the reset, the event(%d, %d, %d) was not found!", event.type, event.mapid, event.instanceId);
        }
    }
    else
        m_resetTimeQueue.insert(std::pair<time_t, InstResetEvent>(time, event));
}

void InstanceSaveManager::Update()
{
    time_t now = time(NULL);
    time_t t;

    while (!m_resetTimeQueue.empty())
    {
        t = m_resetTimeQueue.begin()->first;
        if (t >= now)
            break;

        InstResetEvent &event = m_resetTimeQueue.begin()->second;
        if (event.type == 0)
        {
            // for individual normal instances, max creature respawn + X hours
            _ResetInstance(event.mapid, event.instanceId);
            m_resetTimeQueue.erase(m_resetTimeQueue.begin());
        }
        else
        {
            // global reset/warning for a certain map
            time_t resetTime = GetResetTimeFor(event.mapid);
            _ResetOrWarnAll(event.mapid, event.type != 4, resetTime);
            if (event.type != 4)
            {
                // schedule the next warning/reset
                ++event.type;
                ScheduleReset(true, resetTime - ResetTimeDelay[event.type-1], event);
            }
            m_resetTimeQueue.erase(m_resetTimeQueue.begin());
        }
    }
}

void InstanceSaveManager::_ResetSave(InstanceSaveHashMap::iterator &itr)
{
    // unbind all players bound to the instance
    // do not allow UnbindInstance to automatically unload the InstanceSaves
    lock_instLists = true;

    InstanceSave::PlayerListType &pList = itr->second->m_playerList;
    while (!pList.empty())
    {
        Player* player = *(pList.begin());
        player->UnbindInstance(itr->second->GetMapId(), true);
    }

    InstanceSave::GroupListType &gList = itr->second->m_groupList;
    while (!gList.empty())
    {
        Group* group = *(gList.begin());
        group->UnbindInstance(itr->second->GetMapId(), true);
    }

    delete itr->second;
    m_instanceSaveById.erase(itr++);

    lock_instLists = false;
}

void InstanceSaveManager::_ResetInstance(uint32 mapid, uint32 instanceId)
{
    sLog->outDebug(LOG_FILTER_MAPS, "InstanceSaveMgr::_ResetInstance %u, %u", mapid, instanceId);
    Map const* map = sMapMgr->CreateBaseMap(mapid);
    if (!map->Instanceable())
        return;

    InstanceSaveHashMap::iterator itr = m_instanceSaveById.find(instanceId);
    if (itr != m_instanceSaveById.end())
        _ResetSave(itr);

    DeleteInstanceFromDB(instanceId);                       // even if save not loaded

    Map* iMap = ((MapInstanced*)map)->FindInstanceMap(instanceId);

    if (iMap && iMap->IsDungeon())
        ((InstanceMap*)iMap)->Reset(INSTANCE_RESET_RESPAWN_DELAY);

    if (iMap)
        iMap->DeleteRespawnTimes();
    else
        Map::DeleteRespawnTimesInDB(mapid, instanceId);

    // Free up the instance id and allow it to be reused
    sMapMgr->FreeInstanceId(instanceId);
}

void InstanceSaveManager::_ResetOrWarnAll(uint32 mapid, bool warn, time_t resetTime)
{
    // global reset for all instances of the given map
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry->Instanceable())
        return;

    time_t now = time(NULL);

    if (!warn)
    {
        // remove all binds to instances of the given map
        for (InstanceSaveHashMap::iterator itr = m_instanceSaveById.begin(); itr != m_instanceSaveById.end();)
        {
            if (itr->second->GetMapId() == mapid)
                _ResetSave(itr);
            else
                ++itr;
        }

        // delete them from the DB, even if not loaded
        SQLTransaction trans = CharacterDatabase.BeginTransaction();
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INSTANCE_BY_MAP_DIFF);
        stmt->setUInt16(0, uint16(mapid));
        trans->Append(stmt);
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GROUP_INSTANCE_BY_MAP_DIFF);
        stmt->setUInt16(0, uint16(mapid));
        trans->Append(stmt);
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INSTANCE_BY_MAP_DIFF);
        stmt->setUInt16(0, uint16(mapid));
        trans->Append(stmt);
        CharacterDatabase.CommitTransaction(trans);

        // calculate the next reset time
        uint32 diff = sWorld->getIntConfig(CONFIG_INSTANCE_RESET_TIME_HOUR) * HOUR;

        uint32 period = uint32(((GetResetTimeFor(mapid) * sWorld->getRate(RATE_INSTANCE_RESET_TIME))/DAY) * DAY);
        if (period < DAY)
            period = DAY;

        uint32 next_reset = uint32(((resetTime + MINUTE) / DAY * DAY) + period + diff);

        SetResetTimeFor(mapid, next_reset);
        ScheduleReset(true, time_t(next_reset-3600), InstResetEvent(1, mapid, 0));

        // Update it in the DB
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_GLOBAL_INSTANCE_RESETTIME);
        stmt->setUInt32(0, next_reset);
        stmt->setUInt16(1, uint16(mapid));
        CharacterDatabase.Execute(stmt);
    }

    // note: this isn't fast but it's meant to be executed very rarely
    Map const* map = sMapMgr->CreateBaseMap(mapid);
    MapInstanced::InstancedMaps &instMaps = ((MapInstanced*)map)->GetInstancedMaps();
    MapInstanced::InstancedMaps::iterator mitr;
    uint32 timeLeft;

    for (mitr = instMaps.begin(); mitr != instMaps.end(); ++mitr)
    {
        Map* map2 = mitr->second;
        if (!map2->IsDungeon())
            continue;

        if (warn)
        {
            if (now <= resetTime)
                timeLeft = 0;
            else
                timeLeft = uint32(now - resetTime);

            ((InstanceMap*)map2)->SendResetWarnings(timeLeft);
        }
        else
            ((InstanceMap*)map2)->Reset(INSTANCE_RESET_GLOBAL);
    }

    // TODO: delete creature/gameobject respawn times even if the maps are not loaded
}

uint32 InstanceSaveManager::GetNumBoundPlayersTotal()
{
    uint32 ret = 0;
    for (InstanceSaveHashMap::iterator itr = m_instanceSaveById.begin(); itr != m_instanceSaveById.end(); ++itr)
        ret += itr->second->GetPlayerCount();

    return ret;
}

uint32 InstanceSaveManager::GetNumBoundGroupsTotal()
{
    uint32 ret = 0;
    for (InstanceSaveHashMap::iterator itr = m_instanceSaveById.begin(); itr != m_instanceSaveById.end(); ++itr)
        ret += itr->second->GetGroupCount();

    return ret;
}
