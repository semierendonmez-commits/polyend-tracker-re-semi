// Turing Machine (after the Music Thing Modular Turing Machine by Tom
// Whitwell; the behaviour, not its circuit): a 16-bit shift register clocked
// once per step. The bit at Length - 1 comes back into bit 0, flipped with a
// probability set by the Lock knob: +100 never (a locked loop of Length
// steps), 0 half the time (random), -100 always (a locked loop of twice the
// length, the second half inverted). The low byte is the pitch (quantized
// like Marbles); hashes of the whole register give the gates, the velocity
// and the slides, so a locked loop repeats all of them.
#include <cstdio>

#include "gen/gen_internal.h"

namespace ot {
namespace gen {

namespace {
// A byte from the whole register (so a locked loop repeats it too) that is
// not the pitch byte: the gates, the velocity and the slides.
uint32_t regByte(uint16_t reg, uint32_t salt) { return ((uint32_t(reg) ^ salt) * 0x9E3779B1u) >> 24; }
}  // namespace

float turingFlip(int lock) {
    lock = lock < -100 ? -100 : (lock > 100 ? 100 : lock);
    return float(100 - lock) / 200.0f;
}

uint16_t turingClock(uint16_t r, int len, float flip, Rng& rng) {
    len = len < 1 ? 1 : (len > 16 ? 16 : len);
    uint16_t bit = uint16_t((r >> (len - 1)) & 1u);
    if (flip >= 1.0f || (flip > 0.0f && rng.coin(flip))) bit ^= 1u;
    return uint16_t((r << 1) | bit);
}

uint16_t turingSeedRegister(const int16_t* v, Rng& rng) {
    uint16_t r = uint16_t(rng.next() >> 7);
    if (!r) r = 0xA5A5u;
    // As if the machine had been running: 16 clocks put every bit of the
    // register in the loop (a locked loop repeats from the first step).
    const float flip = turingFlip(v[1]);
    for (int i = 0; i < 16; ++i) r = turingClock(r, v[0], flip, rng);
    return r;
}

int turingRender(const int16_t* v, int lock, int len, uint16_t& reg, Rng& rng, const Target& t, int track, int s0,
                 int n) {
    const float flip = turingFlip(lock);
    const int density = v[2] < 0 ? 0 : (v[2] > 100 ? 100 : v[2]);
    const int range = (v[3] < 0 ? 0 : (v[3] > 2 ? 2 : v[3])) + 1;
    const int steps = v[4];
    const int gateMode = v[6];
    const int velAmt = v[7] < 0 ? 0 : (v[7] > 100 ? 100 : v[7]);
    const int every = v[8] == 1 ? 2 : 1;
    const int glide = v[9] < 0 ? 0 : (v[9] > 100 ? 100 : v[9]);
    Track& tr = t.pat->tracks[track];
    for (int i = 0; i < n && s0 + i < kStepCount; ++i) tr.steps[s0 + i].clear();
    int hits = 0;
    bool prevHit = false;
    for (int i = 0; i < n; i += every) {
        const int s = s0 + i;
        if (s >= kStepCount) break;
        reg = turingClock(reg, len, flip, rng);
        const uint32_t gateByte = regByte(reg, 0x5bd1u);
        const bool hit = gateByte * 100u < uint32_t(density) * 256u;
        if (!hit) {
            prevHit = false;
            continue;
        }
        Step& st = tr.steps[s];
        const int note = marblesNote(float(reg & 0xFFu) / 255.0f, steps, t, range);
        st.note = int8_t(note < 0 ? 0 : (note > 127 ? 127 : note));
        st.instrument = t.instrument;
        // Glide (the slide into this note), Volume, Gate: two FX at most.
        if (prevHit && glide > 0 && int(regByte(reg, 0x1d87u)) * 100 < glide * 256) putFx(st, kFxGlide, 30);
        if (velAmt > 0) {
            const int low = int(regByte(reg, 0x3a6bu) >> 3);   // 0..31: how far below full
            const int vol = 100 - velAmt * low * 60 / (100 * 31);
            if (vol < 100) putFx(st, kFxVolume, uint8_t(vol));
        }
        if (gateMode == 1) putFx(st, kFxGate, 50);
        else if (gateMode == 2) putFx(st, kFxGate, 20);
        prevHit = true;
        ++hits;
    }
    return hits;
}

void genTuring(const int16_t* v, Target& t, Rng& rng, Result& r) {
    const int n = t.s1 - t.s0 + 1;
    int notes = 0;
    for (int tr = t.t0; tr <= t.t1; ++tr) {
        uint16_t reg = turingSeedRegister(v, rng);
        notes += turingRender(v, v[1], v[0], reg, rng, t, tr, t.s0, n);
    }
    if (t.t1 > t.t0)
        std::snprintf(r.msg, sizeof(r.msg), "Turing: %d notes, %d tracks", notes, t.t1 - t.t0 + 1);
    else
        std::snprintf(r.msg, sizeof(r.msg), "Turing: %d notes", notes);
    r.ok = true;
}

}  // namespace gen
}  // namespace ot
