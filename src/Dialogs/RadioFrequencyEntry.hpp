// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class RadioFrequency;

bool RadioFrequencyEntryDialog(const char *caption,
                               RadioFrequency &value,
                               bool nullable = false);
