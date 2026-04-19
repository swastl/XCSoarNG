// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Glue.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "LogFile.hpp"
#include "co/Task.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "lib/curl/CoRequest.hxx"
#include "util/BindMethod.hxx"
#include "time/BrokenDate.hpp"

#include <boost/json.hpp>
#include <curl/curl.h>
#include <fmt/format.h>

namespace TeamsTracking {

static constexpr const char *POST_URL =
  "https://backend.xcsoar-teams.web.blueberrybirds.de/api/positions";
static constexpr const char *GET_URL =
  "https://backend.xcsoar-teams.web.blueberrybirds.de/api/positions/team";
static constexpr std::chrono::seconds FETCH_INTERVAL{30};

static constexpr std::size_t ISO8601_BUFFER_SIZE = 32;

/**
 * RAII wrapper for curl_slist (HTTP headers list).
 */
struct CurlHeaders {
  struct curl_slist *list = nullptr;

  ~CurlHeaders() noexcept {
    curl_slist_free_all(list);
  }

  void Append(const char *str) noexcept {
    list = curl_slist_append(list, str);
  }
};

Glue::Glue(CurlGlobal &_curl) noexcept
  :curl(_curl),
   inject_task(_curl.GetEventLoop())
{
  settings.SetDefaults();
}

void
Glue::SetSettings(const Settings &_settings) noexcept
{
  settings = _settings;
}

void
Glue::OnTimer(const MoreData &basic, [[maybe_unused]] const DerivedInfo &calculated)
{
  if (!settings.enabled && !settings.team_enabled)
    return;

  if (!basic.location_available)
    return;

  if (!basic.gps.real)
    return;

  const bool should_send = settings.enabled &&
    send_clock.CheckUpdate(std::chrono::seconds(settings.interval));
  const bool should_fetch = settings.team_enabled &&
    fetch_clock.CheckUpdate(FETCH_INTERVAL);

  if (!should_send && !should_fetch)
    return;

  if (inject_task)
    /* previous tick still running, skip */
    return;

  /* capture current GPS data for async submission */
  location = basic.location;
  altitude = basic.NavAltitudeAvailable() ? basic.nav_altitude : 0.0;
  heading = basic.track_available ? basic.track : Angle::Zero();
  timestamp = basic.date_time_utc;
  if (!timestamp.IsDatePlausible())
    static_cast<BrokenDate &>(timestamp) = BrokenDate::TodayUTC();

  inject_task.Start(Tick(settings), BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
Glue::Tick(Settings settings)
{
  if (settings.enabled && !settings.api_key.empty()) {
    /* POST own position */
    char iso_time[ISO8601_BUFFER_SIZE];
    FormatISO8601(iso_time, timestamp);

    const auto json_body = fmt::format(
      "{{\"latitude\":{:.6f},\"longitude\":{:.6f},"
      "\"altitude\":{:.1f},\"heading\":{:.1f},"
      "\"recordedAt\":\"{}\"}}",
      location.latitude.Degrees(),
      location.longitude.Degrees(),
      altitude,
      heading.AsBearing().Degrees(),
      iso_time);

    const auto api_key_header = fmt::format("X-API-Key: {}", settings.api_key.c_str());

    CurlHeaders post_headers;
    post_headers.Append(api_key_header.c_str());
    post_headers.Append("Content-Type: application/json");

    CurlEasy easy{POST_URL};
    Curl::Setup(easy);
    easy.SetRequestHeaders(post_headers.list);
    easy.SetRequestBody(json_body);

    const auto response = co_await Curl::CoRequest(curl, std::move(easy));
    if (response.status != 200 && response.status != 201 &&
        response.status != 204) {
      LogFmt("TeamsTracking: POST returned HTTP {}", response.status);
    }
  }

  if (settings.team_enabled && !settings.api_key.empty()) {
    /* GET team positions */
    const auto api_key_header = fmt::format("X-API-Key: {}", settings.api_key.c_str());

    CurlHeaders get_headers;
    get_headers.Append(api_key_header.c_str());

    CurlEasy easy{GET_URL};
    Curl::Setup(easy);
    easy.SetRequestHeaders(get_headers.list);

    const auto response = co_await Curl::CoRequest(curl, std::move(easy));

    if (response.status == 200) {
      const auto jv = boost::json::parse(response.body);
      const auto &arr = jv.as_array();

      std::vector<Data::Member> members;
      members.reserve(arr.size());

      for (const auto &item : arr) {
        const auto &obj = item.as_object();

        Data::Member m;
        m.user_id = 0;
        m.altitude = 0;
        m.own_position = false;

        if (const auto *id = obj.if_contains("userId");
            id != nullptr && id->is_number())
          m.user_id = id->to_number<uint32_t>();

        if (const auto *uname = obj.if_contains("username");
            uname != nullptr && uname->is_string())
          m.username = std::string(uname->as_string());

        if (const auto *v = obj.if_contains("firstName");
            v != nullptr && v->is_string())
          m.first_name = std::string(v->as_string());

        if (const auto *v = obj.if_contains("lastName");
            v != nullptr && v->is_string())
          m.last_name = std::string(v->as_string());

        double lat = 0.0, lon = 0.0;
        if (const auto *v = obj.if_contains("latitude");
            v != nullptr && v->is_number())
          lat = v->to_number<double>();
        if (const auto *v = obj.if_contains("longitude");
            v != nullptr && v->is_number())
          lon = v->to_number<double>();
        m.location = GeoPoint{Angle::Degrees(lon), Angle::Degrees(lat)};

        if (const auto *v = obj.if_contains("altitude");
            v != nullptr && v->is_number())
          m.altitude = static_cast<int>(v->to_number<double>());

        if (const auto *v = obj.if_contains("heading");
            v != nullptr && v->is_number())
          m.heading = Angle::Degrees(v->to_number<double>());
        else
          m.heading = Angle::Zero();

        if (const auto *v = obj.if_contains("ownPosition");
            v != nullptr && v->is_bool())
          m.own_position = v->as_bool();

        members.push_back(std::move(m));
      }

      const std::lock_guard lock{data.mutex};
      data.members = std::move(members);
    }
  }
}

void
Glue::OnCompletion(std::exception_ptr error) noexcept
{
  if (error)
    LogError(error, "TeamsTracking error");
}

} // namespace TeamsTracking
