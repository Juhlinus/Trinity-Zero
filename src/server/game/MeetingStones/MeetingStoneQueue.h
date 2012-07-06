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

class MeetingStone
 {
     public:
        explicit MeetingStone();
        ~MeetingStone();

         struct MeetingStonesContainer
         {
             uint32 dungeonAreaId;
             uint32 minLevel;
             uint32 maxLevel;
         };

         //typedef std::vector<std::pair<uint32 /*goID*/, MeetingStonesContainer> > MeetingStones;
         void CheckMeetingStoneValidity(uint32 GUIDLow, time_t currTime);
         bool PlayerInMeetingStoneQueue(uint32 GUIDLow);
         bool PlayerInMeetingStoneQueueForInstanceId(uint32 GUIDLow, uint32 areaId);
         std::vector<Player*> GetPlayersInMeetingStoneQueueForInstanceId(uint32 GUIDLow, uint32 areaId);
         uint32 GetSizeOfMeetingStoneQueueForInstanceId(uint32 GUIDLow, uint32 areaId);
         uint32 GetAreaIdInMeetingStoneQueue(uint32 GUIDLow);
         uint32 GetTimeInMeetingStoneQueue() { return timeInMeetingStoneQueue; }
         std::string GetMeetingStoneQueueDungeonName(uint32 _areaId);
         void UpdateTimeInQueue(time) { timeInMeetingStoneQueue = time; }
         void AddPlayerToMeetingStoneQueue(uint32 areaId);
         void RemovePlayerFromMeetingStoneQueue(uint32 GUIDLow);
         void RemoveGroupFromMeetingStoneQueue();
         MeetingStones const& GetMeetingStones() const { return meetingStoneList; }

     protected:
         MeetingStoneList   meetingStoneList;
         std::map<uint32 /*GUIDLow*/, uint32 /*areaId*/> meetingStoneQueue;
         MeetingStones      meetingStoneList;
         time_t             timeInMeetingStoneQueue;
}
