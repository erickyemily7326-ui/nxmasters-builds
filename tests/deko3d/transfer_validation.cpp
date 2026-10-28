#include "dk_transfer.h"
#include <iostream>
#include <stdexcept>

namespace {
unsigned checked = 0;
void valid(uint32_t pitch, uint32_t row_bytes, uint32_t rows, uint64_t size) {
  const auto buffer = nxdk::copy_buffer(0x100000, pitch, row_bytes, rows, size);
  if (buffer.addr != 0x100000 || buffer.rowLength != pitch || buffer.imageHeight != 0)
    throw std::runtime_error("DMA descriptor did not preserve byte strides");
  ++checked;
}
void invalid(uint32_t pitch, uint32_t row_bytes, uint32_t rows, uint64_t size,
             DkGpuAddr address = 0x100000) {
  try { nxdk::copy_buffer(address, pitch, row_bytes, rows, size); }
  catch (const std::invalid_argument &) { ++checked; return; }
  throw std::runtime_error("unsafe DMA buffer was accepted");
}
}
int main() {
  try {
    valid(2048, 2048, 64, 131072);
    valid(2048, 2048, 1024, 2097152);
    invalid(512, 2048, 1024, 2097152);
    valid(16, 4, 1, 16);
    invalid(4, 8, 1, 16);
    valid(4096, 4096, 256, 1048576);
    valid(32, 24, 2, 64);
    valid(48, 48, 2, 96);
    valid(4096, 320, 8, 7 * 4096 + 320);
    invalid(2048, 2048, 1024, 2097151);
    invalid(4096, 320, 8, 7 * 4096 + 319);
    invalid(0, 4, 1, 16);
    invalid(16, 0, 1, 16);
    invalid(16, 4, 0, 16);
    invalid(UINT32_MAX, 4, UINT32_MAX, UINT32_MAX);
    invalid(16, 4, 1, 16, DK_GPU_ADDR_INVALID);
    invalid(16, 4, 2, 32, DK_GPU_ADDR_INVALID - 16);
    std::cout << "Validated " << checked << " DMA pitch/bounds cases\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
