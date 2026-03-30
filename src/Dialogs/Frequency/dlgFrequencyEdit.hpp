// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "RadioFrequency.hpp"
#include "util/StaticString.hxx"

struct UserFrequency {
  StaticString<64> name;
  RadioFrequency frequency;
};

bool dlgFrequencyEditShowModal(UserFrequency &entry);
