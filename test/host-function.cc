#include <assert.h>

#include "../include/jsi.h"

int
main () {
  JSIPlatform platform;

  JSIRuntime runtime(platform);

  jsi::Scope scope(runtime);

  bool called = false;

  auto host = [&called] (jsi::Runtime &rt, const jsi::Value &receiver, const jsi::Value *args, size_t count) -> jsi::Value {
    called = true;

    return jsi::Value(42);
  };

  auto function = jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forAscii(runtime, "fn"), 0, host);

  assert(function.isHostFunction(runtime));
  assert(function.getHostFunction(runtime));

  auto result = function.call(runtime);

  assert(called);
  assert(result.isNumber());
  assert(result.getNumber() == 42);
}
