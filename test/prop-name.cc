#include <assert.h>

#include "../include/jsi.h"

int
main () {
  JSIPlatform platform;

  JSIRuntime runtime(platform);

  jsi::Scope scope(runtime);

  auto a = jsi::PropNameID::forAscii(runtime, "hello", 5);

  auto b = std::move(a);
  assert(b.utf8(runtime) == "hello");

  auto c = jsi::PropNameID::forAscii(runtime, "world");
  assert(c.utf8(runtime) == "world");

  auto d = jsi::PropNameID(runtime, c);
  assert(jsi::PropNameID::compare(runtime, c, d));
}
