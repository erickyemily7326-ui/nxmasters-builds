#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stddef.h>
#include "../hooks.h"
#include "../error.h"

void egl_gl_ownership_park(void) {}
void egl_gl_ownership_release(void) {}
int egl_gl_thread_holds_context(void) { return 0; }
void egl_gl_shutdown(void) {}

EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint a, EGLint *v) {
  (void)d; (void)c; (void)a; if (v) *v = 0; return EGL_FALSE;
}
EGLContext eglGetCurrentContext(void) { return EGL_NO_CONTEXT; }
EGLSurface eglGetCurrentSurface(EGLint v) { (void)v; return EGL_NO_SURFACE; }
EGLint eglGetError(void) { return EGL_NOT_INITIALIZED; }
EGLDisplay eglGetDisplayHook(EGLNativeDisplayType d) { (void)d; return EGL_NO_DISPLAY; }
EGLBoolean eglInitializeHook(EGLDisplay d, EGLint *ma, EGLint *mi) {
  (void)d; (void)ma; (void)mi; return EGL_FALSE;
}
EGLBoolean eglBindAPIHook(EGLenum api) { (void)api; return EGL_FALSE; }
const char *eglQueryStringHook(EGLDisplay d, EGLint n) { (void)d; (void)n; return ""; }
EGLBoolean eglChooseConfigHook(EGLDisplay d, const EGLint *a, EGLConfig *c, EGLint s, EGLint *n) {
  (void)d; (void)a; (void)c; (void)s; if (n) *n = 0; return EGL_FALSE;
}
EGLContext eglCreateContextHook(EGLDisplay d, EGLConfig c, EGLContext shared, const EGLint *a) {
  (void)d; (void)c; (void)shared; (void)a;
  fatal_error("Deko3D: the core attempted to create an OpenGL context. Native hooks are incomplete.");
  return EGL_NO_CONTEXT;
}
EGLSurface eglCreateWindowSurfaceHook(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a) {
  (void)d; (void)c; (void)w; (void)a; return EGL_NO_SURFACE;
}
EGLSurface eglCreatePbufferSurfaceHook(EGLDisplay d, EGLConfig c, const EGLint *a) {
  (void)d; (void)c; (void)a; return EGL_NO_SURFACE;
}
EGLBoolean eglDestroySurfaceHook(EGLDisplay d, EGLSurface s) { (void)d; (void)s; return EGL_TRUE; }
EGLBoolean eglDestroyContextHook(EGLDisplay d, EGLContext c) { (void)d; (void)c; return EGL_TRUE; }
EGLBoolean eglMakeCurrentHook(EGLDisplay d, EGLSurface a, EGLSurface b, EGLContext c) {
  (void)d; (void)a; (void)b; (void)c; return EGL_FALSE;
}
EGLBoolean eglQuerySurfaceHook(EGLDisplay d, EGLSurface s, EGLint a, EGLint *v) {
  (void)d; (void)s; (void)a; if (v) *v = 0; return EGL_FALSE;
}
EGLBoolean eglSwapIntervalHook(EGLDisplay d, EGLint i) { (void)d; (void)i; return EGL_FALSE; }
EGLBoolean eglSwapBuffersHook(EGLDisplay d, EGLSurface s) { (void)d; (void)s; return EGL_FALSE; }
void *eglGetProcAddressHook(const char *name) { (void)name; return NULL; }
const GLubyte *glGetStringHook(GLenum name) { (void)name; return (const GLubyte *)"Deko3D"; }
