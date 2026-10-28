#pragma once
#include <deko3d.h>
#include <cstdint>

namespace nxdk {
class ImageVisibility {
public:
  void written(uint64_t &stamp) const { stamp = m_epoch; }
  bool needs_barrier(uint64_t stamp) const { return stamp == m_epoch; }
  void barrier(DkBarrier mode, uint32_t invalidate) {
    if (mode >= DkBarrier_Fragments && (invalidate & DkInvalidateFlags_Image)) ++m_epoch;
  }
private:
  uint64_t m_epoch = 1;
};
}
