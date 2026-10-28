#pragma once

namespace nxdk {
template<class EndFrame>
bool classic_present_result(unsigned result, const EndFrame &end_frame) {
  if (!result) return true;
  end_frame();
  return false;
}
}
