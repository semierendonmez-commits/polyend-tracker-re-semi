// OT fork: .ptfx Sample Editor plugins (ptfx.h). Our own reader of Polyend's
// published format (third_party/polyend-ptfx-sdk/tracker_plugin.h, MIT): the
// header is parsed field by field, little-endian, at the SDK's offsets.
#include "app/ptfx.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "app/sample_fx_ot.h"
#include "hal_api.h"

namespace ot {
namespace ptfx {

namespace {

constexpr uint32_t kMagic = 0x58465450u;   // "PTFX"
constexpr int kNameLen = 24, kFileLen = 64, kHeaderParams = 12;
constexpr const char* kDir = "Effects";   // card root (IStorage paths)

struct Param {
    char label[8];
    char unit[6];
    uint8_t type;    // 'f', 'd', 'l'
    uint8_t items;   // 'l': names in strings[strOff..]
    uint16_t strOff;
    float min, max, def, step;
};

struct Store {
    char names[kMaxPlugins][kNameLen];
    char files[kMaxPlugins][kFileLen];
    int n, sel;
    // The selected plugin.
    uint16_t flags;
    uint8_t np;
    uint32_t scratch, codeOff, codeSize, bss, off[3];
    Param p[kHeaderParams];
    char strings[512];
    sfx::EffectDesc desc;
    sfx::ParamDesc descP[sfx::kMaxParams];   // desc.p
};

IStorage* gFs = nullptr;
Store* gS = nullptr;
Prepare gPrepare = nullptr;

// The loaded run.
Entry gE;
uint8_t* gScratch = nullptr;
uint32_t gScratchBytes = 0;   // granted (CE also gives less than asked when it must)
uint32_t gPre = 0, gApp = 0, gRand = 1;

uint32_t rd32(const uint8_t* b) { return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24; }
float rdf(const uint8_t* b) {
    const uint32_t u = rd32(b);
    float f;
    std::memcpy(&f, &u, 4);
    return f == f ? f : 0.0f;
}
uint32_t al8(uint32_t v) { return (v + 7u) & ~7u; }
char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c + 32) : c; }
int cmpCi(const char* a, const char* b) {
    while (*a && lower(*a) == lower(*b)) ++a, ++b;
    return int(uint8_t(lower(*a))) - int(uint8_t(lower(*b)));
}

// Host API, the SDK's PtfxHostApi field order.
float hSin(float x) { return std::sin(x); }
float hCos(float x) { return std::cos(x); }
float hTan(float x) { return std::sin(x) / std::cos(x); }
float hTanh(float x) { return std::tanh(x); }
float hPow(float x, float y) { return std::pow(x, y); }
float hExp(float x) { return std::exp(x); }
float hSqrt(float x) { return std::sqrt(x); }
float hLog(float x) { return std::log(x); }
float hLog10(float x) { return std::log10(x); }
float hMin(float a, float b) { return a < b ? a : b; }
float hMax(float a, float b) { return a > b ? a : b; }
float hAbs(float x) { return std::fabs(x); }
void* hCpy(void* d, const void* s, size_t n) { return std::memcpy(d, s, n); }
void* hSet(void* d, int v, size_t n) { return std::memset(d, v, n); }
int hRand() {   // seeded per run: Preview and Apply hear the same
    gRand = gRand * 1103515245u + 12345u;
    return int((gRand >> 1) & 0x7FFFFFFFu);
}
void hExtra(uint32_t pre, uint32_t app) {
    gPre = pre;
    gApp = app;
}
struct HostApi {
    uint32_t sampleRate;
    float (*sinf)(float), (*cosf)(float), (*tanf)(float), (*tanhf)(float);
    float (*powf)(float, float);
    float (*expf)(float), (*sqrtf)(float), (*logf)(float), (*log10f)(float);
    float (*fminf)(float, float), (*fmaxf)(float, float);
    float (*fabsf)(float);
    void* (*memcpy)(void*, const void*, size_t);
    void* (*memset)(void*, int, size_t);
    int (*rand)();
    void (*setExtra)(uint32_t, uint32_t);
};
const HostApi kHost = {44100, hSin, hCos, hTan, hTanh, hPow, hExp, hSqrt, hLog, hLog10,
                       hMin, hMax, hAbs, hCpy, hSet, hRand, hExtra};

bool readAll(IStorage::Handle h, void* dst, uint32_t n) {
    auto* d = static_cast<uint8_t*>(dst);
    while (n) {
        const int32_t got = gFs->read(h, d, n > 4096u ? 4096u : n);
        if (got <= 0) return false;
        d += got;
        n -= uint32_t(got);
    }
    return true;
}

IStorage::Handle openEntry(int k) {
    char path[sizeof(Store::files[0]) + 12];
    std::snprintf(path, sizeof(path), "%s/%s", kDir, gS->files[k]);
    return gFs->open(path, IStorage::kRead);
}

// Header of entry k into h; false when it is not a plugin this firmware runs.
bool header(int k, uint8_t* h) {
    const IStorage::Handle f = openEntry(k);
    if (f < 0) return false;
    const bool ok = readAll(f, h, kHeaderBytes);
    gFs->close(f);
    if (!ok || rd32(h) != kMagic || (h[4] | h[5] << 8) != 1 || (h[6] & kFlagImxOnly)) return false;
    const uint32_t codeOff = rd32(h + 0x40), code = rd32(h + 0x44), bss = rd32(h + 0x48);
    if (codeOff < kHeaderBytes || code == 0 || code > kMaxCode || bss > kMaxCode - code || h[0x38] > kHeaderParams)
        return false;
    for (int i = 0; i < 3; ++i)
        if (rd32(h + 0x4C + 4 * i) >= code) return false;
    return true;
}

bool listFn(const IStorage::DirEntry& e, void* ctx) {
    (void)ctx;
    const size_t len = std::strlen(e.name);
    if (e.isDir || e.name[0] == '.' || len < 6 || len >= size_t(kFileLen) || cmpCi(e.name + len - 5, ".ptfx"))
        return true;
    std::memcpy(gS->files[gS->n], e.name, len + 1);
    return ++gS->n < kMaxPlugins;
}

}  // namespace

