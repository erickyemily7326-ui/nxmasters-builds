#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nxdk {
struct alignas(16) Rect { int32_t x, y, z, w; };
struct FloatRect { float x, y, z, w; };
struct Size { int32_t x, y; };
struct CoreString {
  alignas(8) uint8_t bytes[24]{};
  std::string_view view() const;
  void assign(std::string_view value);
  void release();
};
static_assert(sizeof(CoreString) == 24);
struct CoreVector { const size_t *begin, *end, *capacity; };
struct TextureAbi {
  uintptr_t *vtable;
  float scale[2];
  int32_t width, height, levels;
  uint8_t type, format, state, needs_mipmaps;
  uint32_t last_frame_used;
  float offset_hack;
};
static_assert(sizeof(TextureAbi) == 0x28);
static_assert(offsetof(TextureAbi, width) == 0x10);
static_assert(offsetof(TextureAbi, format) == 0x1d);
struct MapAbi { uint8_t *bits; int32_t pitch; };
struct DownloadAbi {
  uintptr_t *vtable;
  uint32_t width, height;
  uint8_t format;
  uint8_t padding[7]{};
  const uint8_t *map;
  uint32_t pitch;
  bool needs_flush;
};
static_assert(offsetof(DownloadAbi, map) == 0x18);
static_assert(offsetof(DownloadAbi, needs_flush) == 0x24);
struct PsSelector { uint32_t words[3]; };
struct AlphaPass {
  alignas(8) PsSelector ps;
  bool enable;
  uint8_t color_mask, depth;
  uint8_t pad;
  float aref;
};
static_assert(sizeof(AlphaPass) == 24);
struct alignas(16) DrawConfig {
  TextureAbi *rt, *ds, *texture, *palette;
  const void *vertices;
  const uint32_t *indices;
  uint32_t vertex_count, index_count, indices_per_primitive;
  uint32_t padding;
  const CoreVector *drawlist;
  Rect scissor, drawarea;
  uint8_t topology;
  uint8_t pad_topology[7];
  alignas(8) PsSelector ps;
  uint8_t gs, vs;
  uint8_t pad_selectors[2];
  uint32_t blend;
  uint8_t sampler, color_mask, depth;
  bool one_barrier, full_barrier;
  uint8_t destination_alpha, flags;
  uint8_t pad_alpha[5];
  AlphaPass second_pass;
  alignas(16) uint8_t cb_vs[48];
  alignas(16) uint8_t cb_ps[208];
};
static_assert(offsetof(DrawConfig, scissor) == 0x50);
static_assert(offsetof(DrawConfig, ps) == 0x78);
static_assert(offsetof(DrawConfig, destination_alpha) == 0x91);
static_assert(offsetof(DrawConfig, second_pass) == 0x98);
static_assert(offsetof(DrawConfig, cb_vs) == 0xb0);
static_assert(offsetof(DrawConfig, cb_ps) == 0xe0);
static_assert(sizeof(DrawConfig) == 0x1b0);
struct WindowAbi {
  uint32_t type, pad;
  void *display, *window, *surface;
  uint32_t width, height;
  float scale, refresh_rate;
};
static_assert(sizeof(WindowAbi) == 48);
struct CoreProfile {
  const char *version, *sha256;
  uint32_t device_table, host_table, base_destructor, ps_source, imgui_io, imgui_render, font_rgba;
  uint32_t common_header_offset, feature_offset;
  unsigned feature_bytes;
  const uint32_t *device_methods;
  unsigned device_method_count;
  const uint32_t *host_methods;
  uint32_t upscale_address;
  uint32_t imgui_end_frame;
};
extern const CoreProfile *profile;
uintptr_t core_address(uint32_t offset);
void *core_imgui_io();
std::string core_pixel_source(void *device, const PsSelector &selector);
}
