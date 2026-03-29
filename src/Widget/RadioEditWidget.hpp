// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Widget.hpp"
#include "Form/Button.hpp"
#include "RadioFrequency.hpp"
#include "util/StaticString.hxx"

#include <array>
#include <memory>

struct DialogLook;

/**
 * A widget that shows three buttons for a radio frequency:
 * current frequency display, select from list, and swap.
 */
class RadioEditWidget : public NullWidget {
public:
  static constexpr unsigned NUM_BUTTONS = 3;

private:
  const DialogLook &look;

  std::unique_ptr<std::array<Button, NUM_BUTTONS>> buttons;

  StaticString<16> freq_text;

public:
  explicit RadioEditWidget(const DialogLook &_look) noexcept
    : look(_look) {}

  void UpdateFrequencyField(RadioFrequency freq) noexcept;

  /* virtual methods from Widget */
  PixelSize GetMinimumSize() const noexcept override;
  PixelSize GetMaximumSize() const noexcept override;
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Show(const PixelRect &rc) noexcept override;
  void Hide() noexcept override;
  void Move(const PixelRect &rc) noexcept override;
  bool SetFocus() noexcept override;
  bool HasFocus() const noexcept override;

protected:
  virtual void OnEditFrequency() noexcept = 0;
  virtual void OnOpenList() noexcept = 0;
  virtual void OnSwapFrequency() noexcept = 0;
  virtual RadioFrequency GetCurrentFrequency() const noexcept = 0;

private:
  static std::array<PixelRect, NUM_BUTTONS>
  LayoutButtons(const PixelRect &rc) noexcept;
};
