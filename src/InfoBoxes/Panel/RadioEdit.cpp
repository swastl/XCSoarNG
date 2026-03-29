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
  RadioFrequency GetCurrentFrequency() const noexcept override;

  /* virtual methods from BlackboardListener */
  void OnCalculatedUpdate(const MoreData &basic,
                          const DerivedInfo &calculated) override;

private:
  void ApplyFrequency(RadioFrequency freq) noexcept;
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
  UpdateFrequencyField(GetCurrentFrequency());
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
  UpdateFrequencyField(GetCurrentFrequency());
}

void RadioEdit::ApplyFrequency(RadioFrequency freq) noexcept
{
  if (set_active_freq)
    ActionInterface::SetActiveFrequency(freq, "");
  else
    ActionInterface::SetStandbyFrequency(freq, "");
}

RadioFrequency RadioEdit::GetCurrentFrequency() const noexcept
{
  const ComputerSettings &settings = CommonInterface::GetComputerSettings();
  const RadioFrequency stored = set_active_freq
    ? settings.radio.active_frequency
    : settings.radio.standby_frequency;

  if (stored.IsDefined())
    return stored;

  /* Fallback: read the live frequency reported by the radio device
     directly from NMEAInfo, in case ComputerSettings has not yet been
     populated (e.g. before the computer thread runs ApplyExternalSettings) */
  const MoreData &basic = CommonInterface::Basic();
  if (set_active_freq) {
    if (basic.settings.has_active_frequency.IsValid())
      return basic.settings.active_frequency;
  } else {
    if (basic.settings.has_standby_frequency.IsValid())
      return basic.settings.standby_frequency;
  }

  return RadioFrequency::Null();
}

void RadioEdit::OnEditFrequency() noexcept
{
  RadioFrequency freq = GetCurrentFrequency();
  if (!RadioFrequencyEntryDialog(_("Frequency"), freq, false))
    return;
  if (!freq.IsDefined())
    return;
  ApplyFrequency(freq);
  UpdateFrequencyField(freq);
}

void RadioEdit::OnOpenList() noexcept
{
  auto mode = UserFrequencyListWidget::DialogMode::SELECT_ACTIVE;
  if (!set_active_freq)
    mode = UserFrequencyListWidget::DialogMode::SELECT_STANDBY;
  dlgUserFrequencyListWidgetShowModal(mode);

  UpdateFrequencyField(GetCurrentFrequency());
}

void RadioEdit::OnSwapFrequency() noexcept
{
  ActionInterface::ExchangeRadioFrequencies(true);
}
