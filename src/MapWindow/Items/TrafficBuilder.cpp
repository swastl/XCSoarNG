// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Builder.hpp"
#include "MapItem.hpp"
#include "List.hpp"
#include "FLARM/List.hpp"
#include "FLARM/Friends.hpp"
#include "Tracking/Teams/Data.hpp"
#include "Tracking/TrackingGlue.hpp"
#include "Components.hpp"
#include "NetComponents.hpp"

void
MapItemListBuilder::AddTraffic(const TrafficList &flarm)
{
  for (const auto &t : flarm.list) {
    if (list.full())
      break;

    if (!t.location_available || !t.location.IsValid())
      continue;

    if (location.DistanceS(t.location) < range) {
      auto color = FlarmFriends::GetFriendColor(t.id);
      list.append(new TrafficMapItem(t.id, color));
    }
  }
}

void
MapItemListBuilder::AddSkyLinesTraffic()
{
  /* SkyLines traffic is now injected into the FLARM traffic list by
     FlarmTrafficBuilder; AddTraffic() already covers it. */
}

void
MapItemListBuilder::AddTeamsTraffic()
{
#ifdef HAVE_HTTP
  if (net_components == nullptr || !net_components->tracking)
    return;

  const auto &data = net_components->tracking->GetTeamsData();
  const std::lock_guard lock{data.mutex};

  for (const auto &member : data.members) {
    if (list.full())
      break;

    if (member.own_position || !member.location.IsValid())
      continue;

    if (location.DistanceS(member.location) < range)
      list.append(new TeamsTrafficMapItem(member.user_id, member.altitude,
                                          member.heading, member.location,
                                          member.username.c_str()));
  }
#endif
}
