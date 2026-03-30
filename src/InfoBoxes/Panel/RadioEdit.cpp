// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "RadioEdit.hpp"
#include "Look/DialogLook.hpp"
#include "Widget/RadioEditWidget.hpp"
#include "Formatter/UserUnits.hpp"
#include "ActionInterface.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "Blackboard/BlackboardListener.hpp"
#include "Dialogs/Frequency/dlgUserFrequencyList.hpp"
#include "Dialogs/RadioFrequencyEntry.hpp"
#include "Language/Language.hpp"

#include <utility>

class RadioEdit final : public RadioEditWidget, NullBlackboardListener
{
public:
  explicit RadioEdit(bool active_freq) noexcept
      : RadioEditWidget(UIGlobals::GetDialogLook()), set_active_freq(active_freq) {}

  /* overrides from Widget */
  void Show(const PixelRect &rc) noexcept override;
  void Hide() noexcept override;

protected:
  /* virtual methods from RadioEditWidget */
  void OnEditFrequency() noexcept override;
  void OnOpenList() noexcept override;
  void OnSwapFrequency() noexcept override;

  /* virtual methods from BlackboardListener */
  void OnCalculatedUpdate(const MoreData &basic,
                          const DerivedInfo &calculated) override;

private:
  RadioFrequency GetFrequency(bool for_active) const noexcept;
  void RefreshDisplay() noexcept;

  const bool set_active_freq;
};

std::unique_ptr<Widget>
LoadActiveRadioFrequencyEditPanel([[maybe_unused]] unsigned id)
{
  return std::make_unique<RadioEdit>(true);
}

std::unique_ptr<Widget>
LoadStandbyRadioFrequencyEditPanel([[maybe_unused]] unsigned id)
{
  return std::make_unique<RadioEdit>(false);
}

void RadioEdit::Show(const PixelRect &rc) noexcept
{
  RadioEditWidget::Show(rc);
  RefreshDisplay();
  CommonInterface::GetLiveBlackboard().AddListener(*this);
}

void RadioEdit::Hide() noexcept
{
  CommonInterface::GetLiveBlackboard().RemoveListener(*this);
  RadioEditWidget::Hide();
}

void RadioEdit::OnCalculatedUpdate([[maybe_unused]] const MoreData &basic,
                                   [[maybe_unused]] const DerivedInfo &calculated)
{
  /* OnCalculatedUpdate fires after ApplyExternalSettings has run, so
     ComputerSettings already reflects any frequency the device reported */
  RefreshDisplay();
}

RadioFrequency RadioEdit::GetFrequency(bool for_active) const noexcept
{
  const ComputerSettings &settings = CommonInterface::GetComputerSettings();
  const RadioFrequency stored = for_active
    ? settings.radio.active_frequency
    : settings.radio.standby_frequency;

  if (stored.IsDefined())
    return stored;

  /* Fallback: read the live frequency reported by the radio device
     directly from NMEAInfo, in case ComputerSettings has not yet been
     populated (e.g. before the computer thread runs ApplyExternalSettings) */
  const MoreData &basic = CommonInterface::Basic();
  if (for_active) {
    if (basic.settings.has_active_frequency.IsValid())
      return basic.settings.active_frequency;
  } else {
    if (basic.settings.has_standby_frequency.IsValid())
      return basic.settings.standby_frequency;
  }

  return RadioFrequency::Null();
}

void RadioEdit::RefreshDisplay() noexcept
{
  UpdateFrequencyField(GetFrequency(true), GetFrequency(false));

  const MoreData &basic = CommonInterface::Basic();
  const bool radio_connected = basic.settings.has_active_frequency ||
                                basic.settings.has_standby_frequency;
  SetSwapEnabled(radio_connected);
}

void RadioEdit::OnOpenList() noexcept
{
  dlgUserFrequencyListWidgetShowModal(UserFrequencyListWidget::DialogMode::SELECT_BOTH);
  RefreshDisplay();
}

void RadioEdit::OnEditFrequency() noexcept
{
  /* Pre-fill with the current frequency for the panel type */
  RadioFrequency freq = GetFrequency(set_active_freq);
  bool set_active = set_active_freq;

  if (!RadioFrequencyEntryDialogWithTarget(_("Frequency"), freq, set_active))
    return;
  if (!freq.IsDefined())
    return;

  if (set_active)
    ActionInterface::SetActiveFrequency(freq, "");
  else
    ActionInterface::SetStandbyFrequency(freq, "");

  RefreshDisplay();
}

void RadioEdit::OnSwapFrequency() noexcept
{
  /* ExchangeRadioFrequencies only sends a command to the external device
     but never updates ComputerSettings.  Swap the in-memory values directly
     so the display refreshes immediately (even when no device is connected). */
  auto &radio = CommonInterface::SetComputerSettings().radio;
  std::swap(radio.active_frequency, radio.standby_frequency);
  std::swap(radio.active_name,      radio.standby_name);

  /* Also tell any connected device to swap */
  ActionInterface::ExchangeRadioFrequencies(true);

  RefreshDisplay();
}
