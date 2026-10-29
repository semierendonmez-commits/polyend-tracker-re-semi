#include "app/config.h"

#include "ui/theme.h"

#include <cstddef>
#include <cstring>

#include "ui/theme_file.h"

namespace ot {

namespace {
constexpr uint8_t kSignature[7] = {'E', 'E', 'P', 'R', 'O', 'M', 0};   // spec 01 §10
constexpr uint8_t kFileMagic[4] = {'O', 'T', 'C', 'F'};
constexpr uint8_t kFileV1 = 1;   // older OpenTracker: raw typed fields
constexpr uint8_t kFileV2 = 2;   // config image

// Typed field <-> image offset (spec 05 §5.4 table, spec 03 §4.1).
struct Map {
    uint16_t field;   // offsetof(Config, ...)
    uint8_t img;
};
#define F(m) uint16_t(offsetof(Config, m))
const Map kMap[] = {
    {F(masterVolume), 0x1B},
    {F(clockIn), 0x26},          {F(clockOut), 0x27},     {F(transportIn), 0x28},
    {F(transportOut), 0x29},     {F(notesIn), 0x2A},      {F(notesInChannel), 0x2B},
    {F(midiOut), 0x2C},          {F(ccIn), 0x2D},         {F(ccInChannel), 0x2E},
    {F(middleC), 0x2F},          {F(clockCorrection), 0x30},
    {F(radioRegion), 0xCA},      {F(brightness), 0xCB},   {F(padsBrightness), 0xCC},
    {F(recordingOptions), 0xCE}, {F(perfPresets), 0xCF},  {F(numbering), 0xD0},
    {F(padsPreview), 0xD1},      {F(arrangement), 0xD2},  {F(patternTopInfo), 0xD3},
    {F(lineInChannel), 0xD4},    {F(theme), 0xD5},        {F(font), 0xD6},
    {F(previewVolume), 0xD7},
    {F(metronome), 0xE6},        {F(preRoll), 0xE7},      {F(tsNum), 0xE8},
    {F(tsDen), 0xE9},            {F(metronomeVolume), 0xEA},
    {F(sliceCount), 0xF2},
};
#undef F

template <typename T>
void clampField(T& v, T max, T def) {
    if (v > max) v = def;
}

uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
void wr16(uint8_t* p, uint16_t v) {
    p[0] = uint8_t(v);
    p[1] = uint8_t(v >> 8);
}

// Defaults of the bytes that are not typed fields (spec 05 §5.4 "Other
// global config fields", spec 12 §A.2 preset defaults).
void defaultImage(uint8_t* img) {
    std::memset(img, 0, Config::kImageSize);
    std::memcpy(img, kSignature, sizeof(kSignature));
    wr16(img + Config::kOffVersion, 0x0A00);
    img[Config::kOffSize] = 0xF4;
    const float master = 81.0f;   // 0x10 f32
    std::memcpy(img + 0x10, &master, 4);
    const uint8_t levels[8] = {0x23, 0, 0, 0x0F, 0x0F, 0x1D, 0x1D, 0x51};   // 0x14..0x1B
    std::memcpy(img + 0x14, levels, 8);
    img[0x21] = 0xFF;                     // no "beta" suffix (spec 05 §1.5 state 1)
    for (int t = 0; t < 8; ++t) wr16(img + Config::kOffPerfPatterns + 2 * t, 1);
    for (int s = 0; s < 12; ++s) img[Config::kOffPerfTypes + s] = uint8_t(s + 1);
    img[0xD7] = 0x56;
    for (int i = 0; i < 7; ++i) img[0xDC + i] = uint8_t(i + 1);
    img[0xF2] = 8;
}

// configValidate (spec 05 §5.4) for the bytes that are not typed fields.
void validateImage(uint8_t* img) {
    clampField<uint8_t>(img[0x14], 0x3F, 0x23);
    clampField<uint8_t>(img[0x15], 1, 0);
    clampField<uint8_t>(img[0x16], 1, 0);
    for (int i = 0x19; i <= 0x1A; ++i)
        if (img[i] < 13 || img[i] > 31) img[i] = 0x1D;
    // Performance presets (spec 12 §A.2 "Validation on load").
    for (int t = 0; t < 8; ++t) {
        const uint16_t v = rd16(img + Config::kOffPerfPatterns + 2 * t);
        if (v < 1 || v > 255) wr16(img + Config::kOffPerfPatterns + 2 * t, 1);
    }
    for (int s = 0; s < 12; ++s) {
        uint8_t& ty = img[Config::kOffPerfTypes + s];
        if (ty >= 22) ty = uint8_t(s + 1 < 22 ? s + 1 : 0);
        uint8_t& r = img[Config::kOffPerfRows + s];
        if (r > 3) r = 0;
        uint8_t* v = img + Config::kOffPerfValues + 8 * s;
        wr16(v, 0);
        for (int k = 1; k < 4; ++k) {
            const int16_t x = int16_t(rd16(v + 2 * k));
            if (x < -255 || x > 255) wr16(v + 2 * k, 0);
        }
    }
}
}  // namespace

Config::Config() { defaultImage(image); }

void Config::setDefaults() {
    Config d;
    *this = d;
}

void Config::sanitize() {
    // Range validation in the spirit of spec 05 §5.4 configValidate.
    clampField<uint8_t>(radioRegion, 3, 0);
    clampField<uint8_t>(streamMode, 2, 0);
    clampField<uint8_t>(streamThreshold, 3, 1);
    clampField<uint8_t>(exportRate, 1, 1);
    clampField<uint8_t>(brightness, 2, 2);
    clampField<uint8_t>(theme, 2, 0);
    clampField<uint8_t>(font, 1, 0);
    clampField<uint8_t>(padsBrightness, 2, 2);
    clampField<uint8_t>(padsPreview, 2, 2);
    clampField<uint8_t>(recordingOptions, 3, 0);
    clampField<uint8_t>(numbering, 2, 1);
    clampField<uint8_t>(arrangement, 2, 0);
    clampField<uint8_t>(patternTopInfo, 2, 0);
    clampField<uint8_t>(lineInChannel, 2, 0);
    clampField<uint8_t>(perfPresets, 1, 0);   // spec 12 §A.3: > 1 -> 0
    clampField<uint8_t>(metronome, 1, 0);
    clampField<uint8_t>(preRoll, 1, 0);
    clampField<uint8_t>(tsNum, 11, 3);
    clampField<uint8_t>(tsDen, 11, 3);
    clampField<uint8_t>(metronomeVolume, 100, 50);
    clampField<uint8_t>(clockIn, 2, 0);
    clampField<uint8_t>(clockOut, 3, 0);
    clampField<uint8_t>(transportIn, 2, 0);
    clampField<uint8_t>(transportOut, 3, 0);
    clampField<uint8_t>(notesIn, 3, 0);
    clampField<uint8_t>(notesInChannel, 16, 0);
    clampField<uint8_t>(midiOut, 3, 0);
    clampField<uint8_t>(ccIn, 3, 0);
    clampField<uint8_t>(ccInChannel, 16, 0);
    clampField<uint8_t>(middleC, 3, 2);
    clampField<uint8_t>(clockCorrection, 12, 6);
    clampField<uint8_t>(masterVolume, 100, 81);
    clampField<uint8_t>(previewVolume, 100, 86);
    if (sliceCount < 1 || sliceCount > 48) sliceCount = 8;
    clampField<uint8_t>(recSource, 3, 0);
    for (auto& g : recGain) clampField<uint8_t>(g, 100, 50);
    clampField<uint8_t>(recMonitor, 1, 1);
    if (radioFreq10 < 760 || radioFreq10 > 1080) radioFreq10 = 875;
    clampField<uint8_t>(cpuMeter, 2, 0);
    clampField<uint8_t>(sliceDetector, 1, 0);
    if (uiTheme >= kThemeTotal) uiTheme = 0;
    clampField<uint8_t>(bbReseed, kBBLocked, 0);
    clampField<uint8_t>(sliceThru, 1, 0);
    clampField<uint8_t>(uiFont, 2, 0);
    clampField<uint8_t>(scaleLock, 1, 0);
    clampField<uint8_t>(screenSaver, kScreenSaverMax, 0);
    clampField<uint8_t>(levelMeter, 1, 0);
    clampField<uint8_t>(reverbModel, uint8_t(kReverbModels - 1), 0);
    clampField<uint8_t>(delayModel, uint8_t(kDelayModels - 1), 0);
    clampField<uint8_t>(satDrive, 24, 0);
    clampField<uint8_t>(satMixInv, 100, 0);
    clampField<uint8_t>(satToneInv, 100, 0);
    clampField<uint8_t>(limCeiling, 12, 0);
    clampField<uint8_t>(limLink, 1, 0);
    clampField<uint8_t>(satColor, uint8_t(kSatColors - 1), 0);
    if (synthVoices < 1 || synthVoices > kMaxSynthVoices) synthVoices = kDefaultSynthVoices;
    themeFile[sizeof(themeFile) - 1] = 0;
    if (themeFile[0] && !themeNameValid(themeFile)) std::memset(themeFile, 0, sizeof(themeFile));
    validateImage(image);
}

// Pads brightness -> LED levels (spec 05 appendix: k = choice + 1,
// DIM = floor(10k/3), FULL = floor(31k/3), LEVEL_3 = floor(5k/3)).
uint8_t Config::padDim() const { return uint8_t(10 * ((padsBrightness < 3 ? padsBrightness : 2) + 1) / 3); }
uint8_t Config::padFull() const { return uint8_t(31 * ((padsBrightness < 3 ? padsBrightness : 2) + 1) / 3); }

void Config::toImage() {
    std::memcpy(image, kSignature, sizeof(kSignature));
    const uint8_t* base = reinterpret_cast<const uint8_t*>(this);
    for (const Map& m : kMap) image[m.img] = base[m.field];
    const int k = (padsBrightness < 3 ? padsBrightness : 2) + 1;
    image[0x34] = uint8_t(10 * k / 3);
    image[0x35] = uint8_t(31 * k / 3);
    image[0x36] = uint8_t(5 * k / 3);
}

void Config::toExt(uint8_t* out) const {
    out[0] = 'O';
    out[1] = 'T';
    out[2] = 'X';
    out[3] = kExtVersion;
    out[4] = uint8_t(kExtPayload);
    out[5] = cpuMeter;
    out[6] = synthVoices;
    out[7] = streamMode;
    out[8] = streamThreshold;
    out[9] = extKeep[0];
    out[10] = exportRate;
    std::memcpy(out + 11, themeFile, sizeof(themeFile));
    // OT fork bytes after the upstream payload.
    uint8_t* f = out + 5 + kUpstreamExtPayload;
    f[0] = sliceDetector;
    f[1] = uiTheme;
    f[2] = bbReseed;
    f[3] = sliceThru;
    f[4] = uiFont;
    f[5] = scaleLock;
    f[6] = screenSaver;
    f[7] = levelMeter;
    f[8] = reverbModel;
    f[9] = delayModel;
    f[10] = satDrive;
    f[11] = satMixInv;
    f[12] = satToneInv;
    f[13] = limCeiling;
    f[14] = limLink;
    f[15] = satColor;
    for (int i = 0; i < 6; ++i) f[16 + i] = extReserved[i];
    uint8_t sum = 0;
    for (uint32_t i = 0; i < kExtPayload; ++i) sum = uint8_t(sum + out[5 + i]);
    out[5 + kExtPayload] = sum;
}

// OT fork fields (record v6 after the upstream payload, or the older fork
// record); n = the fork bytes present (missing ones keep their defaults).
void Config::forkFromExt(const uint8_t* f, uint32_t n) {
    sliceDetector = (n >= 1 && f[0] <= 1) ? f[0] : 0;
    // A fork palette (3+) replaces the stock theme byte; else the image decides.
    if (n >= 2 && f[1] >= kThemeCount && f[1] < kThemeTotal) uiTheme = f[1];
    bbReseed = (n >= 3 && f[2] <= kBBLocked) ? f[2] : 0;
    sliceThru = (n >= 4 && f[3] <= 1) ? f[3] : 0;
    if (n >= 5 && f[4] == 2) uiFont = 2;   // the Pixel set; else the image's font decides
    scaleLock = (n >= 6 && f[5] <= 1) ? f[5] : 0;
    screenSaver = (n >= 7 && f[6] <= kScreenSaverMax) ? f[6] : 0;
    levelMeter = (n >= 8 && f[7] <= 1) ? f[7] : 0;
    reverbModel = (n >= 9 && f[8] < kReverbModels) ? f[8] : 0;
    delayModel = (n >= 10 && f[9] < kDelayModels) ? f[9] : 0;
    satDrive = (n >= 11 && f[10] <= 24) ? f[10] : 0;
    satMixInv = (n >= 12 && f[11] <= 100) ? f[11] : 0;
    satToneInv = (n >= 13 && f[12] <= 100) ? f[12] : 0;
    limCeiling = (n >= 14 && f[13] <= 12) ? f[13] : 0;
    limLink = (n >= 15 && f[14] <= 1) ? f[14] : 0;
    satColor = (n >= 16 && f[15] < kSatColors) ? f[15] : 0;
    for (uint32_t i = 0; i < 6; ++i) extReserved[i] = n >= 17 + i ? f[16 + i] : 0;
}

bool Config::fromExt(const uint8_t* in, uint32_t len) {
    if (len < 6 || in[0] != 'O' || in[1] != 'T' || in[2] != 'X' || in[3] == 0) return false;
    const uint32_t n = in[4];
    if (n == 0 || len < 6 + n) return false;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < n; ++i) sum = uint8_t(sum + in[5 + i]);
    if (sum != in[5 + n]) return false;
    // Newer versions only append fields: read the ones this build knows.
    cpuMeter = in[5] <= 2 ? in[5] : 0;
    synthVoices = (n >= 2 && in[6] >= 1 && in[6] <= kMaxSynthVoices) ? in[6] : kDefaultSynthVoices;
    const Config d;
    if (in[3] == 1 && n >= 3) {
        // The semi fork's older record: the fork bytes right after synthVoices.
        forkFromExt(in + 7, n - 2);   // semi.1 .. semi.6 (payload 4, 8, 16, 24)
        return true;
    }
    streamMode = (n >= 3 && in[7] <= 2) ? in[7] : d.streamMode;
    streamThreshold = (n >= 4 && in[8] <= 3) ? in[8] : d.streamThreshold;
    extKeep[0] = n >= 5 ? in[9] : d.extKeep[0];
    exportRate = (n >= 6 && in[10] <= 1) ? in[10] : 1;
    std::memset(themeFile, 0, sizeof(themeFile));
    if (n >= 6 + sizeof(themeFile) && in[11 + sizeof(themeFile) - 1] == 0) {
        std::memcpy(themeFile, in + 11, sizeof(themeFile));
        if (!themeNameValid(themeFile)) std::memset(themeFile, 0, sizeof(themeFile));
    }
    forkFromExt(in + 5 + kUpstreamExtPayload, n > kUpstreamExtPayload ? n - kUpstreamExtPayload : 0);
    return true;
}

