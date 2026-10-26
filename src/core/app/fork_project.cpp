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

void ForkProject::setDefault() {
    for (auto& s : sc) s = audio::SidechainSettings{};
}

bool ForkProject::isDefault() const {
    ForkProject d;
    uint8_t a[kScBytes], b[kScBytes];
    for (int k = 0; k < 2; ++k) {
        putSc(a, sc[k]);
        putSc(b, d.sc[k]);
        if (std::memcmp(a, b, kScBytes) != 0) return false;
    }
    return true;
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
}

bool ForkProject::load(IStorage& fs, const char* dir) {
    setDefault();
    char p[240];
    path(p, sizeof(p), dir);
    if (!fs.exists(p)) return false;
    IStorage::Handle h = fs.open(p, IStorage::kRead);
    if (h < 0) return false;
    uint8_t buf[256];
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
    uint8_t buf[6 + 2 + 2 * kScBytes];
    std::memcpy(buf, "OTF1", 4);
    buf[4] = kVersion;
    buf[5] = 1;
    buf[6] = kRecSidechain;
    buf[7] = 2 * kScBytes;
    putSc(buf + 8, sc[0]);
    putSc(buf + 8 + kScBytes, sc[1]);
    IStorage::Handle h = fs.open(p, IStorage::kWrite);
    if (h < 0) return false;
    const bool ok = fs.write(h, buf, sizeof(buf)) == int32_t(sizeof(buf));
    fs.close(h);
    return ok;
}

}  // namespace ot
