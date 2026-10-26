// OT fork per-project settings that have no place in the stock project.mt:
// "<project>/otfork.bin" next to it (and Workspace/otfork.bin). Missing file
// = defaults; a project without fork settings gets no file. Stock firmware
// ignores the file.
//
// File: "OTF1", u8 version, u8 record count, then records of
//   u8 id, u8 length, payload.
// Record 1 (sidechain): 2 x 10 bytes (source, mode, attack, hold, release,
//   depth, thresh, reserved, targets lo, targets hi). Unknown records are
//   skipped; a short record keeps the defaults of the missing bytes.
#pragma once

#include <cstdint>

#include "audio/sidechain.h"

namespace ot {

class IStorage;

struct ForkProject {
    audio::SidechainSettings sc[2];

    void setDefault();
    bool isDefault() const;
    void sanitize();
    // `dir` = "Workspace" or a project folder.
    bool load(IStorage& fs, const char* dir);              // false: missing / bad (defaults set)
    bool save(IStorage& fs, const char* dir) const;        // removes the file when default
    static constexpr const char* kFileName = "otfork.bin";
};

}  // namespace ot
