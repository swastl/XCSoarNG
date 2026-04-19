// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Settings.hpp"
#include "Data.hpp"
#include "Geo/GeoPoint.hpp"
#include "Math/Angle.hpp"
#include "co/InjectTask.hxx"
#include "time/BrokenDateTime.hpp"
#include "time/PeriodClock.hpp"

struct MoreData;
struct DerivedInfo;
class CurlGlobal;

namespace TeamsTracking {

class Glue final {
  CurlGlobal &curl;

  Settings settings;

  Data data;

  PeriodClock send_clock;
  PeriodClock fetch_clock;

  Co::InjectTask inject_task;

  /* buffered GPS data for async submission */
  GeoPoint location;
  double altitude;
  Angle heading;
  BrokenDateTime timestamp;

public:
  explicit Glue(CurlGlobal &curl) noexcept;

  void SetSettings(const Settings &_settings) noexcept;

  void OnTimer(const MoreData &basic, const DerivedInfo &calculated);

  const Data &GetData() const noexcept {
    return data;
  }

private:
  Co::InvokeTask Tick(Settings settings);
  void OnCompletion(std::exception_ptr error) noexcept;
};

} // namespace TeamsTracking
