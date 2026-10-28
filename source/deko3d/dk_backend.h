#pragma once

#ifdef __cplusplus
extern "C" {
#endif
int dk_install_native_backend(const char *core_path);
void dk_native_shutdown(void);
extern volatile int dk_present_count;
int dk_lsfg_is_available(void);
int dk_lsfg_is_enabled(void);
int dk_lsfg_is_high_fps_passthrough(void);
void dk_lsfg_request_enabled(int enabled);
#ifdef __cplusplus
}
#endif
