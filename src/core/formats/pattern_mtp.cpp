// pattern_NN.mtp codec: stock 12-track layout (spec 02 §4) and Tracker Mini
// 16-track layout (spec 21 §4).
#include "formats/pattern_mtp.h"

#include <cstring>

#include "formats/bytes.h"
#include "formats/crc32.h"
#include "formats/legacy_upgrade.h"
#include "sdram_bss.h"

namespace ot {

namespace {
void writeHeader(uint8_t* out, uint32_t size) {
    std::memset(out, 0, size);
    FileHeader h{{'K', 'S'}, 2, {}, {}, uint16_t(size)};
    std::memcpy(h.fwVersion, mtp::kFwVersion, 4);
    std::memset(h.structVersion, mtp::kStructVersion, 4);
    writeFileHeader(out, h);
}

void encodeTrack(const Track& track, uint8_t* tr) {
    tr[0] = track.lastStep;
    for (int s = 0; s < kStepCount; ++s) {
        uint8_t* st = tr + 1 + s * mtp::kStepSize;
        const Step& step = track.steps[s];
        st[0] = uint8_t(step.note);
        st[1] = step.instrument;
        // On disk: FX slot 1 first, then FX slot 0 (spec 02 §4.2).
        st[2] = step.fx[1].type;
        st[3] = step.fx[1].value;
        st[4] = step.fx[0].type;
        st[5] = step.fx[0].value;
    }
}

// Byte reader over a file image of `len` bytes; bytes past the end read 0.
struct Reader {
    const uint8_t* file;
    size_t len;
    uint8_t at(uint32_t off) const { return off < len ? file[off] : 0; }
};

void decodeTrack(const Reader& r, int t, Track& track) {
    const uint32_t tr = mtp::kTracksOff + uint32_t(t) * mtp::kTrackSize;
    track.lastStep = r.at(tr);
    for (int s = 0; s < kStepCount; ++s) {
        const uint32_t st = tr + 1 + uint32_t(s) * mtp::kStepSize;
        Step& step = track.steps[s];
        step.note = int8_t(r.at(st));
        step.instrument = r.at(st + 1);
        step.fx[1].type = r.at(st + 2);
        step.fx[1].value = r.at(st + 3);
        step.fx[0].type = r.at(st + 4);
        step.fx[0].value = r.at(st + 5);
    }
}

// Header fields and the (informational) CRC of a zero-filled `crcOff`-byte
// prefix of the file.
PatternLoadInfo readInfo(const Reader& r, uint32_t crcOff) {
    PatternLoadInfo li;
    li.structVersion = r.at(8);
    li.sizeField = uint16_t(r.at(0x0C) | (r.at(0x0D) << 8));
    li.storedCrc = uint32_t(r.at(crcOff)) | (uint32_t(r.at(crcOff + 1)) << 8) |
                   (uint32_t(r.at(crcOff + 2)) << 16) | (uint32_t(r.at(crcOff + 3)) << 24);
    uint32_t covered = r.len < crcOff ? uint32_t(r.len) : crcOff;
    uint32_t st = crc32Update(crc32Init(), r.file, covered);
    static const uint8_t zeros[64] = {};
    for (uint32_t rem = crcOff - covered; rem;) {
        uint32_t c = rem < sizeof(zeros) ? rem : uint32_t(sizeof(zeros));
        st = crc32Update(st, zeros, c);
        rem -= c;
    }
    li.computedCrc = crc32Final(st);
    return li;
}

legacy::PatternUpgradeParams upgradeParams(const PatternUpgradeContext* ctx) {
    legacy::PatternUpgradeParams prm;
    if (ctx) {
        prm.instrumentPlayModes = ctx->instrumentPlayModes;
        prm.beatSliceReference = ctx->beatSliceReference;
        prm.curveV2 = ctx->volumeCurveV2;
        prm.curveV4 = ctx->volumeCurveV4;
    }
    return prm;
}

// --- stock 12-track codec (spec 02 §4) --------------------------------------
void encodeTracks(const Track* tracks, uint8_t out[mtp::kFileSize]) {
    writeHeader(out, mtp::kFileSize);
    for (int t = 0; t < kStockTrackCount; ++t) encodeTrack(tracks[t], out + mtp::kTracksOff + t * mtp::kTrackSize);
    wrU32(out + mtp::kCrcOff, crc32(out, mtp::kCrcOff));
}

void resetUpperTracks(Track* p) {
    for (int t = kLegacyTrackCount; t < kStockTrackCount; ++t) p[t].clear(p[0].lastStep);
}

// `p` = the 12 stock tracks.
void decodeTracks(const uint8_t* file, size_t len, Track* p, const PatternUpgradeContext* ctx,
                  PatternLoadInfo* info) {
    // Read up to 0x242C bytes into a zeroed buffer (spec 02 §0, §4.4 step 1).
    const Reader r{file, len < mtp::kFileSize ? len : mtp::kFileSize};
    PatternLoadInfo li = readInfo(r, mtp::kCrcOff);
    for (int t = 0; t < kStockTrackCount; ++t) decodeTrack(r, t, p[t]);

    // spec 10 §2.2 steps a-d for structure < 5 (tracks 0..7, scan rule).
    const uint8_t v = li.structVersion;
    if (v < mtp::kStructVersion) {
        li.upgraded = legacy::upgradePattern(p, v, upgradeParams(ctx));
        li.upperTracksReset = true;
    }

    // spec 02 §4.4 step 3: size field != 0x242C resets tracks 8..11.
    if (li.sizeField != mtp::kFileSize) {
        resetUpperTracks(p);
        li.upperTracksReset = true;
    }

    if (info) *info = li;
}

// --- 16-track codec (spec 21 §4) ---------------------------------------------
// TrackAt: const Track& (int t) for t in 0..15.
template <typename TrackAt>
size_t encode16(TrackAt trackAt, uint8_t* out, MtpLayout layout) {
    const int n = layout == MtpLayout::Mini16 ? kMiniTrackCount : kStockTrackCount;
    const uint32_t size = mtp::fileSizeFor(n), crcOff = mtp::crcOffFor(n);
    writeHeader(out, size);
    for (int t = 0; t < n; ++t) encodeTrack(trackAt(t), out + mtp::kTracksOff + uint32_t(t) * mtp::kTrackSize);
    wrU32(out + crcOff, crc32(out, crcOff));
    return size;
}

// `stock` = tracks 0..11 (contiguous); ext[e] = track 12 + e.
void decode16(const uint8_t* file, size_t len, Track* stock, Track* const ext[kExtTrackCount],
              const PatternUpgradeContext* ctx, PatternLoadInfo* info) {
    // spec 21 §4.2 step 1: zero a 0x3030 buffer and read up to 0x3030 bytes.
    const Reader r{file, len < mtp::kMiniFileSize ? len : mtp::kMiniFileSize};
    PatternLoadInfo li = readInfo(r, mtp::kMiniCrcOff);
    li.firstResetTrack = kTrackCount;
    for (int t = 0; t < kStockTrackCount; ++t) decodeTrack(r, t, stock[t]);
    for (int e = 0; e < kExtTrackCount; ++e) decodeTrack(r, kStockTrackCount + e, *ext[e]);

    auto resetFrom = [&](int first) {
        const uint8_t last = stock[0].lastStep;
        for (int t = first; t < kStockTrackCount; ++t) stock[t].clear(last);
        for (int e = 0; e < kExtTrackCount; ++e)
            if (kStockTrackCount + e >= first) ext[e]->clear(last);
        if (first < li.firstResetTrack) li.firstResetTrack = uint8_t(first);
        li.upperTracksReset = true;
    };

    // Step 2: structure < 5 -> the 1.9.2 conversions on tracks 0..7, then
    // tracks 8..15 cleared with track 0's length.
    const uint8_t v = li.structVersion;
    if (v < mtp::kStructVersion) {
        li.upgraded = legacy::upgradePattern(stock, v, upgradeParams(ctx), kStockTrackCount);
        resetFrom(kLegacyTrackCount);
    }

    // Step 3: size field != 0x3030 -> 0x242C resets 12..15, other sizes 8..15.
    if (li.sizeField != mtp::kMiniFileSize)
        resetFrom(li.sizeField == mtp::kStockFileSize ? kStockTrackCount : kLegacyTrackCount);

    if (info) *info = li;
}
}  // namespace

void encodePattern(const PatternStock& p, uint8_t out[mtp::kFileSize]) { encodeTracks(p.tracks, out); }
void encodePattern(const Pattern& p, uint8_t out[mtp::kFileSize]) { encodeTracks(p.tracks, out); }

void decodePattern(const uint8_t* file, size_t len, PatternStock& p, const PatternUpgradeContext* ctx,
                   PatternLoadInfo* info) {
    decodeTracks(file, len, p.tracks, ctx, info);
}

uint32_t normalizeV6Pattern(uint8_t* buf, uint32_t len, V6Info* info) {
    if (!buf || len < mtp::kTracksOff) return len;
    const uint8_t version = buf[8];
    const uint16_t size = uint16_t(buf[0x0C] | (buf[0x0D] << 8));
    const int n = size == mtp::kV6File16 ? 16 : (size == mtp::kV6File12 ? 12 : 0);
    if (version < 6 || !n || len < mtp::kTracksOff + mtp::kV6TrackSize * uint32_t(n)) return len;
    V6Info vi;
    vi.converted = true;
    constexpr uint8_t kVolume = 18, kMicro = 2, kFirstUnknown = 43;
    // Front to back: every 6-byte step lands at or before its 8-byte source.
    for (int t = 0; t < n; ++t) {
        const uint8_t* src = buf + mtp::kTracksOff + uint32_t(t) * mtp::kV6TrackSize;
        uint8_t* dst = buf + mtp::kTracksOff + uint32_t(t) * mtp::kTrackSize;
        dst[0] = src[0];
        for (int s = 0; s < kStepCount; ++s) {
            uint8_t b[8];
            std::memcpy(b, src + 1 + s * mtp::kV6StepSize, 8);
            for (int k = 2; k <= 4; k += 2)
                if (b[k] >= kFirstUnknown) {
                    b[k] = b[k + 1] = 0;
                    ++vi.droppedFx;
                }
            const int8_t vol = int8_t(b[6]), micro = int8_t(b[7]);
            auto put = [&](uint8_t type, int8_t v) {
                if (v < 0) return;
                for (int k = 2; k <= 4; k += 2)
                    if (b[k] == 0) {
                        b[k] = type;
                        b[k + 1] = uint8_t(v > 100 ? 100 : v);
                        ++vi.movedVm;
                        return;
                    }
                ++vi.droppedVm;
            };
            put(kVolume, vol);
            put(kMicro, micro);
            std::memcpy(dst + 1 + s * mtp::kStepSize, b, mtp::kStepSize);
        }
    }
    const uint32_t out = mtp::fileSizeFor(n);
    buf[8] = buf[9] = buf[10] = buf[11] = mtp::kStructVersion;
    buf[0x0C] = uint8_t(out);
    buf[0x0D] = uint8_t(out >> 8);
    wrU32(buf + mtp::crcOffFor(n), crc32(buf, mtp::crcOffFor(n)));
    if (info) *info = vi;
    return out;
}

void decodePattern(const uint8_t* file, size_t len, Pattern& p, const PatternUpgradeContext* ctx,
                   PatternLoadInfo* info) {
    decodeTracks(file, len, p.tracks, ctx, info);
    for (int t = kStockTrackCount; t < kTrackCount; ++t) defaultExtTrack(p.tracks[t], p);
}

size_t encodePattern16(const Track tracks[kTrackCount], uint8_t* out, MtpLayout layout) {
    return encode16([&](int t) -> const Track& { return tracks[t]; }, out, layout);
}

size_t encodePattern16(const Pattern& p, uint8_t* out, MtpLayout layout) {
    return encodePattern16(p.tracks, out, layout);
}

size_t encodePattern16(const PatternStock& s, const Track* const ext[kExtTrackCount], uint8_t* out,
                       MtpLayout layout) {
    // A null ext track encodes as the default one: last step = track 0's,
    // every step empty (note 0xFF, other bytes 0).
    return encode16(
        [&](int t) -> const Track& {
            if (t < kStockTrackCount) return s.tracks[t];
            const Track* e = ext ? ext[t - kStockTrackCount] : nullptr;
            if (e) return *e;
            // 769 B, kept off the stack (SDRAM on the device); main loop only.
            OT_SDRAM_BSS static SdramLazy<Track> defMem;
            Track& def = defMem.get();
            defaultExtTrack(def, s);
            for (Step& st : def.steps) st = Step{};
            return def;
        },
        out, layout);
}

void decodePattern16(const uint8_t* file, size_t len, Track tracks[kTrackCount], const PatternUpgradeContext* ctx,
                     PatternLoadInfo* info) {
    Track* const ext[kExtTrackCount] = {&tracks[12], &tracks[13], &tracks[14], &tracks[15]};
    decode16(file, len, tracks, ext, ctx, info);
}

void decodePattern16(const uint8_t* file, size_t len, Pattern& p, const PatternUpgradeContext* ctx,
                     PatternLoadInfo* info) {
    decodePattern16(file, len, p.tracks, ctx, info);
}

void decodePattern16(const uint8_t* file, size_t len, PatternStock& s, Track ext[kExtTrackCount],
                     const PatternUpgradeContext* ctx, PatternLoadInfo* info) {
    Track* const e[kExtTrackCount] = {&ext[0], &ext[1], &ext[2], &ext[3]};
    decode16(file, len, s.tracks, e, ctx, info);
}

}  // namespace ot