void setPrepare(Prepare fn) { gPrepare = fn; }
size_t storeBytes() { return sizeof(Store); }

void attach(IStorage* fs, void* mem, size_t bytes) {
    gFs = fs;
    gS = bytes >= sizeof(Store) ? static_cast<Store*>(mem) : nullptr;
    if (gS) gS->n = 0, gS->sel = -1;
}

int scan() {
    if (!gS || !gFs) return 0;
    gS->n = 0;
    gS->sel = -1;
    gFs->listDir(kDir, listFn, nullptr);
    uint8_t h[kHeaderBytes];
    int n = 0;
    for (int k = 0; k < gS->n; ++k) {
        if (!header(k, h)) continue;
        char nm[kNameLen];
        std::memcpy(nm, h + 8, kNameLen - 1);
        nm[kNameLen - 1] = 0;
        if (!nm[0]) {   // no display name: the file's
            std::snprintf(nm, sizeof(nm), "%s", gS->files[k]);
            char* dot = std::strrchr(nm, '.');
            if (dot) *dot = 0;
        }
        char file[kFileLen];
        std::memcpy(file, gS->files[k], kFileLen);
        int i = n++;   // sorted by name
        for (; i > 0 && cmpCi(gS->names[i - 1], nm) > 0; --i) {
            std::memcpy(gS->names[i], gS->names[i - 1], kNameLen);
            std::memcpy(gS->files[i], gS->files[i - 1], kFileLen);
        }
        std::memcpy(gS->names[i], nm, kNameLen);
        std::memcpy(gS->files[i], file, kFileLen);
    }
    gS->n = n;
    return n;
}

int count() { return gS ? gS->n : 0; }
const char* name(int k) { return gS && k >= 0 && k < gS->n ? gS->names[k] : ""; }
int selected() { return gS ? gS->sel : -1; }

