// OT fork theme editor (Config > General > Edit theme): roles, R / G / B with
// the encoder, a live preview, Save to Themes/<name>.txt, Cancel.
#include <cstring>
#include <string>

#include "test.h"
#include "ui_fakes.h"

#include "ui/screens.h"
#include "ui/theme.h"
#include "ui/theme_file.h"

using namespace ot;

namespace {
constexpr int kEditRow = 5;   // General: region, brightness, theme, font, Export, Edit theme
void openEditor(Harness& h) {
    h.tap(btn::Pattern);
    h.tap(btn::Config);
    ConfigScreen& c = *static_cast<ConfigScreen*>(h.app->ui.screen(kModConfig));
    for (int i = 0; i < 10 && c.category < 1; ++i) h.tap(btn::Down);
    for (int i = 0; i < 10 && c.category > 1; ++i) h.tap(btn::Up);
    h.tap(btn::Right);
    for (int i = 0; i < 30 && c.option < kEditRow; ++i) h.tap(btn::Down);
    for (int i = 0; i < 30 && c.option > kEditRow; ++i) h.tap(btn::Up);
    CHECK_EQ(c.option, kEditRow);
    h.tap(btn::F8);   // Enter
}
int roleIndex(const char* name) {
    for (int i = 0; i < kThemeRoleCount; ++i)
        if (std::string(themeRole(i).name) == name) return i;
    return -1;
}
}  // namespace

TEST(theme_editor_edits_previews_and_saves) {
    Harness h;
    App& a = *h.app;
    a.cfg.uiTheme = 3;   // Early Cyber (a fork palette)
    a.cfg.theme = themeBase(3);
    const uint32_t bg0 = theme(3).bg;
    openEditor(h);
    CHECK(a.themeEditing());
    CHECK(a.customThemeActive());
    CHECK_STR(a.customThemeName(), "Early Cyber edit");
    // Role 0 is the background; red up by 10 with the encoder: the screen
    // shows it at once.
    CHECK_EQ(roleIndex("background"), 0);
    CHECK_EQ(a.themeEditGet(0), bg0 & 0xFFFFFFu);
    h.enc(3);
    h.enc(3);
    h.enc(3);
    h.enc(1);
    const uint32_t bg1 = a.themeEditGet(0);
    CHECK_EQ(int((bg1 >> 16) & 255), int((bg0 >> 16) & 255) + 10);
    CHECK_EQ(a.ui.th().bg, bg1);
    // Next role (title_bg), F5 = blue, Shift + encoder = steps of 8.
    h.tap(btn::F2);
    h.tap(btn::F5);
    const uint32_t tb0 = a.themeEditGet(1);
    h.down(btn::Shift);
    h.enc(1);
    h.up(btn::Shift);
    CHECK_EQ(int(a.themeEditGet(1) & 255), int(tb0 & 255) + 8 > 255 ? 255 : int(tb0 & 255) + 8);
    // Save: the keyboard with the suggested name, F8 writes the file and
    // makes it the display theme.
    h.tap(btn::F8);
    CHECK(a.ui.kb.isOpen());
    h.tap(btn::F8);
    CHECK(!a.ui.kb.isOpen());
    CHECK(!a.themeEditing());
    CHECK(h.fs.exists("Themes/Early Cyber edit.txt"));
    CHECK(a.customThemeActive());
    CHECK_STR(a.cfg.themeFile, "Early Cyber edit");
    CHECK_EQ(a.ui.th().bg, bg1);
    // Saving again over the same name replaces the file (no "exists").
    openEditor(h);
    h.enc(-2);
    h.tap(btn::F8);
    h.tap(btn::F8);
    CHECK(!a.themeEditing());
    CHECK_EQ(a.ui.th().bg, (bg1 - 0x020000u));
    uint32_t slots[kThemeSlotCount];
    ThemeParse pr;
    CHECK(loadThemeFile(h.fs, "Early Cyber edit", slots, &pr));
    CHECK_EQ(slots[themeRole(0).slots[0]] & 0xFFFFFFu, bg1 - 0x020000u);
}

TEST(theme_editor_cancel_restores) {
    Harness h;
    App& a = *h.app;
    a.cfg.uiTheme = 5;   // Amber CRT
    a.cfg.theme = themeBase(5);
    openEditor(h);
    h.enc(3);
    CHECK(a.ui.th().bg != theme(5).bg);
    h.tap(btn::F7);   // Cancel
    CHECK(!a.themeEditing());
    CHECK(!a.customThemeActive());
    CHECK_EQ(a.ui.th().bg, theme(5).bg);
    CHECK(a.cfg.themeFile[0] == 0);
    // Leaving the screen while editing cancels too.
    openEditor(h);
    h.enc(3);
    h.tap(btn::Pattern);
    CHECK(!a.themeEditing());
    CHECK_EQ(a.ui.th().bg, theme(5).bg);
}

// User report (2.2.3-semi.2): every theme of the pack listed twice, once as
// "._<name>" (macOS AppleDouble files on the FAT card) that did nothing.
TEST(theme_list_skips_hidden_files) {
    Harness h;
    uint32_t slots[kThemeSlotCount];
    for (int i = 0; i < kThemeSlotCount; ++i) slots[i] = 0x202020u + uint32_t(i);
    CHECK(writeThemeFile(h.fs, "Iznik", slots, 0) == ThemeWrite::kOk);
    h.fs.files["Themes/._Iznik.txt"] = {0, 5, 22, 7, 0, 0};
    h.fs.files["Themes/.DS_Store"] = {0, 0, 0, 1};
    char names[8][kThemeNameMax + 1];
    const int n = listThemeFiles(h.fs, names, 8);
    CHECK_EQ(n, 1);
    CHECK_STR(names[0], "Iznik");
}
