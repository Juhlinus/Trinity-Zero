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

#include "MeetingStoneQueue.h"
#include "Chat.h"

bool MeetingStone::PlayerInMeetingStoneQueue(uint32 GUIDLow)
{
    for (std::map<uint32, uint32>::iterator itr = meetingStoneQueue.begin(); itr != meetingStoneQueue.end(); ++itr)
        if (itr->first == GUIDLow)
            return true;
    return false;
}

bool MeetingStone::PlayerInMeetingStoneQueueForInstanceId(uint32 GUIDLow, uint32 areaId)
{
    for (std::map<uint32, uint32>::iterator itr = meetingStoneQueue.begin(); itr != meetingStoneQueue.end(); ++itr)
        if (itr->first == GUIDLow && itr->second == areaId)
            return true;
    return false;
}

std::vector<Player*> MeetingStone::GetPlayersInMeetingStoneQueueForInstanceId(uint32 areaId)
{
    std::vector<Player*> players;
    for (std::map<uint32, uint32>::iterator itr = meetingStoneQueue.begin(); itr != meetingStoneQueue.end(); ++itr)
        if (itr->second == areaId)
            players.push_back(ObjectAccessor::GetPlayer(*this, MAKE_NEW_GUID(itr->first, 0, HIGHGUID_PLAYER)));
    return players;
}

uint32 MeetingStone::GetSizeOfMeetingStoneQueueForInstanceId(uint32 areaId)
{
    uint32 count = 0;
    for (std::map<uint32, uint32>::iterator itr = meetingStoneQueue.begin(); itr != meetingStoneQueue.end(); ++itr)
        if (itr->second == areaId)
            count++;
    return count;
}

uint32 MeetingStone::GetAreaIdInMeetingStoneQueue(uint32 GUIDLow)
{
    for (std::map<uint32, uint32>::iterator itr = meetingStoneQueue.begin(); itr != meetingStoneQueue.end(); ++itr)
        if (itr->first == GUIDLow)
            return itr->second;
    return 0;
}

std::string MeetingStone::GetMeetingStoneQueueDungeonName(uint32 _areaId)
{
    return GetAreaEntryByAreaID(_areaId)->area_name[GetSession()->GetSessionDbcLocale()];
}

void MeetingStone::RemovePlayerFromMeetingStoneQueue(uint32 GUIDLow)
{
    std::map<uint32, uint32>::iterator itr = meetingStoneQueue.find(GUIDLow);
    if (itr != meetingStoneQueue.end())
    {
        meetingStoneQueue.erase(itr);
        timeInMeetingStoneQueue = 0;
    }
}

void MeetingStone::AddToMeetingStoneQueue(uint32 GUIDLow, uint32 areaId)
{
    if (!GUIDLow || !areaId)
        return;

    if (Player* player = ObjectAccessor::FindPlayer(GUIDLow))
    {
        if (player->getLevel() < maxLevel)
        {
            if (player->getLevel() > minLevel)
            {

            }
            else
                ChatHandler(player).PSendSysMessage("Your level is too low for the dungeon.");
        }
        else
            ChatHandler(player).PSendSysMessage("Your level is too high for the dungeon.");
    }
}

void MeetingStone::CheckMeetingStoneQueue(uint32 GUIDLow, time_t currTime)
{
    bool foundGroup = false;

    if (Player* player = ObjectAccessor::FindPlayer(GUIDLow))
    {
        if (PlayerInMeetingStoneQueue(GUIDLow))
        {
            timeInMeetingStoneQueue += currTime; // Update player's timer
            uint32 dungeonArea = GetAreaIdInMeetingStoneQueue(GUIDLow);
            uint32 mapId = GetAreaEntryByAreaID(dungeonArea)->mapid;
            std::string dungeonName = dungeonArea ? GetMeetingStoneQueueDungeonName(dungeonArea) : "<unkown>";
            std::vector<Player*> playersInQueueForInstance = GetPlayersInMeetingStoneQueueForInstanceId(dungeonArea);

            //! "As time goes on and you are unable to find a group, the meeting stone will become less picky about who it chooses to group you with."
            if ((timeInMeetingStoneQueue & 30000) == 0)
                ChatHandler(player).PSendSysMessage("You are in queue to find a group for dungeon %s...", dungeonName);

            for (std::vector<Player*>::const_iterator itr = playersInQueueForInstance.begin(); itr != playersInQueueForInstance.end(); ++itr)
            {
                //! Here we iterate through all possibile players that are in the queue, these can be:
                //  - Groups which are put in queue by their leader;
                //  - Players that are queueing on their own;
                //  - 
                if (Player* playerInQ = (*itr)->ToPlayer())
                {
                    //! Only continue if this player is actually in queue for the
                    //! dungeon we are searching a group for.
                    if (playerInQ->PlayerInMeetingStoneQueueForInstanceId(dungeonArea))
                    {
                        if (Group* group = playerInQ->GetGroup())
                        {
                            //! Only make this work if this player is the leader of the group, else there is
                            //! a high chance we iterate through the same group more than once. Also please
                            //! note how the leader is contained in the iteration (obviously).
                            if (group->GetLeaderGUID() == playerInQ->GetGUID())
                            {
                                for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
                                {
                                    if (Player* grpMember = itr->getSource())
                                    {
                                        //! We won't invite ourselves to this group if:
                                        //  - One of their members is not in queue for the queue;
                                        //  - A groupmember ignored this player (or likewise);
                                        //  - The group is full;
                                        if (PlayerInMeetingStoneQueueForInstanceId(grpMember->GetGUIDLow(), dungeonArea) || group->IsFull() || grpMember->GetSocial()->HasIgnore(GetGUIDLow()) || GetSocial()->HasIgnore(grpMember->GetGUIDLow()))
                                            break;

                                        group->AddInvite(player);
                                        //RemovePlayerFromMeetingStoneQueue(GUIDLow); //? Won't this crash? Removing an element while iterating over it...

                                    }
                                }
                            }
                        }
                        //! If player is not in a group and thus searching on its own...
                        else
                        {
                            //! This means there are two players searching for a group and are currently
                            //! not in a group, so group them up together.
                            group = new Group;
                            group->Create(this);
                            group->AddMember(playerInQ);
                            //group->AddMember(this); // Not sure if neccesary
                            //! We are not removing them from queue on purpose as they are
                            //! still searching for new players to join. The difference is
                            //! that they check if the player in queue is in group will now
                            //! become true, so this is never reached again.
                        }
                    }
                }
            }
        }
        else
            timeInMeetingStoneQueue = 0; // If player is not in queue, set the time back to zero (we do this every updatecall)
    }
}