bool select(int k) {
    if (!gS || k < 0 || k >= gS->n) return false;
    if (gS->sel == k) return true;
    uint8_t h[kHeaderBytes];
    if (!header(k, h)) return false;
    Store& s = *gS;
    s.flags = uint16_t(h[6] | h[7] << 8);
    s.np = h[0x38];
    s.scratch = rd32(h + 0x3C);
    s.codeOff = rd32(h + 0x40);
    s.codeSize = rd32(h + 0x44);
    s.bss = rd32(h + 0x48);
    for (int i = 0; i < 3; ++i) s.off[i] = rd32(h + 0x4C + 4 * i);
    s.sel = k;
    uint32_t used = 0;
    const IStorage::Handle f = openEntry(k);
    for (int i = 0; i < s.np; ++i) {
        const uint8_t* b = h + 0x58 + 32 * i;
        Param& p = s.p[i];
        std::memcpy(p.label, b, 7);
        p.label[7] = 0;
        p.min = rdf(b + 8);
        p.max = rdf(b + 12);
        p.def = rdf(b + 16);
        p.step = rdf(b + 20);
        p.type = b[30];
        p.items = 0;
        p.strOff = uint16_t(used);
        p.unit[0] = 0;
        if (p.type == 'l') {
            // The names: item_count C strings at list_offset in the code.
            const uint32_t at = uint32_t(b[24] | b[25] << 8);
            uint32_t want = uint32_t(b[26]) * 11u;
            if (want > sizeof(s.strings) - used) want = uint32_t(sizeof(s.strings)) - used;
            if (at + want > s.codeSize) want = at < s.codeSize ? s.codeSize - at : 0;
            if (f >= 0 && want && gFs->seek(f, s.codeOff + at) && readAll(f, s.strings + used, want)) {
                uint32_t q = used;
                while (p.items < b[26] && q < used + want) {   // whole names only
                    const void* z = std::memchr(s.strings + q, 0, used + want - q);
                    if (!z) break;
                    q = uint32_t(static_cast<const char*>(z) - s.strings) + 1;
                    ++p.items;
                }
                used = q;
            }
            p.min = 0;
            p.max = p.items ? float(p.items - 1) : 0.0f;
            p.step = 1;
        } else {
            std::memcpy(p.unit, b + 24, 5);
            p.unit[5] = 0;
            if (!(p.step > 0)) p.step = 1;
            if (p.max < p.min) p.max = p.min;
        }
        p.def = clamp(i, p.def);
    }
    if (f >= 0) gFs->close(f);
    sfx::EffectDesc& d = s.desc;
    d.name = s.names[k];
    d.p = s.descP;
    d.params = s.np < sfx::kMaxParams ? s.np : sfx::kMaxParams;
    for (int i = 0; i < d.params; ++i) {
        const Param& p = s.p[i];
        s.descP[i] = {p.label, p.min, p.max, p.step, p.def, sfx::kFmtFloat2, nullptr};
    }
    return true;
}

const sfx::EffectDesc& desc() { return gS && gS->sel >= 0 ? gS->desc : sfx::effect(0); }   // (none: never shown)

void defaults(float* p) {
    for (int i = 0; i < sfx::kMaxParams; ++i) p[i] = gS && gS->sel >= 0 && i < gS->np ? gS->p[i].def : 0.0f;
}

float clamp(int i, float v) {
    if (!gS || gS->sel < 0 || i < 0 || i >= gS->np) return v;
    const Param& p = gS->p[i];
    if (p.type != 'f') v = std::floor(v + 0.5f);
    return v < p.min ? p.min : (v > p.max ? p.max : v);
}

void format(int i, float v, char* out, int cap) {
    const size_t c = size_t(cap);
    if (!gS || gS->sel < 0 || i < 0 || i >= gS->np) {
        if (cap) out[0] = 0;
        return;
    }
    const Param& p = gS->p[i];
    if (p.type == 'l') {
        const char* s = gS->strings + p.strOff;
        for (int k = int(v + 0.5f); k > 0 && s[0]; --k) s += std::strlen(s) + 1;
        std::snprintf(out, c, "%s", p.items ? s : "-");
        return;
    }
    const char* sep = p.unit[0] && p.unit[0] != '%' && p.unit[0] != ' ' ? " " : "";
    if (p.type == 'd') {
        std::snprintf(out, c, "%d%s%s", int(std::floor(v + 0.5f)), sep, p.unit);
        return;
    }
    const int dec = p.step >= 1.0f ? 0 : (p.step >= 0.1f ? 1 : (p.step >= 0.01f ? 2 : 3));
    std::snprintf(out, c, "%.*f%s%s", dec, double(v), sep, p.unit);
}

