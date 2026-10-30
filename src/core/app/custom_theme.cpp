// 2.x theme files from the card (ui/theme_file.h): the app side of the
// Display theme choice (see App::loadCustomTheme in app.h).
#include <cstdio>
#include <cstring>

#include "app/app.h"
#include "sdram_bss.h"
#include "ui/theme_file.h"

namespace ot {

namespace {
// The one loaded theme file (main loop only; Ui::th() reads it per draw call).
OT_SDRAM_BSS uint32_t g_customSlots[kThemeSlotCount];
}  // namespace

bool App::loadCustomTheme(const char* name) {
    if (usbActive_ || !hal_.storage || !themeNameValid(name)) return false;
    ThemeParse r;
    if (!loadThemeFile(fs(), name, g_customSlots, &r)) return false;
    std::snprintf(themeName_, sizeof(themeName_), "%s", name);
    themeView_ = themeView(themeName_, g_customSlots);
    themeActive_ = true;
    cfg.theme = r.base;
    cfg.uiTheme = r.base;   // OT fork: the Display theme row shows the file's base
    std::snprintf(cfg.themeFile, sizeof(cfg.themeFile), "%s", name);
    ui.dirty();
    return true;
}

void App::clearCustomTheme() {
    themeActive_ = false;
    std::memset(cfg.themeFile, 0, sizeof(cfg.themeFile));
    ui.dirty();
}

// boot(): the chosen file again (the card may have changed); without it the
// base theme shows and the name stays for the next card.
void App::restoreCustomTheme() {
    themeActive_ = false;
    if (!cfg.themeFile[0]) return;
    char name[sizeof(cfg.themeFile)];
    std::memcpy(name, cfg.themeFile, sizeof(name));
    loadCustomTheme(name);
}

bool App::beginThemeEdit() {
    if (usbActive_) return false;
    if (themeActive_) std::snprintf(themeEditPrev_, sizeof(themeEditPrev_), "%s", themeName_);
    else themeEditPrev_[0] = 0;
    const uint32_t* from = activeTheme().slots;
    if (from != g_customSlots) std::memcpy(g_customSlots, from, sizeof(g_customSlots));
    if (!themeActive_) {
        // A built-in theme: the file name to suggest ("Ember edit").
        std::snprintf(themeName_, sizeof(themeName_), "%.15s edit", activeTheme().name);
        cfg.theme = themeBase(cfg.uiTheme);
    }
    themeView_ = themeView(themeName_, g_customSlots);
    themeActive_ = true;
    themeEditing_ = true;
    ui.dirty();
    return true;
}

uint32_t App::themeEditGet(int role) const {
    if (role < 0 || role >= kThemeRoleCount) return 0;
    return g_customSlots[themeRole(role).slots[0]] & 0xFFFFFFu;
}

void App::themeEditSet(int role, uint32_t rgb) {
    if (!themeEditing_ || role < 0 || role >= kThemeRoleCount) return;
    const ThemeRole& r = themeRole(role);
    for (int i = 0; i < r.count; ++i) g_customSlots[r.slots[i]] = rgb & 0xFFFFFFu;
    themeView_ = themeView(themeName_, g_customSlots);
    ui.dirty();
}

App::ThemeExport App::saveThemeEdit(const char* name) {
    if (usbActive_ || !hal_.storage) return ThemeExport::kUnavailable;
    const uint8_t base = cfg.theme < kThemeCount ? cfg.theme : 0;
    if (writeThemeFile(fs(), name, g_customSlots, base, true) != ThemeWrite::kOk) return ThemeExport::kFailed;
    themeEditing_ = false;
    if (!loadCustomTheme(name)) return ThemeExport::kFailed;
    markConfigDirty();
    return ThemeExport::kOk;
}

void App::cancelThemeEdit() {
    if (!themeEditing_) return;
    themeEditing_ = false;
    if (themeEditPrev_[0] && loadCustomTheme(themeEditPrev_)) return;
    clearCustomTheme();
}

App::ThemeExport App::exportTheme() {
    if (usbActive_ || !hal_.storage) return ThemeExport::kUnavailable;
    const uint32_t* slots = activeTheme().slots;   // OT fork: a fork palette exports too
    switch (writeThemeFile(fs(), activeThemeName(), slots, cfg.theme)) {
        case ThemeWrite::kOk: return ThemeExport::kOk;
        case ThemeWrite::kExists: return ThemeExport::kExists;
        default: return ThemeExport::kFailed;
    }
}

}  // namespace ot