bool Config::anyScene() const {
    for (int i = 0; i < kScenes; ++i)
        if (scenes[i][0]) return true;
    return false;
}

void Config::toScenes(uint8_t* out) const {
    out[0] = 'O';
    out[1] = 'T';
    out[2] = 'S';
    out[3] = 1;
    out[4] = uint8_t(kScenes);
    std::memcpy(out + 5, scenes, sizeof(scenes));
    uint8_t sum = 0;
    for (uint32_t i = 0; i < sizeof(scenes); ++i) sum = uint8_t(sum + out[5 + i]);
    out[5 + sizeof(scenes)] = sum;
}

bool Config::fromScenes(const uint8_t* in, uint32_t len) {
    if (len < kScenesSize || in[0] != 'O' || in[1] != 'T' || in[2] != 'S' || in[3] != 1 || in[4] != kScenes)
        return false;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < sizeof(scenes); ++i) sum = uint8_t(sum + in[5 + i]);
    if (sum != in[5 + sizeof(scenes)]) return false;
    std::memcpy(scenes, in + 5, sizeof(scenes));
    for (auto& s : scenes)
        if (s[0] > 1) s[0] = 0;
    return true;
}

bool Config::fromImage(const uint8_t* img) {
    if (std::memcmp(img, kSignature, sizeof(kSignature)) != 0) return false;
    Config c;
    std::memcpy(c.image, img, kImageSize);
    uint8_t* base = reinterpret_cast<uint8_t*>(&c);
    for (const Map& m : kMap) base[m.field] = c.image[m.img];
    // Recorder working copies are per project, not in the image.
    std::memcpy(c.recGain, recGain, sizeof(recGain));
    c.recSource = recSource;
    c.recMonitor = recMonitor;
    c.radioFreq10 = radioFreq10;
    c.cpuMeter = cpuMeter;   // extension record, not in the image
    c.synthVoices = synthVoices;
    c.sliceDetector = sliceDetector;
    c.bbReseed = bbReseed;
    c.sliceThru = sliceThru;
    c.uiFont = c.font;   // the stock font; an extension record may name the Pixel set
    c.scaleLock = scaleLock;
    c.screenSaver = screenSaver;
    c.levelMeter = levelMeter;
    c.reverbModel = reverbModel;
    c.delayModel = delayModel;
    c.satDrive = satDrive;
    c.satMixInv = satMixInv;
    c.satToneInv = satToneInv;
    c.limCeiling = limCeiling;
    c.limLink = limLink;
    c.satColor = satColor;
    std::memcpy(c.extReserved, extReserved, sizeof(extReserved));
    std::memcpy(c.scenes, scenes, sizeof(scenes));
    c.uiTheme = c.theme;     // the stock theme; an extension record may name a fork palette
    c.streamMode = streamMode;
    c.streamThreshold = streamThreshold;
    c.exportRate = exportRate;
    std::memcpy(c.themeFile, themeFile, sizeof(themeFile));
    std::memcpy(c.extKeep, extKeep, sizeof(extKeep));
    c.sanitize();
    *this = c;
    toImage();
    return true;
}

