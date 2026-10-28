#pragma once
#include "dk_abi.h"
#include "dk_gpu.h"
#include "dk_shader.h"
#include <map>

namespace nxdk {
struct NativeTexture {
  TextureAbi abi;
  std::shared_ptr<Image> image;
  Upload mapping{};
  Rect mapped_rect{};
  unsigned mapped_level = 0;
};
static_assert(offsetof(NativeTexture, abi) == 0);
NativeTexture *texture(TextureAbi *value);
NativeTexture *create_texture(uint8_t type, uint32_t width, uint32_t height, unsigned levels,
                              uint8_t format);
void destroy_texture(NativeTexture *value);
void texture_shutdown();
bool update_texture(NativeTexture *value, Rect rect, const void *data, unsigned pitch,
                    unsigned level = 0);
bool download_texture(NativeTexture *value, Rect rect, MapAbi &map);
unsigned texture_bytes(uint8_t format);
bool compressed_texture(uint8_t format);

struct Kernel {
  ShaderSource source;
  std::shared_ptr<ShaderBinary> binary;
  std::vector<uint8_t> uniforms;
  uint64_t uniform_generation = 0;
  DkGpuAddr uniform_gpu = 0;
  bool hdr = false;
  int date = 0;
  void uniform(std::string_view name, const void *data, size_t size);
};
using KernelPtr = std::shared_ptr<Kernel>;
KernelPtr make_kernel(std::string source, DkStage stage);
KernelPtr utility_kernel(std::string_view file, std::string_view entry, DkStage stage,
                         std::string_view defines = {});
bool create_native_device(void *device);
void destroy_native_device(void *device);
void render_hardware(void *device, DrawConfig *config);
void bind_target(Image *color, Image *depth, const Rect *scissor = nullptr);
void clear_color(NativeTexture *target, const float *color);
void clear_depth(NativeTexture *target);
void clear_stencil(NativeTexture *target, uint8_t value);
void copy_texture(NativeTexture *source, NativeTexture *destination, const Rect &rect,
                  uint32_t x, uint32_t y);
void draw_rect(NativeTexture *source, const FloatRect &source_rect, Image *target,
               const FloatRect &target_rect, KernelPtr pixel, bool linear,
               uint8_t color_mask = 15, bool blend = false, Image *depth = nullptr,
               bool stencil = false);
void stretch_rect(void *, TextureAbi *, const FloatRect *, TextureAbi *, const FloatRect *,
                  unsigned shader, bool linear);
void present_rect(void *, TextureAbi *, const FloatRect *, TextureAbi *, const FloatRect *,
                  unsigned shader, float time, bool linear);
void update_clut(void *, TextureAbi *, unsigned x, unsigned y, TextureAbi *, unsigned offset,
                 unsigned count);
struct MultiRect {
  FloatRect source_rect, target_rect;
  TextureAbi *source;
  bool linear;
  uint8_t color_mask;
  uint8_t padding[6];
};
static_assert(sizeof(MultiRect) == 48);
static_assert(offsetof(MultiRect, target_rect) == 16 && offsetof(MultiRect, source) == 32);
void multi_stretch(void *, const MultiRect *, unsigned, TextureAbi *, unsigned shader);
void merge_outputs(void *, TextureAbi **, const FloatRect *, TextureAbi *, const FloatRect *,
                   const uint64_t *, const uint64_t *, const FloatRect *, bool linear);
void interlace(void *, TextureAbi *, TextureAbi *, int, bool, float, int);
void fxaa(void *, TextureAbi *, TextureAbi *);
void shadeboost(void *, TextureAbi *, TextureAbi *, const float *);
bool sharpen(void *, TextureAbi *, TextureAbi *, bool, const uint32_t *);
void bind_kernel_uniforms(Kernel &kernel, DkStage stage);
void bind_native_shaders(const KernelPtr &vertex, const KernelPtr &pixel,
                         const KernelPtr &geometry = {});
void bind_native_texture(const NativeTexture *texture, const DkSampler &sampler,
                        unsigned slot, DkStage stage = DkStage_Fragment);
void ui_shutdown();
bool ui_initialize();
bool ui_update_font();
void ui_render();
void native_device_shutdown();
}
