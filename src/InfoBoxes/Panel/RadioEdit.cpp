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
  void OnGPSUpdate(const MoreData &basic) override;

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

void RadioEdit::OnGPSUpdate([[maybe_unused]] const MoreData &basic)
{
  /* GPS update fires each device data cycle - refresh the frequency display
     so it picks up any changes reported by the radio hardware */
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
  ComputerSettings &settings =
      CommonInterface::SetComputerSettings();
  if (set_active_freq)
    return settings.radio.active_frequency;
  else
    return settings.radio.standby_frequency;
}

void RadioEdit::OnEditFrequency() noexcept
{
  RadioFrequency freq = GetCurrentFrequency();
  if (!RadioFrequencyEntryDialog(_("Frequency"), freq, false))
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
