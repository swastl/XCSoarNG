// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "dlgUserFrequencyList.hpp"
#include "dlgFrequencyEdit.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/ListWidget.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "Renderer/WaypointListRenderer.hpp"
#include "ActionInterface.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Look/MapLook.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "RadioFrequency.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"
#include "util/StaticString.hxx"
#include "util/StringSplit.hxx"
#include "LogFile.hpp"
#include "Components.hpp"
#include "BackendComponents.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Engine/Task/TaskManager.hpp"
#include "Engine/Task/Unordered/AlternateList.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Look/Colors.hpp"
#include "Screen/Layout.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif

#include <vector>
#include <string>

static std::vector<UserFrequency> g_user_frequencies;

static void
LoadFromFile() noexcept
{
  g_user_frequencies.clear();

  const auto path = LocalPath("user.freq");

  try {
    FileLineReaderA reader(path);

    char *line;
    while ((line = reader.ReadLine()) != nullptr) {
      /* format: "frequency_string,name" */
      std::string_view sv(line);
      auto [freq_str, name_str] = Split(sv, ',');

      if (freq_str.empty())
        continue;

      RadioFrequency freq = RadioFrequency::Parse(freq_str);
      if (!freq.IsDefined())
        continue;

      UserFrequency entry;
      entry.frequency = freq;
      entry.name = name_str;
      g_user_frequencies.push_back(entry);
    }
  } catch (...) {
    /* ignore file read errors */
  }

  LogDebug("UserFrequency loaded {} frequencies from [{}]",
           g_user_frequencies.size(), path.c_str());
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_DEBUG, "XCSoarFreq",
                      "Loaded %zu frequencies from [%s]",
                      g_user_frequencies.size(), path.c_str());
#endif
}

static void
SaveToFile() noexcept
{
  const auto path = LocalPath("user.freq");

  try {
    FileOutputStream fos(path);
    BufferedOutputStream bos(fos);

    for (const auto &entry : g_user_frequencies) {
      char freq_buf[16];
      entry.frequency.Format(freq_buf, sizeof(freq_buf));
      bos.Write(freq_buf);
      bos.Write(',');
      bos.Write(entry.name.c_str());
      bos.Write('\n');
    }

    bos.Flush();
    fos.Commit();
  } catch (...) {
    /* ignore file write errors */
  }
}

static void
LogFrequencyChange(const char *action, const UserFrequency &entry) noexcept
{
  char freq_buf[16];
  if (entry.frequency.IsDefined())
    entry.frequency.Format(freq_buf, sizeof(freq_buf));
  else
    freq_buf[0] = '\0';

  const auto path = LocalPath("user.freq");
  LogDebug("UserFrequency {}: '{}' {} [{}]",
           action, entry.name.c_str(), freq_buf, path.c_str());
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_DEBUG, "XCSoarFreq",
                      "%s: '%s' %s [%s]",
                      action, entry.name.c_str(), freq_buf, path.c_str());
#endif
}

class UserFrequencyListWidgetImpl final : public ListWidget {
  UserFrequencyListWidget::DialogMode mode;

  TwoTextRowsRenderer row_renderer;

  Button *select_button = nullptr;
  Button *active_button = nullptr;
  Button *standby_button = nullptr;
  Button *add_button = nullptr;
  Button *edit_button = nullptr;
  Button *delete_button = nullptr;

  /** Up to 3 nearest airports with radio frequencies (from alternates list). */
  AlternateList nearby_airports;

public:
  explicit UserFrequencyListWidgetImpl(UserFrequencyListWidget::DialogMode _mode) noexcept
    : mode(_mode) {}

  void CreateButtons(WidgetDialog &dialog) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;

  /* virtual methods from ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned index) noexcept override;

  /* virtual methods from ListCursorHandler */
  bool CanActivateItem(unsigned index) const noexcept override {
    return !IsSeparatorIndex(index);
  }

  void OnActivateItem(unsigned index) noexcept override;

  void OnCursorMoved([[maybe_unused]] unsigned index) noexcept override {
    UpdateButtons();
  }

private:
  void UpdateButtons() noexcept;
  void LoadNearbyAirports() noexcept;

  [[gnu::pure]]
  unsigned GetUserFreqCount() const noexcept {
    return (unsigned)g_user_frequencies.size();
  }

  [[gnu::pure]]
  bool HasNearbyAirports() const noexcept {
    return !nearby_airports.empty();
  }

  /** True if @p index is the separator row between user freqs and nearby airports. */
  [[gnu::pure]]
  bool IsSeparatorIndex(unsigned index) const noexcept {
    return HasNearbyAirports() && index == GetUserFreqCount();
  }

  /** True if @p index refers to a nearby airport row. */
  [[gnu::pure]]
  bool IsAirportIndex(unsigned index) const noexcept {
    return HasNearbyAirports() && index > GetUserFreqCount();
  }

  /** Convert list index to index into nearby_airports. */
  [[gnu::pure]]
  unsigned GetAirportListIndex(unsigned index) const noexcept {
    return index - GetUserFreqCount() - 1;
  }

