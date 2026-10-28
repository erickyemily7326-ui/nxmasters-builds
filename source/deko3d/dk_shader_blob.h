#pragma once
#include <lsfg/common/dksh.hpp>
#include <array>
#include <cstring>
#include <span>

namespace nxdk {
inline bool validate_shader_blob(std::span<const uint8_t> blob, unsigned stage,
                                 uint32_t scratch_limit = 65536) {
  constexpr std::array<uint32_t, 6> program_types{0, 3, 4, 2, 1, 5};
  if (stage >= program_types.size() || blob.size() < sizeof(lsfg::dksh::FileHeader)) return false;
  lsfg::dksh::FileHeader header;
  memcpy(&header, blob.data(), sizeof(header));
  if (header.magic != lsfg::dksh::magic || header.header_size != sizeof(header) ||
      header.program_count != 1 || header.programs_offset < sizeof(header) ||
      uint64_t(header.programs_offset) + 64 > header.control_size ||
      uint64_t(header.control_size) + header.code_size != blob.size() ||
      (header.control_size & 255) || (header.code_size & 255)) return false;
  uint32_t program[16];
  memcpy(program, blob.data() + header.programs_offset, sizeof(program));
  return program[0] == program_types[stage] && program[1] < header.code_size && program[2] <= 255 &&
         uint64_t(program[3]) + program[4] <= header.code_size && program[5] <= scratch_limit;
}
}
