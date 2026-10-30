// Pattern screen FX entry with the pads (user report, 2.2.3-semi.2: "when
// placing an fx on a note the pads will not enter that fx; the top right
// pad should be R for roll"). Fx key + pad picks the FX by the menu order
// (top right pad = Roll), also from the held FX list; on an empty FX cell a
// pad alone picks the FX too (it used to write a value without an effect);
// on a cell with an FX the pad sets its value.
#include "test.h"
#include "ui_fakes.h"

#include "app/pattern_ops.h"
#include "model/fx.h"
#include "ui/screens.h"

using namespace ot;

TEST(fx_pad_entry_picks_the_effect) {
    Harness h;
    App& a = *h.app;
    h.tap(btn::Pattern);
    PatternScreen& p = *static_cast<PatternScreen*>(a.ui.screen(kModPattern));
    h.tap(btn::Rec);   // edit mode
    CHECK(a.editMode);
    for (int s = 0; s < 8; ++s) a.pattern().tracks[0].steps[s].note = 48;
    h.tap(btn::Fx1);
    CHECK_EQ(p.col, 2);
    const int slot = fxSlotForColumn(2);
    auto fx = [&](int s) { return a.pattern().tracks[0].steps[s].fx[slot]; };
    CHECK_EQ(int(kFxMenuOrder[11]), int(FxId::Roll));
    // Fx1 + the top right pad: Roll.
    h.down(btn::Fx1);
    h.pad(11);
    h.up(btn::Fx1);
    CHECK_EQ(int(fx(0).type), int(FxId::Roll));
    // Held long enough for the FX list, then the pad.
    p.step = 2;
    h.down(btn::Fx1);
    h.run(1200);
    h.pad(11);
    h.run(50);
    h.up(btn::Fx1);
    h.run(50);
    CHECK_EQ(int(fx(2).type), int(FxId::Roll));
    // A pad alone on an empty FX cell: the effect of that pad.
    p.step = 4;
    h.pad(11);
    CHECK_EQ(int(fx(4).type), int(FxId::Roll));
    // A pad on a cell that has an FX: its value (the type stays).
    p.step = 4;
    const uint8_t v0 = fx(4).value;
    h.pad(3);
    CHECK_EQ(int(fx(4).type), int(FxId::Roll));
    CHECK(fx(4).value != v0 || v0 == 3);
    // Not in edit mode: the pads only play.
    h.tap(btn::Rec);
    p.step = 6;
    h.down(btn::Fx1);
    h.pad(11);
    h.up(btn::Fx1);
    CHECK_EQ(int(fx(6).type), 0);
}
