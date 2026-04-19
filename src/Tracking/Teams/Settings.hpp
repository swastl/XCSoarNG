// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "util/StaticString.hxx"

namespace TeamsTracking {

struct Settings {
  /** Enable sending own position to the backend. */
  bool enabled;

  /** Enable fetching and displaying team member positions. */
  bool team_enabled;

  /** Position sending interval in seconds. */
  unsigned interval;

  /** X-API-Key for authentication. */
  StaticString<256> api_key;

  void SetDefaults() noexcept {
    enabled = false;
    team_enabled = true;
    interval = 10;
    api_key.clear();
  }
};

} // namespace TeamsTracking
