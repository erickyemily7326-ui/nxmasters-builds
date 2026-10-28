#pragma once
#include <deko3d.h>
#include "dk_abi.h"
#include <cstdint>

namespace nxdk {
inline constexpr uint32_t NativeDeviceFlags = DkDeviceFlags_OriginLowerLeft |
                                               DkDeviceFlags_DepthZeroToOne;
inline constexpr DkProvokingVertex GsProvokingVertex = DkProvokingVertex_Last;

constexpr uint32_t native_core_features(unsigned feature_bytes) {
  constexpr uint32_t geometry = 1u << 1, primitive_id = 1u << 3, texture_barrier = 1u << 4;
  constexpr uint32_t provoking_last = 1u << 5, point_expand = 1u << 6;
  constexpr uint32_t dxt = 1u << 9, bptc = 1u << 10;

  const unsigned dual_source_bit = feature_bytes == 3 ? 13 : 12;
  return geometry | primitive_id | texture_barrier | provoking_last | point_expand | dxt | bptc |
      (15u << dual_source_bit);
}

inline DkBlendState gs_blend_state(DkBlendFactor source, DkBlendFactor destination,
                                    DkBlendOp operation) {
  DkBlendState result{};
  dkBlendStateDefaults(&result);

  dkBlendStateSetOps(&result, operation, DkBlendOp_Add);
  dkBlendStateSetFactors(&result, source, destination, DkBlendFactor_One, DkBlendFactor_Zero);
  return result;
}

constexpr float gs_line_width(uint8_t flags, float scale) {
  return flags & 2 ? scale : 1.0f;
}

constexpr FloatRect gs_clut_rect(unsigned count) {
  return {0, 0, float(count), 1};
}

inline bool gs_draw_starts_with_barrier(const DrawConfig &config) {
  return config.drawlist || config.full_barrier || config.one_barrier ||
      (config.texture && config.texture == config.ds);
}
}
