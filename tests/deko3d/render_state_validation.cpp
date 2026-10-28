#include "dk_render_state.h"
#include "dk_image_sync.h"
#include "dk_presentation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checked = 0;
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
  ++checked;
}
float blend(DkBlendOp op, float source, float destination) {
  switch (op) {
    case DkBlendOp_Add: return source + destination;
    case DkBlendOp_Sub: return source - destination;
    case DkBlendOp_RevSub: return destination - source;
    case DkBlendOp_Min: return std::min(source, destination);
    default: throw std::runtime_error("unexpected blend operation");
  }
}
}

int main() {
  try {

    unsigned ended_frames = 0;
    auto end_frame = [&] { ++ended_frames; };
    require(nxdk::classic_present_result(0, end_frame),
            "Classic core skipped a ready frame instead of calling EndPresent");
    require(!ended_frames, "Classic core ended a ready ImGui frame before rendering");
    require(!nxdk::classic_present_result(1, end_frame),
            "Classic core presented a skipped frame");
    require(ended_frames == 1, "Classic core left a skipped ImGui frame open");
    require(!nxdk::classic_present_result(2, end_frame),
            "Classic core presented without a backbuffer");
    require(ended_frames == 2, "Classic core left a failed ImGui frame open");

    for (unsigned bytes : {2u, 3u}) {
      const auto features = nxdk::native_core_features(bytes);
      const bool last = features & (1u << 5);
      const unsigned cpu_indices[3] = {last ? 0u : 2u, last ? 1u : 0u, last ? 2u : 1u};
      const unsigned provoking = nxdk::GsProvokingVertex == DkProvokingVertex_Last ? 2 : 0;
      require(cpu_indices[provoking] == 2, "flat triangles use the wrong GS vertex color");
      const unsigned dual_source = bytes == 3 ? 13 : 12;
      require((features & (15u << dual_source)) == (15u << dual_source),
              "core-specific blend/depth/stencil/CAS feature positions changed");
      require(!(features & (1u << 2)), "unsupported VS expansion advertised");
      require(!(features & ((1u << 11) | (bytes == 3 ? 1u << 12 : 0u))),
              "unsupported framebuffer fetch advertised");
    }

    for (DkBlendOp op : {DkBlendOp_Add, DkBlendOp_Sub, DkBlendOp_RevSub, DkBlendOp_Min}) {
      const auto state = nxdk::gs_blend_state(DkBlendFactor_One, DkBlendFactor_One, op);
      require(state.colorBlendOp == op, "RGB blend operation changed");
      require(state.srcAlphaBlendFactor == DkBlendFactor_One &&
              state.dstAlphaBlendFactor == DkBlendFactor_Zero, "alpha is blended with destination");
      const float alpha = std::clamp(blend(state.alphaBlendOp, 128.0f / 255.0f, 0.0f), 0.0f, 1.0f);
      require(alpha >= 127.5f / 255.0f, "RGB equation corrupted the subsequent DATE input");
    }
    require((nxdk::NativeDeviceFlags & DkDeviceFlags_DepthMinusOneToOne) == 0,
            "GS floating-point depth is remapped through -1..1");
    const float first = std::ldexp(1.0f, -32), second = std::ldexp(2.0f, -32);
    require(first < second && 2.0f * first - 1.0f == 2.0f * second - 1.0f,
            "depth regression fixture must expose loss of low GS depth bits");
    require(nxdk::gs_line_width(2, 4.0f) == 4.0f && nxdk::gs_line_width(0, 4.0f) == 1.0f,
            "native line width ignores the core expansion flag");
    for (unsigned count : {16u, 256u}) {
      const auto rect = nxdk::gs_clut_rect(count);
      for (unsigned offset : {0u, 16u, 32u, 128u}) {
        require(rect.x == 0 && rect.z == count && rect.y == 0 && rect.w == 1,
                "palette bank offset moved/clipped the compact destination palette");
        const unsigned first_source = unsigned(rect.x + 0.5f) + offset;
        require(first_source == offset, "palette bank offset applied twice to the source index");
      }
    }
    nxdk::DrawConfig draw{};
    require(!nxdk::gs_draw_starts_with_barrier(draw),
            "two null texture/depth pointers were mistaken for a depth feedback loop");
    nxdk::TextureAbi depth_source{};
    draw.texture = draw.ds = &depth_source;
    require(nxdk::gs_draw_starts_with_barrier(draw), "depth feedback lost its required barrier");
    draw.texture = nullptr;
    draw.one_barrier = true;
    require(nxdk::gs_draw_starts_with_barrier(draw), "framebuffer feedback lost its initial barrier");

    nxdk::ImageVisibility visibility;
    uint64_t color = 0, depth = 0, palette = 0;
    require(!visibility.needs_barrier(color), "untouched image needs a barrier");

    visibility.written(color);
    visibility.written(depth);
    require(visibility.needs_barrier(color) && visibility.needs_barrier(depth),
            "rendered color/depth became readable without synchronization");
    visibility.barrier(DkBarrier_None, DkInvalidateFlags_Image);
    require(visibility.needs_barrier(color), "cache invalidation finished a pending draw");
    visibility.barrier(DkBarrier_Fragments, DkInvalidateFlags_Descriptors);
    require(visibility.needs_barrier(color), "descriptor invalidate made texture data visible");
    visibility.barrier(DkBarrier_Tiles, DkInvalidateFlags_Image);
    require(visibility.needs_barrier(color), "tile-local barrier exposed unrelated texture tiles");
    visibility.barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
    require(!visibility.needs_barrier(color) && !visibility.needs_barrier(depth),
            "one global image barrier failed to cover both pending render targets");
    require(!visibility.needs_barrier(palette), "immutable palette causes an extra barrier");

    visibility.written(color);
    require(visibility.needs_barrier(color), "reused render target kept a stale visibility stamp");
    visibility.barrier(DkBarrier_Primitives, DkInvalidateFlags_Image);
    require(!visibility.needs_barrier(color), "compute completion did not expose image writes");
    visibility.written(palette);
    require(visibility.needs_barrier(palette), "updated palette reused stale sampled data");
    visibility.barrier(DkBarrier_Full, DkInvalidateFlags_Image);
    require(!visibility.needs_barrier(palette), "DMA completion did not expose updated palette");
    std::cout << "Validated " << checked << " GS rendering/synchronization regressions\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
