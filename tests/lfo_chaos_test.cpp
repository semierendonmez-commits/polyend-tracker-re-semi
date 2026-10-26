// OT fork chaos LFO shapes (Rungler, Gingerbread, Rossler, Drift, Bridge):
// the .pti keeps them in the LFO record's padding byte (stock sees Random),
// the instrument editor lists them after the stock five, and a voice with one
// on the filter renders finite, moving audio.
#include "test.h"

#include <cmath>
#include <memory>
#include <set>
#include <vector>

#include "app/instrument_edit.h"
#include "audio/engine.h"
#include "formats/instrument_pti.h"

using namespace ot;
using namespace ot::audio;

TEST(chaos_lfo_shapes_survive_the_pti) {
    Instrument in;
    in.env[kAutoCutoff].enabled = 1;
    in.env[kAutoCutoff].loop = 1;   // LFO
    for (int v = 0; v < kLfoShapeCount; ++v) {
        CHECK(iedit::set(in, InstrParam::AutoAttackShape, kAutoCutoff, v) || v == iedit::get(in, InstrParam::AutoAttackShape, kAutoCutoff));
        CHECK_EQ(iedit::get(in, InstrParam::AutoAttackShape, kAutoCutoff), v);
        uint8_t hdr[pti::kHeaderBlockSize];
        encodePtiHeader(in, hdr);
        // Stock reads the shape byte: Random for the fork shapes.
        CHECK_EQ(int(hdr[pti::kBodyOff + 0x0C4 + kAutoCutoff * 8]), (v > 4 ? 4 : v));
        Instrument back;
        std::vector<uint8_t> file(hdr, hdr + sizeof(hdr));
        file.resize(file.size() + 64, 0);
        PtiLoadInfo info;
        CHECK(decodePtiHeader(file.data(), file.size(), back, &info) == FmtStatus::Ok);
        CHECK_EQ(iedit::get(back, InstrParam::AutoAttackShape, kAutoCutoff), v);
    }
    CHECK_EQ(iedit::range(in, InstrParam::AutoAttackShape, kAutoCutoff).hi, kLfoShapeCount - 1);
}

TEST(chaos_lfo_moves_the_filter) {
    std::vector<int16_t> fx(AudioEngine::kFxMemorySamples), pool(4u << 20);
    std::unique_ptr<Instrument[]> ins(new Instrument[kSampleInstrumentCount]);
    std::unique_ptr<AudioEngine> eng(new AudioEngine());
    CHECK(eng->init(fx.data(), fx.size(), pool.data(), pool.size(), ins.get()));
    std::vector<int16_t> saw(44100);
    for (size_t i = 0; i < saw.size(); ++i) saw[i] = int16_t(((i * 220 * 65536 / 44100) & 0xFFFF) - 32768) / 2;
    ins[0].lengthFrames = uint32_t(saw.size());
    ins[0].playMode = uint8_t(PlayMode::ForwardLoop);
    CHECK(eng->bank().store(0, saw.data(), uint32_t(saw.size()), false));
    ins[0].filterEnabled = 1;
    ins[0].cutoff = 0.5f;
    ins[0].env[kAutoCutoff].enabled = 1;
    ins[0].env[kAutoCutoff].loop = 1;
    ins[0].lfo[kAutoCutoff].amount = 1.0f;
    ins[0].lfo[kAutoCutoff].speed = 20;
    for (int shape = 1; shape <= kForkLfoShapes; ++shape) {
        ins[0].lfo[kAutoCutoff].shape = 4;
        ins[0].lfo[kAutoCutoff].forkShape = uint8_t(shape);
        VoiceNoteOn n;
        n.instrument = 0;
        n.note = 60;
        eng->voiceNoteOn(0, n);
        int16_t out[kBlock * 2];
        std::set<int> levels;
        bool finite = true;
        for (int b = 0; b < 600; ++b) {
            eng->renderLive(out);
            double s = 0;
            for (uint32_t i = 0; i < kBlock; ++i) s += double(out[2 * i]) * out[2 * i];
            const double rms = std::sqrt(s / kBlock);
            finite = finite && std::isfinite(rms);
            if (b > 20) levels.insert(int(rms / 200.0));
        }
        std::printf("  shape %d: %d level buckets\n", shape, int(levels.size()));
        CHECK(finite);
        CHECK(levels.size() >= 3);   // the filter moves
    }
}
