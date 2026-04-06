// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Geo/GeoPoint.hpp"
#include "Math/Angle.hpp"
#include "thread/Mutex.hxx"

#include <cstdint>
#include <string>
#include <vector>

namespace TeamsTracking {

struct Data {
  struct Member {
    uint32_t user_id;
    std::string username;
    std::string first_name;
    std::string last_name;
    GeoPoint location;
    int altitude;
    Angle heading;
    bool own_position;
  };

  std::vector<Member> members;

  mutable Mutex mutex;
};

} // namespace TeamsTracking
