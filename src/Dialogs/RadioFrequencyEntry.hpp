// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class RadioFrequency;

bool RadioFrequencyEntryDialog(const char *caption,
                               RadioFrequency &value,
                               bool nullable = false);

/**
 * Open a frequency entry dialog with "Active" and "Standby" buttons
 * instead of "OK"/"Cancel".  Returns false if the user cancelled,
 * true otherwise.  On success, \p set_active is set to true if the
 * user pressed "Active", or false for "Standby".
 */
bool RadioFrequencyEntryDialogWithTarget(const char *caption,
                                         RadioFrequency &value,
                                         bool &set_active) noexcept;