  [[gnu::pure]]
  unsigned GetTotalCount() const noexcept {
    if (HasNearbyAirports())
      return GetUserFreqCount() + 1 + (unsigned)nearby_airports.size();
    return GetUserFreqCount();
  }

  /**
   * Get frequency and name for the item at @p index.
   * Returns false if the item has no valid frequency (e.g. separator).
   */
  bool GetFrequencyAtIndex(unsigned index,
                           RadioFrequency &freq,
                           std::string &name) const noexcept;
};

void
UserFrequencyListWidgetImpl::CreateButtons(WidgetDialog &dialog) noexcept
{
  if (mode == UserFrequencyListWidget::DialogMode::BROWSE) {
    add_button = dialog.AddButton(_("Add"), [this]() {
      UserFrequency entry;
      entry.name = "";
      entry.frequency = RadioFrequency::Null();
      if (dlgFrequencyEditShowModal(entry)) {
        LogFrequencyChange("added", entry);
        g_user_frequencies.push_back(entry);
        SaveToFile();
        GetList().SetLength(g_user_frequencies.size());
        GetList().SetCursorIndex(g_user_frequencies.size() - 1);
        GetList().Invalidate();
        UpdateButtons();
      }
    });

    edit_button = dialog.AddButton(_("Edit"), [this]() {
      const unsigned index = GetList().GetCursorIndex();
      if (index >= g_user_frequencies.size())
        return;
      UserFrequency entry = g_user_frequencies[index];
      if (dlgFrequencyEditShowModal(entry)) {
        LogFrequencyChange("edited", entry);
        g_user_frequencies[index] = entry;
        SaveToFile();
        GetList().Invalidate();
      }
    });

    delete_button = dialog.AddButton(_("Delete"), [this]() {
      const unsigned index = GetList().GetCursorIndex();
      if (index >= g_user_frequencies.size())
        return;
      LogFrequencyChange("deleted", g_user_frequencies[index]);
      g_user_frequencies.erase(g_user_frequencies.begin() + index);
      SaveToFile();
      GetList().SetLength(g_user_frequencies.size());
      UpdateButtons();
    });

    dialog.AddButton(_("Close"), mrCancel);
  } else if (mode == UserFrequencyListWidget::DialogMode::SELECT_BOTH) {
    auto on_select = [this, &dialog](bool for_active) {
      const unsigned index = GetList().GetCursorIndex();
      RadioFrequency freq;
      std::string name;
      if (!GetFrequencyAtIndex(index, freq, name))
        return;

      if (for_active)
        ActionInterface::SetActiveFrequency(freq, name.c_str());
      else
        ActionInterface::SetStandbyFrequency(freq, name.c_str());
      dialog.SetModalResult(mrOK);
    };

    active_button = dialog.AddButton(_("Active"),
                                     [on_select]() { on_select(true); });
    standby_button = dialog.AddButton(_("Standby"),
                                      [on_select]() { on_select(false); });
    dialog.AddButton(_("Cancel"), mrCancel);
  } else {
    select_button = dialog.AddButton(_("Select"), [this, &dialog]() {
      const unsigned index = GetList().GetCursorIndex();
      RadioFrequency freq;
      std::string name;
      if (!GetFrequencyAtIndex(index, freq, name))
        return;

      if (mode == UserFrequencyListWidget::DialogMode::SELECT_ACTIVE)
        ActionInterface::SetActiveFrequency(freq, name.c_str());
      else
        ActionInterface::SetStandbyFrequency(freq, name.c_str());
      dialog.SetModalResult(mrOK);
    });

    dialog.AddButton(_("Cancel"), mrCancel);
  }
}

void
UserFrequencyListWidgetImpl::Prepare(ContainerWindow &parent,
                                     const PixelRect &rc) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  CreateList(parent, look, rc,
             row_renderer.CalculateLayout(*look.list.font_bold,
                                          look.small_font));

  if (mode != UserFrequencyListWidget::DialogMode::BROWSE)
    LoadNearbyAirports();

  GetList().SetLength(GetTotalCount());
  UpdateButtons();
}