bool Config::load(INvStore& nv) {
    uint8_t buf[kImageSize];
    if (nv.size() < kImageSize || !nv.read(0, buf, kImageSize)) return false;
    if (!fromImage(buf)) return false;
    uint8_t ext[kExtSize];
    const Config d;
    cpuMeter = d.cpuMeter;
    synthVoices = d.synthVoices;
    streamMode = d.streamMode;
    streamThreshold = d.streamThreshold;
    exportRate = d.exportRate;
    themeFile[0] = 0;
    std::memcpy(extKeep, d.extKeep, sizeof(extKeep));
    sliceDetector = 0;
    bbReseed = 0;
    sliceThru = 0;
    scaleLock = screenSaver = levelMeter = reverbModel = delayModel = 0;
    // The longest record that fits the store (older records are shorter;
    // fromExt reads the payload length from the record).
    uint32_t recLen = kExtSize;
    for (uint32_t len = kExtSize; len >= 7; --len)
        if (nv.size() >= kExtOffset + len && nv.read(kExtOffset, ext, len)) {
            if (fromExt(ext, len)) recLen = 5 + ext[4] + 1;
            break;
        }
    // OT fork scenes: after the record (an older fork record was shorter).
    std::memset(scenes, 0, sizeof(scenes));
    uint8_t sc[kScenesSize];
    const uint32_t scAt = recLen < kExtSize ? kOldScenesOffset : kScenesOffset;
    if (nv.size() >= scAt + kScenesSize && nv.read(scAt, sc, kScenesSize)) fromScenes(sc, kScenesSize);
    return true;
}

