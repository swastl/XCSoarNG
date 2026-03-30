// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "dlgFrequencyEdit.hpp"
#include "Dialogs/RadioFrequencyEntry.hpp"
#include "Dialogs/TextEntry.hpp"
#include "Language/Language.hpp"

bool
dlgFrequencyEditShowModal(UserFrequency &entry)
{
  RadioFrequency freq = entry.frequency;
  if (!RadioFrequencyEntryDialog(_("Frequency"), freq, false))
    return false;
  entry.frequency = freq;

  if (!TextEntryDialog(entry.name, _("Name")))
    return false;

  return true;
}
