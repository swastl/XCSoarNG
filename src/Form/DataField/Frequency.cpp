// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Frequency.hpp"

const char *
RadioFrequencyDataField::GetAsString() const noexcept
{
  if (!value.IsDefined()) {
    output_buffer[0] = '\0';
    return output_buffer;
  }
  value.Format(output_buffer, sizeof(output_buffer));
  return output_buffer;
}

const char *
RadioFrequencyDataField::GetAsDisplayString() const noexcept
{
  return GetAsString();
}