bool Config::save(INvStore& nv) {
    if (uiTheme >= kThemeCount && uiTheme < kThemeTotal) theme = themeBase(uiTheme);   // a stock firmware shows the base
    font = uiFont >= 2 ? 0 : uiFont;                                                     // Pixel -> Original for stock
    toImage();
    if (nv.size() < kImageSize || !nv.write(0, image, kImageSize)) return false;
    // Extension record (best effort): written once the option leaves its
    // default; a store that never had one keeps its erased bytes past 0xF4.
    if (nv.size() < kExtOffset + kExtSize) return true;
    uint8_t ext[kExtSize];
    bool writeExt = true;
    const Config d;
    if (cpuMeter == d.cpuMeter && synthVoices == d.synthVoices && streamMode == d.streamMode &&
        streamThreshold == d.streamThreshold && exportRate == d.exportRate && themeFile[0] == 0 &&
        std::memcmp(extKeep, d.extKeep, sizeof(extKeep)) == 0 && sliceDetector == 0 && uiTheme < kThemeCount &&
        bbReseed == 0 && sliceThru == 0 && uiFont < 2 && scaleLock == 0 && screenSaver == 0 && levelMeter == 0 &&
        reverbModel == 0 && delayModel == 0 && satDrive == 0 && satMixInv == 0 && satToneInv == 0 &&
        limCeiling == 0 && limLink == 0 && satColor == 0 && !anyScene()) {
        Config probe;
        writeExt = nv.read(kExtOffset, ext, kExtSize) && probe.fromExt(ext, kExtSize);
    }
    if (writeExt) {
        toExt(ext);
        nv.write(kExtOffset, ext, kExtSize);
    }
    if (nv.size() >= kScenesOffset + kScenesSize) {   // scenes: once there are any
        uint8_t sc[kScenesSize];
        Config probe;
        if (anyScene() || (nv.read(kScenesOffset, sc, kScenesSize) && probe.fromScenes(sc, kScenesSize))) {
            toScenes(sc);
            nv.write(kScenesOffset, sc, kScenesSize);
        }
    }
    return true;
}

