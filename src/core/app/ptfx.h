// OT fork: Sample Editor plugins in Polyend's .ptfx format (the MIT SDK in
// third_party/polyend-ptfx-sdk: a 472-byte header, then position-independent
// Thumb-2 code with plugin_init / plugin_set_param / plugin_process_block).
// The same files run on Polyend's Community Edition firmware.
//
// /Effects/*.ptfx are listed after the built-in effects. A run loads the
// code (with its .bss and the scratch memory it asks for) into memory the
// Sample Editor lends from the free sample memory, then feeds the selection
// in blocks of up to 8192 samples, one channel at a time:
//   [prepend: no input][the selection][append: no input]
// where prepend / append come from host->set_extra_samples() during init and
// set_param. A block may return fewer samples than given (the rest of the
// input comes again in the next block); 0 ends the run there.
// Generators (flag 1) make up to 2 s without input.
//
// The listing and the selected plugin's descriptions live in memory the
// caller lends (attach(): the browsers' shared list buffer).
#pragma once

#include <cstddef>
#include <cstdint>

#include "app/sample_fx.h"

namespace ot {

class IStorage;

namespace ptfx {

constexpr int kMaxPlugins = 32;
constexpr uint32_t kHeaderBytes = 472;
constexpr uint32_t kMaxCode = 32u * 1024u;   // code + .bss (the Original Tracker's limit in CE)
constexpr uint32_t kBlock = 8192;
constexpr uint32_t kGenFrames = 88200;       // a generator's longest result
constexpr uint16_t kFlagGenerator = 0x0001, kFlagImxOnly = 0x0010;

using InitFn = void (*)(void* scratch, uint32_t scratchBytes, const void* host);
using SetParamFn = void (*)(uint8_t index, float value);
using ProcessFn = int32_t (*)(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels);
struct Entry {
    InitFn init = nullptr;
    SetParamFn setParam = nullptr;
    ProcessFn process = nullptr;
};
// Platform hook: makes the code just read into `code` runnable (caches,
// barriers) and gives its entry points (offsets into the code). The device
// sets one at boot; without it (host builds) plugins list but do not run.
using Prepare = bool (*)(uint8_t* code, uint32_t bytes, uint32_t initOff, uint32_t setOff, uint32_t procOff, Entry* out);
void setPrepare(Prepare fn);

// The card and memory for the listing (at least storeBytes(); less: none).
size_t storeBytes();
void attach(IStorage* fs, void* mem, size_t bytes);
// Lists /Effects again (valid headers only, sorted by name); the selection is
// dropped. Returns the count.
int scan();
int count();
const char* name(int k);
// Reads entry k's header and list names: desc() / defaults() / format() then
// describe it (the editor shows the first sfx::kMaxParams parameters).
bool select(int k);
int selected();   // -1 none
const sfx::EffectDesc& desc();
void defaults(float* p);
float clamp(int i, float v);
void format(int i, float v, char* out, int cap);
bool generator();
// Bytes a run asks for (code + .bss + scratch, aligned), and the least it
// takes (the scratch memory may be granted in part, as CE does).
uint32_t memBytes();
uint32_t minBytes();
// Loads the selected plugin's code into mem (`bytes` long, at least
// minBytes()). false: no plugin, a read error or no platform hook.
bool load(void* mem, uint32_t bytes);
// After load(): frames out for n frames in (prepend + n + append, or a
// generator's 2 s), at most `max`.
uint32_t frames(const float* p, uint32_t n, uint32_t max);
// After load(): runs one channel; returns the frames written (<= m; fewer
// when the plugin ended early).
uint32_t run(const float* p, const int16_t* in, uint32_t n, int16_t* out, uint32_t m);

}  // namespace ptfx
}  // namespace ot
