#include "dk_native.h"
#include "dk_boundary.h"
#include "dk_transfer.h"
#include <algorithm>
#include <cstring>
#include <png.h>

namespace nxdk {
namespace {
MemoryPtr readback;

DkImageFormat image_format(uint8_t format) {
  static constexpr DkImageFormat formats[] = {
      DkImageFormat_None, DkImageFormat_RGBA8_Unorm, DkImageFormat_RGBA16_Unorm,
      DkImageFormat_ZF32_X24S8, DkImageFormat_R8_Unorm, DkImageFormat_R16_Uint,
      DkImageFormat_R32_Uint, DkImageFormat_R32_Float, DkImageFormat_RGBA_BC1,
      DkImageFormat_RGBA_BC2, DkImageFormat_RGBA_BC3, DkImageFormat_RGBA_BC7_Unorm};
  return format < std::size(formats) ? formats[format] : DkImageFormat_None;
}

bool valid_rect(const NativeTexture *value, const Rect &rect, unsigned level) {
  if (!value || !value->image || level >= value->image->levels || rect.x < 0 || rect.y < 0 ||
      rect.z <= rect.x || rect.w <= rect.y) return false;
  return uint32_t(rect.z) <= std::max(1u, value->image->width >> level) &&
         uint32_t(rect.w) <= std::max(1u, value->image->height >> level);
}

void destruct(NativeTexture *value) {
  if (value->image) gpu().retain(value->image->memory);
  value->image.reset();
  invalidate_native_state();
}
void deleting_destruct(NativeTexture *value) { destruct(value); delete value; }
void *handle(NativeTexture *value) { return value->image.get(); }
bool update(NativeTexture *value, const Rect *rect, const void *data, int pitch, int level) {
  return rect && pitch > 0 && level >= 0 && update_texture(value, *rect, data, pitch, level);
}
bool map(NativeTexture *value, MapAbi *output, const Rect *requested, int level) {
  if (!value || !output || level < 0) return false;
  const Rect rect = requested ? *requested : Rect{0, 0, std::max(1, value->abi.width >> level),
                                                std::max(1, value->abi.height >> level)};
  if (value->abi.type == 4) return download_texture(value, rect, *output);
  if (!valid_rect(value, rect, level)) return false;
  const bool compressed = compressed_texture(value->abi.format);
  const unsigned width = compressed ? (rect.z - rect.x + 3) / 4 : rect.z - rect.x;
  const unsigned height = compressed ? (rect.w - rect.y + 3) / 4 : rect.w - rect.y;
  const uint32_t pitch = align_up(uint64_t(width) * texture_bytes(value->abi.format), 16);
  const uint32_t size = align_up(uint64_t(pitch) * height, 16);
  if (!size) return false;
  value->mapping = gpu().upload(size, 16);
  if (!value->mapping.cpu) return false;
  value->mapped_rect = rect;
  value->mapped_level = level;
  output->bits = static_cast<uint8_t *>(value->mapping.cpu);
  output->pitch = pitch;
  return true;
}
void unmap(NativeTexture *value) {
  if (!value || !value->mapping.cpu) return;
  const Rect &rect = value->mapped_rect;
  const bool compressed = compressed_texture(value->abi.format);
  const unsigned rows = compressed ? (rect.w - rect.y + 3) / 4 : rect.w - rect.y;
  const uint32_t pitch = value->mapping.size / rows;
  const unsigned blocks = compressed ? (rect.z - rect.x + 3) / 4 : rect.z - rect.x;
  const DkCopyBuf source = copy_buffer(value->mapping.gpu, pitch,
      blocks * texture_bytes(value->abi.format), rows, value->mapping.size);
  const DkImageView view = value->image->view(value->mapped_level);
  const DkImageRect destination{uint32_t(rect.x), uint32_t(rect.y), 0,
      uint32_t(rect.z - rect.x), uint32_t(rect.w - rect.y), 1};
  gpu().barrier(DkBarrier_Full, 0);
  dkCmdBufCopyBufferToImage(gpu().commands(), &source, &view, &destination, 0);
  gpu().barrier(DkBarrier_Full, DkInvalidateFlags_Image);
  value->mapping = {};
  value->abi.state = 0;
  value->abi.needs_mipmaps = value->abi.levels > 1;
}
void mipmap(NativeTexture *value) {
  if (!value || !value->image || value->abi.levels <= 1 || compressed_texture(value->abi.format)) return;
  for (unsigned level = 1; level < value->image->levels; ++level) {
    const auto source = value->image->view(level - 1), destination = value->image->view(level);
    const DkImageRect src{0, 0, 0, std::max(1u, value->image->width >> (level - 1)),
                         std::max(1u, value->image->height >> (level - 1)), 1};
    const DkImageRect dst{0, 0, 0, std::max(1u, value->image->width >> level),
                         std::max(1u, value->image->height >> level), 1};
    gpu().barrier(DkBarrier_Primitives, DkInvalidateFlags_Image);
    dkCmdBufBlitImage(gpu().commands(), &source, &src, &destination, &dst, DkBlitFlag_FilterLinear, 0);
  }
  gpu().barrier(DkBarrier_Primitives, DkInvalidateFlags_Image);
  value->abi.needs_mipmaps = false;
}
bool save(NativeTexture *value, const CoreString *filename) {
  if (!filename || !value || value->abi.format != 1) return false;
  MapAbi pixels;
  if (!download_texture(value, {0, 0, value->abi.width, value->abi.height}, pixels)) return false;
  const std::string path(filename->view());
  FILE *file = fopen(path.c_str(), "wb");
  if (!file) return false;
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png ? png_create_info_struct(png) : nullptr;
  if (!png || !info) {
    if (png) png_destroy_write_struct(&png, nullptr);
    fclose(file);
    return false;
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    fclose(file);
    return false;
  }
  png_init_io(png, file);
  png_set_IHDR(png, info, value->abi.width, value->abi.height, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  for (int y = 0; y < value->abi.height; ++y) png_write_row(png, pixels.bits + y * pixels.pitch);
  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  return fclose(file) == 0;
}
void swap(NativeTexture *left, NativeTexture *right) {
  if (!left || !right) return;

  TextureAbi original = left->abi;
  left->abi = right->abi;
  right->abi = original;
  std::swap(left->image, right->image);
  left->mapping = right->mapping = {};
  invalidate_native_state();
}
uint32_t id(NativeTexture *value) { return uint32_t(reinterpret_cast<uintptr_t>(value)); }
uintptr_t texture_methods[] = {
    abi_entry<destruct>(), abi_entry<deleting_destruct>(),
    abi_entry<handle>(), abi_entry<update>(),
    abi_entry<map>(), abi_entry<unmap>(),
    abi_entry<mipmap>(), abi_entry<save>(),
    abi_entry<swap>(), abi_entry<id>()};
}

unsigned texture_bytes(uint8_t format) {
  static constexpr unsigned bytes[] = {0, 4, 8, 8, 1, 2, 4, 4, 8, 16, 16, 16};
  return format < std::size(bytes) ? bytes[format] : 0;
}
bool compressed_texture(uint8_t format) { return format >= 8 && format <= 11; }
NativeTexture *texture(TextureAbi *value) { return reinterpret_cast<NativeTexture *>(value); }

NativeTexture *create_texture(uint8_t type, uint32_t width, uint32_t height, unsigned levels,
                              uint8_t format) {
  const bool compressed = compressed_texture(format), depth = format == 3;
  const uint32_t flags = depth ? DkImageFlags_UsageRender : compressed ? 0 :
      DkImageFlags_UsageRender | DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine;
  auto image = gpu().create_image(width, height, image_format(format), levels, flags);
  if (!image) return nullptr;
  auto result = new NativeTexture{};
  result->abi = {texture_methods, {1.0f, 1.0f}, int(width), int(height), int(levels), type, format,
                 0, 0, 0, 0.0f};
  result->image = std::move(image);
  return result;
}

void destroy_texture(NativeTexture *value) { if (value) deleting_destruct(value); }

bool update_texture(NativeTexture *value, Rect rect, const void *data, unsigned pitch,
                    unsigned level) {
  if (!data || !valid_rect(value, rect, level)) return false;
  const bool compressed = compressed_texture(value->abi.format);
  const unsigned blocks = compressed ? (rect.z - rect.x + 3) / 4 : rect.z - rect.x;
  const unsigned rows = compressed ? (rect.w - rect.y + 3) / 4 : rect.w - rect.y;
  const unsigned bytes = texture_bytes(value->abi.format);
  const unsigned row_bytes = blocks * bytes;
  const uint32_t storage_pitch = align_up(row_bytes, 16);
  const uint32_t size = align_up(uint64_t(storage_pitch) * rows, 16);
  if (!bytes || pitch < row_bytes || !size) return false;
  Upload staging = gpu().upload(size, 16);
  if (!staging.cpu) return false;
  for (unsigned row = 0; row < rows; ++row)
    memcpy(static_cast<uint8_t *>(staging.cpu) + row * storage_pitch,
           static_cast<const uint8_t *>(data) + size_t(row) * pitch, row_bytes);
  const DkCopyBuf source = copy_buffer(staging.gpu, storage_pitch, row_bytes, rows, staging.size);
  const DkImageView target = value->image->view(level);
  const DkImageRect destination{uint32_t(rect.x), uint32_t(rect.y), 0,
      uint32_t(rect.z - rect.x), uint32_t(rect.w - rect.y), 1};

  gpu().barrier(DkBarrier_Full, 0);
  dkCmdBufCopyBufferToImage(gpu().commands(), &source, &target, &destination, 0);
  gpu().barrier(DkBarrier_Full, DkInvalidateFlags_Image);
  value->abi.state = 0;
  value->abi.needs_mipmaps = value->abi.levels > 1;
  return true;
}

bool download_texture(NativeTexture *value, Rect rect, MapAbi &map) {
  if (!valid_rect(value, rect, 0) || compressed_texture(value->abi.format)) return false;
  const unsigned bytes = texture_bytes(value->abi.format);
  const uint32_t pitch = align_up(uint64_t(rect.z - rect.x) * bytes, 16);
  const uint32_t size = align_up(uint64_t(pitch) * (rect.w - rect.y), 16);
  if (!size) return false;
  if (!readback || readback->size < size) {
    gpu().finish();
    readback = gpu().allocate(size);
  }
  if (!readback) return false;
  const DkImageView source = value->image->view();
  const DkImageRect source_rect{uint32_t(rect.x), uint32_t(rect.y), 0,
      uint32_t(rect.z - rect.x), uint32_t(rect.w - rect.y), 1};
  const DkCopyBuf destination = copy_buffer(readback->gpu(), pitch,
      uint32_t(rect.z - rect.x) * bytes, uint32_t(rect.w - rect.y), readback->size);
  gpu().barrier(DkBarrier_Full, DkInvalidateFlags_Image);
  dkCmdBufCopyImageToBuffer(gpu().commands(), &source, &source_rect, &destination, 0);
  gpu().barrier(DkBarrier_Full, DkInvalidateFlags_L2Cache);
  gpu().finish();
  map.bits = static_cast<uint8_t *>(readback->cpu());
  map.pitch = pitch;
  return true;
}

void texture_shutdown() { readback.reset(); }
}