bool Config::load(IStorage& fs) {
    IStorage::Handle h = fs.open(kConfigPath, IStorage::kRead);
    if (h < 0) return false;
    uint8_t buf[8 + kImageSize + kExtSize + kScenesSize + sizeof(Config)] = {};
    int32_t n = 0, r;
    while (n < int32_t(sizeof(buf)) && (r = fs.read(h, buf + n, uint32_t(sizeof(buf) - n))) > 0) n += r;
    fs.close(h);
    if (n < 8 || std::memcmp(buf, kFileMagic, 4) != 0) return false;
    const uint32_t len = uint32_t(buf[5]) | (uint32_t(buf[6]) << 8);
    const uint32_t avail = uint32_t(n) - 8;
    if (buf[4] == kFileV2) {
        if (len != kImageSize || avail < kImageSize) return false;
        if (!fromImage(buf + 8)) return false;
        const Config d;
        cpuMeter = d.cpuMeter;
        synthVoices = d.synthVoices;
        streamMode = d.streamMode;
        streamThreshold = d.streamThreshold;
        exportRate = d.exportRate;
        themeFile[0] = 0;
        std::memcpy(extKeep, d.extKeep, sizeof(extKeep));
        sliceDetector = 0;
        bbReseed = 0;
        sliceThru = 0;
        scaleLock = screenSaver = levelMeter = reverbModel = delayModel = 0;
        const uint8_t* ext = buf + 8 + kImageSize;
        uint32_t recLen = kExtSize;
        if (fromExt(ext, avail - kImageSize)) recLen = 5 + ext[4] + 1;
        std::memset(scenes, 0, sizeof(scenes));
        if (avail >= kImageSize + recLen + kScenesSize) fromScenes(ext + recLen, kScenesSize);
        return true;
    }
    if (buf[4] != kFileV1) return false;
    // v1: a copy of the typed fields (an older OpenTracker build).
    Config c;
    uint32_t take = len < avail ? len : avail;
    if (take > offsetof(Config, image)) take = offsetof(Config, image);
    std::memcpy(&c, buf + 8, take);
    c.sanitize();
    *this = c;
    toImage();
    return true;
}

bool Config::save(IStorage& fs) {
    if (uiTheme >= kThemeCount && uiTheme < kThemeTotal) theme = themeBase(uiTheme);
    font = uiFont >= 2 ? 0 : uiFont;
    toImage();
    fs.mkdir(kConfigDir);
    uint8_t buf[8 + kImageSize + kExtSize + kScenesSize] = {};
    std::memcpy(buf, kFileMagic, 4);
    buf[4] = kFileV2;
    buf[5] = uint8_t(kImageSize);
    buf[6] = uint8_t(kImageSize >> 8);
    std::memcpy(buf + 8, image, kImageSize);
    toExt(buf + 8 + kImageSize);
    toScenes(buf + 8 + kImageSize + kExtSize);
    if (fs.exists(kConfigPath)) fs.remove(kConfigPath);
    IStorage::Handle h = fs.open(kConfigPath, IStorage::kWrite);
    if (h < 0) return false;
    bool ok = fs.write(h, buf, sizeof(buf)) == int32_t(sizeof(buf));
    fs.close(h);
    return ok;
}

}  // namespace ot
