#pragma once
#include "dk_gpu.h"
#include "dk_shader_source.h"

namespace nxdk {
struct ShaderBinary {
  DkShader shader{};
  MemoryPtr memory;
};
std::shared_ptr<ShaderBinary> compile_shader(const ShaderSource &source, DkStage stage,
                                          std::string &diagnostics);
}
