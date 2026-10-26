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
//   level, chance, fx, reserved), written when a link is set. Unknown records are
//   skipped; a short record keeps the defaults of the missing bytes.
#pragma once

#include <cstdint>

#include "audio/sidechain.h"
#include "sequencer/track_link.h"

namespace ot {

class IStorage;

struct ForkProject {
    audio::SidechainSettings sc[2];
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
