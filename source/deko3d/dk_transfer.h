#pragma once
#include <deko3d.h>
#include <stdexcept>

namespace nxdk {
inline DkCopyBuf copy_buffer(DkGpuAddr address, uint32_t pitch_bytes, uint32_t row_bytes,
                             uint32_t rows, uint64_t available_bytes) {
  const uint64_t extent = rows ? uint64_t(rows - 1) * pitch_bytes + row_bytes : 0;
  if (!row_bytes || !rows || pitch_bytes < row_bytes || extent > available_bytes ||
      address == DK_GPU_ADDR_INVALID || extent > DK_GPU_ADDR_INVALID - address)
    throw std::invalid_argument("invalid Deko3D DMA byte pitch or buffer extent");
  return {address, pitch_bytes, 0};
}
}
