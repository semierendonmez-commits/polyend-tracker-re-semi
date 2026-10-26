// OT fork track links (Pattern page 3, F4 "Link"; saved in otfork.bin).
#pragma once

#include <cstdint>

namespace ot {

// OT fork track link: the track also plays whenever its source track
// triggers a note (pattern steps, rolls, arps, play modes, live pads): a
// live layer of a mutating / generated track. Links do not chain.
struct TrackLink {
    uint8_t source = 0;      // 0 off, 1..16 the source track
    uint8_t instr = 0;       // the instrument the linked track plays (0..66)
    int8_t note = -1;        // -1 follow the source's note, 0..127 a fixed note
    int8_t transpose = 0;    // -24..+24 semitones
    uint8_t level = 100;     // % of the source note's volume
    uint8_t chance = 100;    // % of the source's notes it plays
    uint8_t fx = 0;          // 0 none, 1 copies the source step's FX
    uint8_t reserved = 0;
};

}  // namespace ot
