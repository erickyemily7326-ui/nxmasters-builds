#pragma once
#include "dk_gpu.h"
#include <stdexcept>

namespace nxdk {
struct UtilityTargets {
  Image *color, *depth;

  uint8_t color_mask(uint8_t requested) const { return color ? requested : 0; }

  DkDepthStencilState depth_stencil(bool stencil = false) const {
    DkDepthStencilState state{};
    dkDepthStencilStateDefaults(&state);

    state.depthTestEnable = state.depthWriteEnable = depth && !stencil &&
        depth->format != DkImageFormat_S8;
    state.depthCompareOp = DkCompareOp_Always;
    state.stencilTestEnable = depth && stencil;
    return state;
  }
};

inline UtilityTargets utility_targets(Image *destination, Image *depth = nullptr) {
  if (destination && destination->is_depth()) {
    if (depth && depth != destination)
      throw std::invalid_argument("utility pass has two different depth destinations");
    depth = destination;
    destination = nullptr;
  }
  if (depth && !depth->is_depth())
    throw std::invalid_argument("utility depth attachment has a color format");
  return {destination, depth};
}
}
