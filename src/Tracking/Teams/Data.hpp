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
    GeoPoint location;
    int altitude;
    Angle heading;
    bool own_position;
  };

  std::vector<Member> members;

  mutable Mutex mutex;
};

} // namespace TeamsTracking
