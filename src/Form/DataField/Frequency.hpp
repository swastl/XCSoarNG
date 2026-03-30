// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Base.hpp"
#include "RadioFrequency.hpp"

class RadioFrequencyDataField : public DataField {
  RadioFrequency value;
  mutable char output_buffer[OUTBUFFERSIZE + 1];

public:
  RadioFrequencyDataField(RadioFrequency _value,
                          DataFieldListener *listener = nullptr) noexcept
    : DataField(Type::RADIO_FREQUENCY, false, listener), value(_value) {}

  RadioFrequency GetValue() const noexcept { return value; }

  void SetValue(RadioFrequency _value) noexcept { value = _value; }

  void ModifyValue(RadioFrequency new_value) noexcept {
    if (new_value != value) {
      value = new_value;
      Modified();
    }
  }

  /* virtual methods from class DataField */
  const char *GetAsString() const noexcept override;
  const char *GetAsDisplayString() const noexcept override;
};
