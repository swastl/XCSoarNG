// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "RadioEditWidget.hpp"
#include "Renderer/ButtonRenderer.hpp"
#include "Renderer/TextRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Font.hpp"
#include "Look/DialogLook.hpp"
#include "Look/ButtonLook.hpp"
#include "Screen/Layout.hpp"
#include "Language/Language.hpp"
#include "util/StaticString.hxx"

#include <algorithm>
#include <stdio.h>
#include <string.h>

/**
 * Custom button renderer that shows two lines of text:
 * the active frequency in the upper portion (button font) and
 * the standby frequency in the lower portion (smaller font).
 */
class DualFrequencyButtonRenderer final : public ButtonRenderer {
  ButtonFrameRenderer frame_renderer;
  const Font &main_font;
  const Font &sub_font;

public:
  StaticString<16> active_text{"---"};
  StaticString<16> standby_text{"---"};

  DualFrequencyButtonRenderer(const ButtonLook &look,
                               const Font &_main_font,
                               const Font &_sub_font) noexcept
    : frame_renderer(look), main_font(_main_font), sub_font(_sub_font) {}

  unsigned GetMinimumButtonWidth() const noexcept override {
    return ButtonFrameRenderer::GetMargin() * 2 + Layout::GetTextPadding() * 2;
  }

  void DrawButton(Canvas &canvas, const PixelRect &rc,
                  ButtonState state) const noexcept override;
};

void
DualFrequencyButtonRenderer::DrawButton(Canvas &canvas, const PixelRect &rc,
                                        ButtonState state) const noexcept
{
  frame_renderer.DrawButton(canvas, rc, state);

  const PixelRect inner = frame_renderer.GetDrawingRect(rc, state);

  canvas.SetBackgroundTransparent();

  const ButtonLook &look = frame_renderer.GetLook();
  switch (state) {
  case ButtonState::DISABLED:
    canvas.SetTextColor(look.disabled.color);
    break;
  case ButtonState::FOCUSED:
  case ButtonState::PRESSED:
    canvas.SetTextColor(look.focused.foreground_color);
    break;
  case ButtonState::SELECTED:
    canvas.SetTextColor(look.selected.foreground_color);
    break;
  case ButtonState::ENABLED:
    canvas.SetTextColor(look.standard.foreground_color);
    break;
  }

  const unsigned h = inner.GetHeight();
  const int main_h = static_cast<int>(main_font.GetHeight());
  const int sub_h  = static_cast<int>(sub_font.GetHeight());
  const int total  = main_h + sub_h + static_cast<int>(Layout::GetTextPadding());

  /* vertically center the two lines as a block */
  const int block_top = inner.top + (static_cast<int>(h) - total) / 2;

  PixelRect upper = inner;
  upper.top    = std::max(inner.top, block_top);
  upper.bottom = upper.top + main_h;

  PixelRect lower = inner;
  lower.top    = upper.bottom + static_cast<int>(Layout::GetTextPadding());
  lower.bottom = std::min(inner.bottom, lower.top + sub_h);

  /* active frequency — main (bold) font, centered */
  canvas.Select(main_font);
  TextRenderer tr;
  tr.SetCenter();
  tr.Draw(canvas, upper, active_text.c_str());

  /* standby frequency — small font, centered */
  canvas.Select(sub_font);
  TextRenderer tr2;
  tr2.SetCenter();
  tr2.Draw(canvas, lower, standby_text.c_str());
}

// ---------------------------------------------------------------------------

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

  auto renderer = std::make_unique<DualFrequencyButtonRenderer>(
    button_look, *button_look.font, look.small_font);
  freq_renderer = renderer.get();

  buttons.reset(new std::array<Button, NUM_BUTTONS>{
    Button(parent, rects[0], style, std::move(renderer),
           [this](){ OnEditFrequency(); }),
    Button(parent, button_look, _("List"), rects[1], style,
           [this](){ OnOpenList(); }),
    Button(parent, button_look, _("Swap"), rects[2], style,
           [this](){ OnSwapFrequency(); }),
  });
}

void
RadioEditWidget::SetSwapEnabled(bool enabled) noexcept
{
  if (buttons)
    (*buttons)[SWAP_BUTTON_INDEX].SetEnabled(enabled);
}

void
RadioEditWidget::UpdateFrequencyField(RadioFrequency active,
                                      RadioFrequency standby) noexcept
{
  if (!freq_renderer || !buttons)
    return;

  char active_buf[16], standby_buf[16];

  if (active.IsDefined())
    active.Format(active_buf, sizeof(active_buf));
  else
    strcpy(active_buf, "---");

  if (standby.IsDefined())
    standby.Format(standby_buf, sizeof(standby_buf));
  else
    strcpy(standby_buf, "---");

  freq_renderer->active_text  = active_buf;
  freq_renderer->standby_text = standby_buf;
  (*buttons)[0].Invalidate();
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
