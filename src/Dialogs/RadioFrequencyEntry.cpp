// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "RadioFrequencyEntry.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/FixedWindowWidget.hpp"
#include "Form/DigitEntry.hpp"
#include "RadioFrequency.hpp"
#include "Language/Language.hpp"
#include "UIGlobals.hpp"

bool
RadioFrequencyEntryDialog(const char *caption,
                          RadioFrequency &value,
                          bool nullable)
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<FixedWindowWidget>
    dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look, caption);

  ContainerWindow &client_area = dialog.GetClientAreaWindow();

  WindowStyle control_style;
  control_style.Hide();
  control_style.TabStop();

  auto entry = std::make_unique<DigitEntry>(look);
  entry->CreateRadioFrequency(client_area, client_area.GetClientRect(),
                              control_style);
  entry->Resize(entry->GetRecommendedSize());
  if (value.IsDefined())
    entry->SetValue(value);
  entry->SetCallback(dialog.MakeModalResultCallback(mrOK));

  dialog.AddButton(_("OK"), mrOK);
  if (nullable)
    dialog.AddButton(_("Clear"), [&dialog, &value](){
      value = RadioFrequency::Null();
      dialog.SetModalResult(mrOK);
    });
  dialog.AddButton(_("Cancel"), mrCancel);

  dialog.SetWidget(std::move(entry));

  bool result = dialog.ShowModal() == mrOK;
  if (!result)
    return false;

  /* only update value if not already set by the Clear button;
     in non-nullable mode always read from the digit entry (even when the
     initial value was undefined), but in nullable mode skip the read when
     Clear was pressed (which sets value to Null before returning) */
  if (value.IsDefined() || !nullable) {
    auto &digit_entry = (DigitEntry &)dialog.GetWidget().GetWindow();
    RadioFrequency new_value = digit_entry.GetRadioFrequency();
    if (new_value.IsDefined())
      value = new_value;
  }

  return true;
}
