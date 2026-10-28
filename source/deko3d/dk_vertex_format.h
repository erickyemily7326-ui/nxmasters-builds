#pragma once
#include <deko3d.h>
#include <array>

namespace nxdk {
constexpr std::array<DkVtxAttribState, 8> gs_vertex_attributes(bool hardware) {
  std::array<DkVtxAttribState, 8> attributes{};
  for (auto &attribute : attributes) {
    attribute.isFixed = true;
    attribute.size = DkVtxAttribSize_1x32;
    attribute.type = DkVtxAttribType_Float;
  }
  auto set = [&](unsigned index, unsigned offset, DkVtxAttribSize size, DkVtxAttribType type) {
    attributes[index] = {};
    attributes[index].offset = offset;
    attributes[index].size = size;
    attributes[index].type = type;
  };
  set(0, 0, DkVtxAttribSize_2x32, DkVtxAttribType_Float);
  set(1, 16, DkVtxAttribSize_2x32, DkVtxAttribType_Float);
  set(7, 28, DkVtxAttribSize_4x8, DkVtxAttribType_Unorm);
  if (hardware) {
    set(2, 8, DkVtxAttribSize_4x8, DkVtxAttribType_Uscaled);
    set(3, 12, DkVtxAttribSize_1x32, DkVtxAttribType_Float);
    set(4, 16, DkVtxAttribSize_2x16, DkVtxAttribType_Uint);
    set(5, 20, DkVtxAttribSize_1x32, DkVtxAttribType_Uint);
    set(6, 24, DkVtxAttribSize_2x16, DkVtxAttribType_Uint);
  }
  return attributes;
}
}