void
UserFrequencyListWidgetImpl::OnPaintItem(Canvas &canvas, const PixelRect rc,
                                         unsigned index) noexcept
{
  if (IsSeparatorIndex(index)) {
    /* draw a separator line with "Nearby Airports" label */
    const DialogLook &look = UIGlobals::GetDialogLook();
    const unsigned mid_y = (rc.top + rc.bottom) / 2;
    const unsigned padding = Layout::GetTextPadding();

    canvas.Select(look.small_font);
    const auto label = _("Nearby Airports");
    const unsigned text_width = canvas.CalcTextWidth(label);

    /* horizontal line left side */
    canvas.DrawHLine(rc.left + padding, rc.left + padding + 20,
                     mid_y, COLOR_GRAY);
    /* label */
    const unsigned text_x = rc.left + padding + 24;
    canvas.SetTextColor(COLOR_GRAY);
    canvas.SetBackgroundTransparent();
    canvas.DrawText({(int)text_x, (int)(mid_y - look.small_font.GetHeight() / 2)}, label);
    /* horizontal line right side */
    const unsigned right_x = text_x + text_width + 4;
    if ((int)right_x < rc.right - (int)padding)
      canvas.DrawHLine(right_x, rc.right - padding, mid_y, COLOR_GRAY);
    return;
  }

  if (IsAirportIndex(index)) {
    const unsigned ai = GetAirportListIndex(index);
    if (ai >= nearby_airports.size())
      return;

    const ComputerSettings &settings = CommonInterface::GetComputerSettings();
    const auto &alt = nearby_airports[ai];
    WaypointListRenderer::Draw(canvas, rc, *alt.waypoint,
                               alt.solution.vector.distance,
                               alt.solution.SelectAltitudeDifference(settings.task.glide),
                               row_renderer,
                               UIGlobals::GetMapLook().waypoint,
                               CommonInterface::GetMapSettings().waypoint);
    return;
  }

  if (index >= g_user_frequencies.size())
    return;

  const auto &entry = g_user_frequencies[index];

  char freq_buf[16];
  if (entry.frequency.IsDefined())
    entry.frequency.Format(freq_buf, sizeof(freq_buf));
  else
    freq_buf[0] = '\0';

  row_renderer.DrawFirstRow(canvas, rc, entry.name.c_str());
  row_renderer.DrawSecondRow(canvas, rc, freq_buf);
}

void
UserFrequencyListWidgetImpl::OnActivateItem([[maybe_unused]] unsigned index) noexcept
{
  if (IsSeparatorIndex(index))
    return;

  if (mode == UserFrequencyListWidget::DialogMode::BROWSE) {
    if (edit_button != nullptr && !IsAirportIndex(index))
      edit_button->Click();
  } else if (mode != UserFrequencyListWidget::DialogMode::SELECT_BOTH) {
    if (select_button != nullptr)
      select_button->Click();
  }
  /* SELECT_BOTH: no default action; user must press Active or Standby explicitly */
}

void
UserFrequencyListWidgetImpl::UpdateButtons() noexcept
{
  const unsigned cursor = GetList().GetCursorIndex();
  const bool is_separator = IsSeparatorIndex(cursor);
  const bool is_airport = IsAirportIndex(cursor);
  const bool is_user_freq = !is_separator && !is_airport;

  const bool has_user_selection = is_user_freq &&
    !g_user_frequencies.empty() &&
    cursor < g_user_frequencies.size();

  bool has_airport_freq = false;
  if (is_airport) {
    const unsigned ai = GetAirportListIndex(cursor);
    if (ai < nearby_airports.size())
      has_airport_freq = nearby_airports[ai].waypoint->radio_frequency.IsDefined();
  }

  const bool can_select = has_user_selection || has_airport_freq;

  if (select_button != nullptr)
    select_button->SetEnabled(can_select);
  if (active_button != nullptr)
    active_button->SetEnabled(can_select);
  if (standby_button != nullptr)
    standby_button->SetEnabled(can_select);
  if (edit_button != nullptr)
    edit_button->SetEnabled(has_user_selection);
  if (delete_button != nullptr)
    delete_button->SetEnabled(has_user_selection);
}

void
UserFrequencyListWidgetImpl::LoadNearbyAirports() noexcept
{
  nearby_airports.clear();

  if (backend_components == nullptr ||
      backend_components->protected_task_manager == nullptr)
    return;

  ProtectedTaskManager::Lease lease(*backend_components->protected_task_manager);
  const AlternateList &alternates = lease->GetAlternates();

  for (const auto &alt : alternates) {
    if (nearby_airports.size() >= 3)
      break;
    if (alt.waypoint->radio_frequency.IsDefined())
      nearby_airports.push_back(alt);
  }
}

bool
UserFrequencyListWidgetImpl::GetFrequencyAtIndex(unsigned index,
                                                  RadioFrequency &freq,
                                                  std::string &name) const noexcept
{
  if (IsSeparatorIndex(index))
    return false;

  if (IsAirportIndex(index)) {
    const unsigned ai = GetAirportListIndex(index);
    if (ai >= nearby_airports.size())
      return false;
    const auto &wp = *nearby_airports[ai].waypoint;
    freq = wp.radio_frequency;
    name = wp.name;
  } else {
    if (index >= g_user_frequencies.size())
      return false;
    const auto &entry = g_user_frequencies[index];
    freq = entry.frequency;
    name = entry.name.c_str();
  }

  return freq.IsDefined();
}

void
dlgUserFrequencyListWidgetShowModal(UserFrequencyListWidget::DialogMode mode)
{
  LoadFromFile();

  const DialogLook &look = UIGlobals::GetDialogLook();

  auto widget = std::make_unique<UserFrequencyListWidgetImpl>(mode);
  auto *widget_ptr = widget.get();

  TWidgetDialog<UserFrequencyListWidgetImpl>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(), look,
           _("Frequencies"));

  widget_ptr->CreateButtons(dialog);
  dialog.FinishPreliminary(std::move(widget));
  dialog.ShowModal();
}
