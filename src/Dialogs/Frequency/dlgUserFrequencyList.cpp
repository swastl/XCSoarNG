// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "dlgUserFrequencyList.hpp"
#include "dlgFrequencyEdit.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/ListWidget.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "ActionInterface.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "RadioFrequency.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"
#include "util/StaticString.hxx"
#include "util/StringSplit.hxx"

#include <vector>
#include <string>

static std::vector<UserFrequency> g_user_frequencies;

static void
LoadFromFile() noexcept
{
  g_user_frequencies.clear();

  const auto path = LocalPath("user.freq");

  try {
    FileLineReaderA reader(path);

    char *line;
    while ((line = reader.ReadLine()) != nullptr) {
      /* format: "frequency_string,name" */
      std::string_view sv(line);
      auto [freq_str, name_str] = Split(sv, ',');

      if (freq_str.empty())
        continue;

      RadioFrequency freq = RadioFrequency::Parse(freq_str);
      if (!freq.IsDefined())
        continue;

      UserFrequency entry;
      entry.frequency = freq;
      entry.name = name_str;
      g_user_frequencies.push_back(entry);
    }
  } catch (...) {
    /* ignore file read errors */
  }
}

static void
SaveToFile() noexcept
{
  const auto path = LocalPath("user.freq");

  try {
    FileOutputStream fos(path);
    BufferedOutputStream bos(fos);

    for (const auto &entry : g_user_frequencies) {
      char freq_buf[16];
      entry.frequency.Format(freq_buf, sizeof(freq_buf));
      bos.Write(freq_buf);
      bos.Write(',');
      bos.Write(entry.name.c_str());
      bos.Write('\n');
    }

    bos.Flush();
    fos.Commit();
  } catch (...) {
    /* ignore file write errors */
  }
}

class UserFrequencyListWidgetImpl final : public ListWidget {
  UserFrequencyListWidget::DialogMode mode;

  TwoTextRowsRenderer row_renderer;

  Button *select_button = nullptr;
  Button *add_button = nullptr;
  Button *edit_button = nullptr;
  Button *delete_button = nullptr;

public:
  explicit UserFrequencyListWidgetImpl(UserFrequencyListWidget::DialogMode _mode) noexcept
    : mode(_mode) {}

  void CreateButtons(WidgetDialog &dialog) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;

  /* virtual methods from ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned index) noexcept override;

  /* virtual methods from ListCursorHandler */
  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem(unsigned index) noexcept override;

  void OnCursorMoved([[maybe_unused]] unsigned index) noexcept override {
    UpdateButtons();
  }

private:
  void UpdateButtons() noexcept;
};

void
UserFrequencyListWidgetImpl::CreateButtons(WidgetDialog &dialog) noexcept
{
  if (mode == UserFrequencyListWidget::DialogMode::BROWSE) {
    add_button = dialog.AddButton(_("Add"), [this]() {
      UserFrequency entry;
      entry.name = "";
      entry.frequency = RadioFrequency::Null();
      if (dlgFrequencyEditShowModal(entry)) {
        g_user_frequencies.push_back(entry);
        SaveToFile();
        GetList().SetLength(g_user_frequencies.size());
        GetList().SetCursorIndex(g_user_frequencies.size() - 1);
        UpdateButtons();
      }
    });

    edit_button = dialog.AddButton(_("Edit"), [this]() {
      const unsigned index = GetList().GetCursorIndex();
      if (index >= g_user_frequencies.size())
        return;
      UserFrequency entry = g_user_frequencies[index];
      if (dlgFrequencyEditShowModal(entry)) {
        g_user_frequencies[index] = entry;
        SaveToFile();
        GetList().Invalidate();
      }
    });

    delete_button = dialog.AddButton(_("Delete"), [this]() {
      const unsigned index = GetList().GetCursorIndex();
      if (index >= g_user_frequencies.size())
        return;
      g_user_frequencies.erase(g_user_frequencies.begin() + index);
      SaveToFile();
      GetList().SetLength(g_user_frequencies.size());
      UpdateButtons();
    });

    dialog.AddButton(_("Close"), mrCancel);
  } else {
    select_button = dialog.AddButton(_("Select"), [this, &dialog]() {
      const unsigned index = GetList().GetCursorIndex();
      if (index >= g_user_frequencies.size())
        return;
      const auto &entry = g_user_frequencies[index];
      if (mode == UserFrequencyListWidget::DialogMode::SELECT_ACTIVE)
        ActionInterface::SetActiveFrequency(entry.frequency,
                                            entry.name.c_str());
      else
        ActionInterface::SetStandbyFrequency(entry.frequency,
                                             entry.name.c_str());
      dialog.SetModalResult(mrOK);
    });

    dialog.AddButton(_("Cancel"), mrCancel);
  }
}

void
UserFrequencyListWidgetImpl::Prepare(ContainerWindow &parent,
                                     const PixelRect &rc) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  CreateList(parent, look, rc,
             row_renderer.CalculateLayout(*look.list.font_bold,
                                          look.small_font));

  GetList().SetLength(g_user_frequencies.size());
  UpdateButtons();
}

void
UserFrequencyListWidgetImpl::OnPaintItem(Canvas &canvas, const PixelRect rc,
                                         unsigned index) noexcept
{
  if (index >= g_user_frequencies.size())
    return;

  const auto &entry = g_user_frequencies[index];

  char freq_buf[16];
  if (entry.frequency.IsDefined())
    entry.frequency.Format(freq_buf, sizeof(freq_buf));
  else
    freq_buf[0] = '\0';

  row_renderer.DrawFirstRow(canvas, rc, entry.name.c_str());
  row_renderer.DrawSecondRow(canvas, rc, freq_buf);
}

void
UserFrequencyListWidgetImpl::OnActivateItem([[maybe_unused]] unsigned index) noexcept
{
  if (mode == UserFrequencyListWidget::DialogMode::BROWSE) {
    if (edit_button != nullptr)
      edit_button->Click();
  } else {
    if (select_button != nullptr)
      select_button->Click();
  }
}

void
UserFrequencyListWidgetImpl::UpdateButtons() noexcept
{
  const bool has_items = !g_user_frequencies.empty();
  const bool has_selection = has_items &&
    GetList().GetCursorIndex() < g_user_frequencies.size();

  if (select_button != nullptr)
    select_button->SetEnabled(has_selection);
  if (edit_button != nullptr)
    edit_button->SetEnabled(has_selection);
  if (delete_button != nullptr)
    delete_button->SetEnabled(has_selection);
}

void
dlgUserFrequencyListWidgetShowModal(UserFrequencyListWidget::DialogMode mode)
{
  LoadFromFile();

  const DialogLook &look = UIGlobals::GetDialogLook();

  auto widget = std::make_unique<UserFrequencyListWidgetImpl>(mode);
  auto *widget_ptr = widget.get();

  TWidgetDialog<UserFrequencyListWidgetImpl>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(), look,
           _("Frequencies"));

  widget_ptr->CreateButtons(dialog);
  dialog.FinishPreliminary(std::move(widget));
  dialog.ShowModal();
}
