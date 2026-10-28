#pragma once
#include <cstdint>
#include <exception>
#include <utility>

extern "C" {
#include "../error.h"
}

namespace nxdk {
template<auto Function> struct AbiGuard;
template<class Result, class... Arguments, Result (*Function)(Arguments...)>
struct AbiGuard<Function> {
  static Result invoke(Arguments... arguments) noexcept {
    try {
      return Function(std::forward<Arguments>(arguments)...);
    } catch (const std::exception &error) {
      fatal_error("Deko3D: %s", error.what());
    } catch (...) {
      fatal_error("Deko3D: unexpected native renderer exception");
    }
  }
};
template<auto Function> uintptr_t abi_entry() {
  return reinterpret_cast<uintptr_t>(&AbiGuard<Function>::invoke);
}
}
