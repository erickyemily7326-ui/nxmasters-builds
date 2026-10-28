#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace nxdk {
constexpr unsigned LooseUniformBinding = 15;
struct Uniform {
  std::string name;
  uint32_t offset = 0, size = 0, array_count = 1;
  int sampler = -1;
};
struct ShaderSource {
  std::string glsl;
  std::vector<Uniform> uniforms;
  uint32_t uniform_bytes = 0;
  bool reads_framebuffer = false;
};
int shader_define(std::string_view source, std::string_view name);
std::string lower_gs_shuffle(std::string source);
ShaderSource lower_shader(std::string_view source, bool vertex,
                         const std::map<std::string, unsigned> &attributes = {});
}
