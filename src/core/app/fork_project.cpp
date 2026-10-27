// OT fork per-project settings file (fork_project.h).
#include "app/fork_project.h"

#include <cstdio>
#include <cstring>

#include "hal_api.h"

namespace ot {

namespace {
constexpr uint8_t kVersion = 1;
constexpr uint8_t kRecSidechain = 1;
constexpr int kScBytes = 10;
constexpr uint8_t kRecLinks = 2;
constexpr uint8_t kRecReverb = 3;
constexpr int kRevBytes = 2 + 2 * ReverbExtra::kModels;

void putRev(uint8_t* p, const ReverbExtra& r) {
    p[0] = r.lowCut;
    p[1] = r.width;
    for (int k = 0; k < ReverbExtra::kModels; ++k) {
        p[2 + k] = r.a[k];
        p[2 + ReverbExtra::kModels + k] = r.b[k];
    }
}
void getRev(const uint8_t* p, int len, ReverbExtra& r) {
    uint8_t b[kRevBytes];
    putRev(b, r);
    for (int i = 0; i < len && i < kRevBytes; ++i) b[i] = p[i];
    r.lowCut = b[0];
    r.width = b[1];
    for (int k = 0; k < ReverbExtra::kModels; ++k) {
        r.a[k] = b[2 + k];
        r.b[k] = b[2 + ReverbExtra::kModels + k];
    }
}
constexpr int kLinkBytes = 8;

void putLink(uint8_t* p, const TrackLink& l) {
    p[0] = l.source;
    p[1] = l.instr;
    p[2] = uint8_t(l.note);
    p[3] = uint8_t(l.transpose);
    p[4] = l.level;
    p[5] = l.chance;
    p[6] = l.fx;
    p[7] = l.reserved;
}
void getLink(const uint8_t* p, int len, TrackLink& l) {
    uint8_t b[kLinkBytes];
    putLink(b, l);
    for (int i = 0; i < len && i < kLinkBytes; ++i) b[i] = p[i];
    l.source = b[0];
    l.instr = b[1];
    l.note = int8_t(b[2]);
    l.transpose = int8_t(b[3]);
    l.level = b[4];
    l.chance = b[5];
    l.fx = b[6];
    l.reserved = b[7];
}

void path(char* out, size_t n, const char* dir) { std::snprintf(out, n, "%.200s/%s", dir, ForkProject::kFileName); }

void putSc(uint8_t* p, const audio::SidechainSettings& s) {
    p[0] = s.source;
    p[1] = s.mode;
    p[2] = s.attack;
    p[3] = s.hold;
    p[4] = s.release;
    p[5] = s.depth;
    p[6] = s.thresh;
    p[7] = s.reserved;
    p[8] = uint8_t(s.targets);
    p[9] = uint8_t(s.targets >> 8);
}
void getSc(const uint8_t* p, int len, audio::SidechainSettings& s) {
    uint8_t* f[8] = {&s.source, &s.mode, &s.attack, &s.hold, &s.release, &s.depth, &s.thresh, &s.reserved};
    for (int i = 0; i < 8 && i < len; ++i) *f[i] = p[i];
    if (len >= kScBytes) s.targets = uint16_t(p[8] | (p[9] << 8));
}
}  // namespace

bool ReverbExtra::isDefault() const {
    ReverbExtra d;
    uint8_t x[kRevBytes], y[kRevBytes];
    putRev(x, *this);
    putRev(y, d);
    return std::memcmp(x, y, kRevBytes) == 0;
}

void ForkProject::setDefault() {
    for (auto& s : sc) s = audio::SidechainSettings{};
    rev = ReverbExtra{};
    for (auto& l : link) l = TrackLink{};
}

bool ForkProject::anyLink() const {
    for (const auto& l : link)
        if (l.source) return true;
    return false;
}

bool ForkProject::isDefault() const {
    ForkProject d;
    uint8_t a[kScBytes], b[kScBytes];
    for (int k = 0; k < 2; ++k) {
        putSc(a, sc[k]);
        putSc(b, d.sc[k]);
        if (std::memcmp(a, b, kScBytes) != 0) return false;
    }
    return !anyLink() && rev.isDefault();
}

void ForkProject::sanitize() {
    for (auto& s : sc) {
        if (s.source >= audio::SidechainSettings::kSourceCount) s.source = 0;
        if (s.mode > 1) s.mode = 0;
        if (s.attack > 100) s.attack = 100;
        if (s.hold > 100) s.hold = 100;
        if (s.release < 1) s.release = 1;
        if (s.release > 100) s.release = 100;
        if (s.depth > 100) s.depth = 100;
        if (s.thresh > 100) s.thresh = 100;
        s.targets &= uint16_t((1u << audio::kScTargetCount) - 1);
    }
    for (int t = 0; t < 16; ++t) {
        TrackLink& l = link[t];
        if (l.source > 16 || l.source == t + 1) l.source = 0;
        if (l.instr > 66) l.instr = 0;
        if (t >= 8 && l.instr < 48) l.instr = 64;   // tracks 9..16: synths and MIDI only
        if (l.note < -1) l.note = -1;
        if (l.transpose < -24) l.transpose = -24;
        if (l.transpose > 24) l.transpose = 24;
        if (l.level > 100) l.level = 100;
        if (l.chance > 100) l.chance = 100;
        if (l.fx > 1) l.fx = 0;
    }
    if (rev.lowCut > 100) rev.lowCut = 100;
    if (rev.width > 150) rev.width = 150;
    for (int k = 0; k < ReverbExtra::kModels; ++k) {
        if (rev.a[k] > 100) rev.a[k] = 100;
        if (rev.b[k] > 100) rev.b[k] = 100;
    }
}

bool ForkProject::load(IStorage& fs, const char* dir) {
    setDefault();
    char p[240];
    path(p, sizeof(p), dir);
    if (!fs.exists(p)) return false;
    IStorage::Handle h = fs.open(p, IStorage::kRead);
    if (h < 0) return false;
    uint8_t buf[512];
    const int32_t n = fs.read(h, buf, sizeof(buf));
    fs.close(h);
    if (n < 6 || std::memcmp(buf, "OTF1", 4) != 0) return false;
    int at = 6;
    for (int r = 0; r < buf[5] && at + 2 <= n; ++r) {
        const uint8_t id = buf[at], len = buf[at + 1];
        at += 2;
        if (at + len > n) break;
        if (id == kRecSidechain)
            for (int k = 0; k < 2; ++k) {
                const int off = k * kScBytes, l = len - off;
                if (l > 0) getSc(buf + at + off, l > kScBytes ? kScBytes : l, sc[k]);
            }
        if (id == kRecReverb) getRev(buf + at, len, rev);
        if (id == kRecLinks)
            for (int k = 0; k < 16; ++k) {
                const int off = k * kLinkBytes, l = len - off;
                if (l > 0) getLink(buf + at + off, l > kLinkBytes ? kLinkBytes : l, link[k]);
            }
        at += len;
    }
    sanitize();
    return true;
}

bool ForkProject::save(IStorage& fs, const char* dir) const {
    char p[240];
    path(p, sizeof(p), dir);
    if (fs.exists(p)) fs.remove(p);
    if (isDefault()) return true;
    uint8_t buf[6 + 2 + 2 * kScBytes + 2 + 16 * kLinkBytes + 2 + kRevBytes];
    std::memcpy(buf, "OTF1", 4);
    buf[4] = kVersion;
    buf[5] = 1;
    buf[6] = kRecSidechain;
    buf[7] = 2 * kScBytes;
    putSc(buf + 8, sc[0]);
    putSc(buf + 8 + kScBytes, sc[1]);
    int n = 8 + 2 * kScBytes;
    if (anyLink()) {
        buf[5] = 2;
        buf[n] = kRecLinks;
        buf[n + 1] = 16 * kLinkBytes;
        for (int k = 0; k < 16; ++k) putLink(buf + n + 2 + k * kLinkBytes, link[k]);
        n += 2 + 16 * kLinkBytes;
    }
    if (!rev.isDefault()) {
        ++buf[5];
        buf[n] = kRecReverb;
        buf[n + 1] = kRevBytes;
        putRev(buf + n + 2, rev);
        n += 2 + kRevBytes;
    }
    IStorage::Handle h = fs.open(p, IStorage::kWrite);
    if (h < 0) return false;
    const bool ok = fs.write(h, buf, uint32_t(n)) == int32_t(n);
    fs.close(h);
    return ok;
}

}  // namespace ot
