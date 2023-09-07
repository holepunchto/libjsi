#include <assert.h>
#include <js.h>
#include <uv.h>

#include "../include/jsi.h"
#include "jsi/jsi.h"

namespace jsi = facebook::jsi;

static uv_loop_t *js_platform_loop = nullptr;

static js_platform_t *js_platform = nullptr;

static uv_once_t js_platform_guard = UV_ONCE_INIT;

static void
on_js_platform_init (void) {
  int err;

  js_platform_loop = new uv_loop_t();
  err = uv_loop_init(js_platform_loop);
  assert(err == 0);

  err = js_create_platform(js_platform_loop, nullptr, &js_platform);
  assert(err == 0);
}

struct JSIRuntime : jsi::Runtime {
  JSIRuntime() {
    uv_once(&js_platform_guard, on_js_platform_init);
  }
};

std::unique_ptr<jsi::Runtime>
makeJSIRuntime () {
  return std::make_unique<JSIRuntime>();
}
