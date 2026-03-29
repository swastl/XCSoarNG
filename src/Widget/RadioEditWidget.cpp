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
  const unsigned width = rc.GetWidth();

  std::array<PixelRect, RadioEditWidget::NUM_BUTTONS> result{};

  /* single row: 3 buttons */
  for (unsigned i = 0; i < RadioEditWidget::NUM_BUTTONS; ++i) {
    result[i].top = rc.top;
    result[i].bottom = rc.bottom;
    result[i].left = rc.left + i * width / RadioEditWidget::NUM_BUTTONS;
    result[i].right = rc.left + (i + 1) * width / RadioEditWidget::NUM_BUTTONS;
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
  return { 3u * Layout::GetMinimumControlHeight(),
      Layout::GetMinimumControlHeight() };
}

PixelSize
RadioEditWidget::GetMaximumSize() const noexcept
{
  return { 3u * Layout::GetMaximumControlHeight(),
      Layout::GetMaximumControlHeight() };
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
    Button(parent, button_look, "---", rects[0], style,
           [this](){ OnEditFrequency(); }),
    Button(parent, button_look, _("List"), rects[1], style,
           [this](){ OnOpenList(); }),
    Button(parent, button_look, _("Swap"), rects[2], style,
           [this](){ OnSwapFrequency(); }),
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
    (*buttons)[0].SetCaption(freq_text.c_str());
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
  (*buttons)[0].SetFocus();
  return true;
}

bool
RadioEditWidget::HasFocus() const noexcept
{
  return std::any_of(buttons->begin(), buttons->end(), [](const Button &b){
    return b.HasFocus();
  });
}
