#include "dk_vertex_format.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
uint32_t word(const DkVtxAttribState &attribute) {
  static_assert(sizeof(attribute) == sizeof(uint32_t));
  uint32_t result;
  std::memcpy(&result, &attribute, sizeof(result));
  return result;
}
bool valid(uint32_t encoded) {

  const unsigned size = (encoded >> 21) & 63, type = (encoded >> 27) & 7;
  return size && type;
}
}
int main() {
  try {
    constexpr auto hardware = nxdk::gs_vertex_attributes(true);
    constexpr auto utility = nxdk::gs_vertex_attributes(false);
    for (const auto *layout : {&hardware, &utility})
      for (const auto &attribute : *layout)
        if (!valid(word(attribute))) throw std::runtime_error("invalid GPU vertex format");

    if (valid(0x00000040) || word(utility[2]) != 0x3a400040)
      throw std::runtime_error("logged method 0x45a regression was not corrected");
    for (unsigned index = 2; index <= 6; ++index)
      if (word(utility[index]) != 0x3a400040)
        throw std::runtime_error("unused utility attribute lacks a fixed float format");

    constexpr uint32_t expected_hardware[]{0x38800000, 0x38800800, 0x29400400, 0x3a400600,
                                           0x21e00800, 0x22400a00, 0x21e00c00, 0x11400e00};
    for (unsigned i = 0; i < hardware.size(); ++i)
      if (word(hardware[i]) != expected_hardware[i])
        throw std::runtime_error("GS vertex byte layout changed");
    for (unsigned index : {0u, 1u, 7u})
      if (word(utility[index]) != word(hardware[index]))
        throw std::runtime_error("active utility attribute changed");
    std::cout << "Validated 16 vertex attribute words; logged 0x45a/0x40 regression rejected\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
