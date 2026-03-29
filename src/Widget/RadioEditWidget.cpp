// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "RadioEditWidget.hpp"
#include "Look/DialogLook.hpp"
#include "Screen/Layout.hpp"
#include "Language/Language.hpp"

#include <algorithm>
#include <stdio.h>

static constexpr std::array<PixelRect, RadioEditWidget::NUM_BUTTONS>
LayoutButtonsImpl(const PixelRect &rc) noexcept
{
  const int mid_y = (rc.top + rc.bottom) / 2;
  const unsigned row1_width = rc.GetWidth();
  const unsigned row2_width = rc.GetWidth();

  std::array<PixelRect, RadioEditWidget::NUM_BUTTONS> result{};

  /* top row: 5 buttons */
  for (unsigned i = 0; i < 5; ++i) {
    result[i].top = rc.top;
    result[i].bottom = mid_y;
    result[i].left = rc.left + i * row1_width / 5;
    result[i].right = rc.left + (i + 1) * row1_width / 5;
  }

  /* bottom row: 2 buttons */
  for (unsigned i = 0; i < 2; ++i) {
    result[5 + i].top = mid_y;
    result[5 + i].bottom = rc.bottom;
    result[5 + i].left = rc.left + i * row2_width / 2;
    result[5 + i].right = rc.left + (i + 1) * row2_width / 2;
  }

  return result;
}

std::array<PixelRect, RadioEditWidget::NUM_BUTTONS>
RadioEditWidget::LayoutButtons(const PixelRect &rc) noexcept
{
  return LayoutButtonsImpl(rc);
}

PixelSize
RadioEditWidget::GetMinimumSize() const noexcept
{
  return { 5u * Layout::GetMinimumControlHeight(),
      2u * Layout::GetMinimumControlHeight() };
}

PixelSize
RadioEditWidget::GetMaximumSize() const noexcept
{
  return { 5u * Layout::GetMaximumControlHeight(),
      2u * Layout::GetMaximumControlHeight() };
}

void
RadioEditWidget::Prepare(ContainerWindow &parent,
                         const PixelRect &rc) noexcept
{
  const auto rects = LayoutButtons(rc);
  const auto &button_look = look.button;

  WindowStyle style;
  style.TabStop();
  style.Hide();

  buttons.reset(new std::array<Button, NUM_BUTTONS>{
    Button(parent, button_look, "-1M", rects[0], style,
           [this](){ OnOffset(-1000); }),
    Button(parent, button_look, "-5k", rects[1], style,
           [this](){ OnOffset(-5); }),
    Button(parent, button_look, "---", rects[2], style,
           [this](){ OnEditFrequency(); }),
    Button(parent, button_look, "+5k", rects[3], style,
           [this](){ OnOffset(+5); }),
    Button(parent, button_look, "+1M", rects[4], style,
           [this](){ OnOffset(+1000); }),
    Button(parent, button_look, _("Swap"), rects[5], style,
           [this](){ OnSwapFrequency(); }),
    Button(parent, button_look, _("List"), rects[6], style,
           [this](){ OnOpenList(); }),
  });

  UpdateFrequencyField(GetCurrentFrequency());
}

void
RadioEditWidget::UpdateFrequencyField(RadioFrequency freq) noexcept
{
  if (freq.IsDefined())
    freq.Format(freq_text.buffer(), freq_text.capacity());
  else
    freq_text = "---";

  if (buttons)
    (*buttons)[2].SetCaption(freq_text.c_str());
}

void
RadioEditWidget::Show(const PixelRect &rc) noexcept
{
  const auto rects = LayoutButtons(rc);
  for (unsigned i = 0; i < NUM_BUTTONS; ++i)
    (*buttons)[i].MoveAndShow(rects[i]);
}

void
RadioEditWidget::Hide() noexcept
{
  for (auto &b : *buttons)
    b.Hide();
}

void
RadioEditWidget::Move(const PixelRect &rc) noexcept
{
  const auto rects = LayoutButtons(rc);
  for (unsigned i = 0; i < NUM_BUTTONS; ++i)
    (*buttons)[i].Move(rects[i]);
}

bool
RadioEditWidget::SetFocus() noexcept
{
  (*buttons)[2].SetFocus();
  return true;
}

bool
RadioEditWidget::HasFocus() const noexcept
{
  return std::any_of(buttons->begin(), buttons->end(), [](const Button &b){
    return b.HasFocus();
  });
}

void
RadioEditWidget::OnOffset(int offset_khz) noexcept
{
  RadioFrequency freq = GetCurrentFrequency();
  if (!freq.IsDefined())
    return;

  freq.OffsetKiloHertz(offset_khz);
  if (!freq.IsDefined())
    return;

  OnFrequencyChanged(freq);
  UpdateFrequencyField(freq);
}
