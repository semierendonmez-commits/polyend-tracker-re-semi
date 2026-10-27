// OT fork per-project settings that have no place in the stock project.mt:
// "<project>/otfork.bin" next to it (and Workspace/otfork.bin). Missing file
// = defaults; a project without fork settings gets no file. Stock firmware
// ignores the file.
//
// File: "OTF1", u8 version, u8 record count, then records of
//   u8 id, u8 length, payload.
// Record 1 (sidechain): 2 x 10 bytes (source, mode, attack, hold, release,
//   depth, thresh, reserved, targets lo, targets hi).
// Record 2 (track links): 16 x 8 bytes (source, instrument, note, transpose,
//   level, chance, fx, reserved), written when a link is set.
// Record 3 (reverb page 2): low cut, width, then the two character values of
//   each of the 7 reverb models (A x 7, B x 7). Unknown records are
//   skipped; a short record keeps the defaults of the missing bytes.
#pragma once

#include <cstdint>

#include "audio/sidechain.h"
#include "sequencer/track_link.h"

namespace ot {

class IStorage;

// Master Reverb page 2 (Shift): low cut and width for every model, and two
// character values per model (defaults reproduce the models' own sound).
struct ReverbExtra {
    static constexpr int kModels = 7;
    uint8_t lowCut = 0;     // 0 off, 1..100: 20 Hz .. 1 kHz
    uint8_t width = 100;    // stereo width %, 0..150
    uint8_t a[kModels] = {50, 50, 50, 50, 50, 50, 50};
    uint8_t b[kModels] = {50, 50, 50, 50, 50, 50, 50};
    bool isDefault() const;
};

struct ForkProject {
    audio::SidechainSettings sc[2];
    ReverbExtra rev;
    TrackLink link[16];
    bool anyLink() const;

    void setDefault();
    bool isDefault() const;
    void sanitize();
    // `dir` = "Workspace" or a project folder.
    bool load(IStorage& fs, const char* dir);              // false: missing / bad (defaults set)
    bool save(IStorage& fs, const char* dir) const;        // removes the file when default
    static constexpr const char* kFileName = "otfork.bin";
};

}  // namespace ot
