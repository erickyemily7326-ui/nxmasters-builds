#pragma once
#ifdef USE_VULKAN
#include "hooks/vk.h"
#define USE_FRAME_GENERATION
#elif defined(USE_DEKO3D)
#include "deko3d/dk_backend.h"
#define USE_FRAME_GENERATION
#define vk_lsfg_is_available dk_lsfg_is_available
#define vk_lsfg_is_enabled dk_lsfg_is_enabled
#define vk_lsfg_is_high_fps_passthrough dk_lsfg_is_high_fps_passthrough
#define vk_lsfg_request_enabled dk_lsfg_request_enabled
#endif
