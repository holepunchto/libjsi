#include <assert.h>

#include "../include/jsi.h"

int
main () {
  JSIPlatform platform;

  JSIRuntime runtime(platform);

  jsi::Scope scope(runtime);

  auto bigint = jsi::BigInt::fromInt64(runtime, 123456);

  assert(bigint.toString(runtime).utf8(runtime) == "123456");
}
