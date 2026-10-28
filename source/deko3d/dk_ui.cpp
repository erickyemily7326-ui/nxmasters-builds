#include "dk_native.h"
#include <algorithm>
#include <cstring>

namespace nxdk {
namespace {
KernelPtr vertex_shader, pixel_shader;
NativeTexture *font_texture = nullptr;
struct VectorAbi { int size, capacity; void *data; };
struct DrawData {
  bool valid;
  uint8_t padding[3];
  int list_count, index_count, vertex_count;
  void **lists;
  float position[2], size[2], scale[2];
};
struct DrawList { VectorAbi commands, indices, vertices; };
struct DrawCommand {
  FloatRect clip;
  Image *texture;
  uint32_t vertex_offset, index_offset, element_count, padding;
  void (*callback)(const DrawList *, const DrawCommand *);
  void *callback_data;
};
static_assert(sizeof(DrawCommand) == 0x38);
static_assert(offsetof(DrawData, size) == 0x20);
static_assert(offsetof(DrawList, vertices) == 0x20);

void ui_state(Image *target, const Upload &vertices, const Upload &indices, const Upload &projection) {
  bind_target(target, nullptr);
  bind_native_shaders(vertex_shader, pixel_shader);
  DkColorState color{};
  dkColorStateDefaults(&color);
  color.blendEnableMask = 1;
  DkColorWriteState write{};
  dkColorWriteStateDefaults(&write);
  DkBlendState blend{};
  dkBlendStateDefaults(&blend);
  dkBlendStateSetFactors(&blend, DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha,
                        DkBlendFactor_One, DkBlendFactor_InvSrcAlpha);
  DkDepthStencilState depth{};
  dkDepthStencilStateDefaults(&depth);
  depth.depthTestEnable = depth.depthWriteEnable = depth.stencilTestEnable = false;
  std::array<DkVtxAttribState, 3> attributes{};
  attributes[0].size = attributes[1].size = DkVtxAttribSize_2x32;
  attributes[0].type = attributes[1].type = DkVtxAttribType_Float;
  attributes[1].offset = 8;
  attributes[2].size = DkVtxAttribSize_4x8;
  attributes[2].type = DkVtxAttribType_Unorm;
  attributes[2].offset = 16;
  const DkVtxBufferState buffer{20, 0};
  const DkCmdBuf commands = gpu().commands();
  dkCmdBufBindColorState(commands, &color);
  dkCmdBufBindColorWriteState(commands, &write);
  dkCmdBufBindBlendState(commands, 0, &blend);
  dkCmdBufBindDepthStencilState(commands, &depth);
  dkCmdBufBindVtxAttribState(commands, attributes.data(), attributes.size());
  dkCmdBufBindVtxBufferState(commands, &buffer, 1);
  dkCmdBufBindVtxBuffer(commands, 0, vertices.gpu, vertices.size);
  dkCmdBufBindIdxBuffer(commands, DkIdxFormat_Uint16, indices.gpu);
  dkCmdBufBindUniformBuffer(commands, DkStage_Vertex, 0, projection.gpu, 256);
  gpu().bind_descriptor_sets();
}
}

bool ui_update_font() {
  auto *context = static_cast<uint8_t *>(core_imgui_io());
  if (!context) return false;
  void *atlas = *reinterpret_cast<void **>(context + 0x50);
  if (!atlas) return false;
  uint8_t *pixels = nullptr;
  int width = 0, height = 0;
  reinterpret_cast<void (*)(void *, uint8_t **, int *, int *, int *)>(core_address(profile->font_rgba))(
      atlas, &pixels, &width, &height, nullptr);
  if (!pixels || width <= 0 || height <= 0) return false;

  auto *replacement = create_texture(3, width, height, 1, 1);
  if (!replacement) return false;
  if (!update_texture(replacement, {0,0,width,height}, pixels, width * 4)) {
    destroy_texture(replacement);
    return false;
  }
  destroy_texture(font_texture);
  font_texture = replacement;
  *reinterpret_cast<Image **>(static_cast<uint8_t *>(atlas) + 8) = replacement->image.get();

  return true;
}

bool ui_initialize() {
  if (!gpu().initialize()) return false;

  vertex_shader = make_kernel(R"(#version 450 core
layout(location=0) in vec2 Position;
layout(location=1) in vec2 UV;
layout(location=2) in vec4 Color;
layout(std140,binding=0) uniform Projection { mat4 matrix; };
layout(location=0) out vec2 TexCoord;
layout(location=1) out vec4 VertexColor;
void main() { TexCoord=UV; VertexColor=Color; gl_Position=matrix*vec4(Position,0,1); }
)", DkStage_Vertex);

  pixel_shader = make_kernel(R"(#version 450 core
layout(binding=0) uniform sampler2D Font;
layout(location=0) in vec2 TexCoord;
layout(location=1) in vec4 VertexColor;
layout(location=0) out vec4 Color;
void main() { Color=VertexColor*texture(Font,TexCoord); }
)", DkStage_Fragment);
  auto *context = static_cast<uint8_t *>(core_imgui_io());
  if (context) {
    *reinterpret_cast<const char **>(context + 0x88) = "NetherSX2_Deko3D";
    *reinterpret_cast<uint32_t *>(context + 0xc) |= 8;
  }
  return ui_update_font();
}

void ui_render() {
  if (!vertex_shader || !pixel_shader) return;
  auto *context = static_cast<uint8_t *>(core_imgui_io());
  if (!context) return;
  auto **viewports = *reinterpret_cast<uint8_t ***>(context + 0x40b0);
  if (!viewports || !viewports[0]) return;
  const auto *draw = reinterpret_cast<const DrawData *>(viewports[0] + 0x48);
  if (!draw->valid || draw->list_count <= 0 || draw->size[0] <= 0 || draw->size[1] <= 0) return;
  Image *target = gpu().backbuffer();
  if (!target) return;
  unsigned command_count = 0;
  for (int i = 0; i < draw->list_count; ++i) {
    const auto *list = static_cast<const DrawList *>(draw->lists[i]);
    if (list->commands.size < 0 || list->vertices.size < 0 || list->indices.size < 0) return;
    command_count += list->commands.size;
  }
  if (command_count >= SamplerDescriptorCount) return;
  gpu().reserve_descriptors(command_count + 1);
  const float left = draw->position[0], right = left + draw->size[0];
  const float top = draw->position[1], bottom = top + draw->size[1];
  const float matrix[16]{2/(right-left),0,0,0, 0,2/(top-bottom),0,0,
                          0,0,-1,0, (right+left)/(left-right),(top+bottom)/(bottom-top),0,1};
  Upload projection = gpu().upload(256);
  if (!projection.cpu) return;
  memset(projection.cpu, 0, 256);
  memcpy(projection.cpu, matrix, sizeof(matrix));
  DkSampler sampler;
  dkSamplerDefaults(&sampler);
  sampler.minFilter = sampler.magFilter = DkFilter_Linear;
  for (auto &wrap : sampler.wrapMode) wrap = DkWrapMode_ClampToEdge;
  for (int i = 0; i < draw->list_count; ++i) {
    const auto *list = static_cast<const DrawList *>(draw->lists[i]);
    if (!list->vertices.size || !list->indices.size) continue;
    const Upload vertices = gpu().upload(uint32_t(list->vertices.size) * 20, 16);
    const Upload indices = gpu().upload(uint32_t(list->indices.size) * 2, 4);
    if (!vertices.cpu || !indices.cpu) return;
    memcpy(vertices.cpu, list->vertices.data, vertices.size);
    memcpy(indices.cpu, list->indices.data, indices.size);
    ui_state(target, vertices, indices, projection);
    const auto *commands = static_cast<const DrawCommand *>(list->commands.data);
    for (int j = 0; j < list->commands.size; ++j) {
      const auto &command = commands[j];
      if (command.callback) {
        if (reinterpret_cast<uintptr_t>(command.callback) == UINTPTR_MAX)
          ui_state(target, vertices, indices, projection);
        else command.callback(list, &command);
        continue;
      }
      if (!command.texture || !command.element_count ||
          uint64_t(command.index_offset) + command.element_count > unsigned(list->indices.size)) continue;
      const int x = std::clamp(int((command.clip.x - left) * draw->scale[0]), 0, int(target->width));
      const int y = std::clamp(int((command.clip.y - top) * draw->scale[1]), 0, int(target->height));
      const int z = std::clamp(int((command.clip.z - left) * draw->scale[0]), x, int(target->width));
      const int w = std::clamp(int((command.clip.w - top) * draw->scale[1]), y, int(target->height));
      if (x == z || y == w) continue;
      const DkScissor scissor{uint32_t(x), target->height - uint32_t(w), uint32_t(z-x), uint32_t(w-y)};
      dkCmdBufSetScissors(gpu().commands(), 0, &scissor, 1);
      const DkImageView view = command.texture->view();
      const DkResHandle descriptor = gpu().descriptor(view, sampler);
      dkCmdBufBindTexture(gpu().commands(), DkStage_Fragment, 0, descriptor);
      gpu().prepare_sampled_image(command.texture);
      dkCmdBufDrawIndexed(gpu().commands(), DkPrimitive_Triangles, command.element_count, 1,
                           command.index_offset, command.vertex_offset, 0);
      gpu().image_written(target);
    }
  }
  invalidate_native_state();
}

void ui_shutdown() {
  if (profile) {
    auto *context = static_cast<uint8_t *>(core_imgui_io());
    if (context) {
      *reinterpret_cast<const char **>(context + 0x88) = nullptr;
      *reinterpret_cast<uint32_t *>(context + 0xc) &= ~8u;
      void *atlas = *reinterpret_cast<void **>(context + 0x50);
      if (atlas) *reinterpret_cast<void **>(static_cast<uint8_t *>(atlas) + 8) = nullptr;
    }
  }
  destroy_texture(font_texture);
  font_texture = nullptr;
  vertex_shader.reset();
  pixel_shader.reset();
}
}