bool generator() { return gS && gS->sel >= 0 && (gS->flags & kFlagGenerator); }

uint32_t memBytes() {
    if (!gS || gS->sel < 0) return 0;
    return al8(gS->codeSize + gS->bss) + al8(gS->scratch) + 8u;
}

uint32_t minBytes() {
    if (!gS || gS->sel < 0) return 0;
    const uint32_t s = gS->scratch < 4096u ? gS->scratch : 4096u;
    return al8(gS->codeSize + gS->bss) + al8(s) + 8u;
}

bool load(void* mem, uint32_t bytes) {
    gE = Entry();
    if (!gS || gS->sel < 0 || !gPrepare || !mem || bytes < minBytes()) return false;
    const Store& s = *gS;
    uint8_t* code = reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(mem) + 7u) & ~uintptr_t(7));
    const IStorage::Handle f = openEntry(s.sel);
    if (f < 0) return false;
    const bool ok = gFs->seek(f, s.codeOff) && readAll(f, code, s.codeSize);
    gFs->close(f);
    if (!ok) return false;
    std::memset(code + s.codeSize, 0, s.bss);
    gScratch = code + al8(s.codeSize + s.bss);
    const uint32_t room = (bytes - 8u - al8(s.codeSize + s.bss)) & ~7u;
    gScratchBytes = s.scratch < room ? s.scratch : room;
    Entry e;
    if (!gPrepare(code, s.codeSize + s.bss, s.off[0], s.off[1], s.off[2], &e) || !e.init || !e.setParam || !e.process)
        return false;
    gE = e;
    return true;
}

namespace {
// A run's start, as Polyend's host does it: zeroed scratch, init, then every
// parameter (those the editor does not show at their defaults).
void begin(const float* p) {
    const Store& s = *gS;
    std::memset(gScratch, 0, gScratchBytes);
    gPre = gApp = 0;
    gRand = 0x2545F491u;
    gE.init(gScratchBytes ? gScratch : nullptr, gScratchBytes, &kHost);
    for (int i = 0; i < kHeaderParams; ++i)
        gE.setParam(uint8_t(i), i < s.np ? (i < sfx::kMaxParams ? clamp(i, p[i]) : s.p[i].def) : 0.0f);
}
}  // namespace

uint32_t frames(const float* p, uint32_t n, uint32_t max) {
    if (!gE.process) return 0;
    begin(p);
    const uint64_t m = generator() ? kGenFrames : uint64_t(gPre) + n + gApp;
    return m < max ? uint32_t(m) : max;
}

uint32_t run(const float* p, const int16_t* in, uint32_t n, int16_t* out, uint32_t m) {
    if (!gE.process) return 0;
    begin(p);
    const bool gen = generator();
    const uint64_t total64 = gen ? kGenFrames : uint64_t(gPre) + n + gApp;
    const uint32_t total = total64 < m ? uint32_t(total64) : m;
    const uint32_t bodyEnd = gPre + n;
    uint32_t pos = 0;
    while (pos < total) {
        uint32_t blk = total - pos < kBlock ? total - pos : kBlock;
        const int16_t* src = nullptr;
        if (!gen && pos < gPre) {
            if (blk > gPre - pos) blk = gPre - pos;
        } else if (!gen && pos < bodyEnd) {
            if (blk > bodyEnd - pos) blk = bodyEnd - pos;
            src = in + (pos - gPre);
        }
        const int16_t* ins[1] = {src};
        int16_t* outs[1] = {out + pos};
        int32_t got = gE.process(ins, outs, blk, 1);
        if (got <= 0) break;
        pos += uint32_t(got) < blk ? uint32_t(got) : blk;
        sfx::progress(pos, total);
    }
    return pos;
}

}  // namespace ptfx
}  // namespace ot
