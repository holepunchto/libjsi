#include <assert.h>
#include <jsi/jsi.h>

#include "../include/jsi.h"

int
main () {
  JSIPlatform platform;

  JSIRuntime runtime(platform);

  jsi::Scope scope(runtime);

  auto host = [&runtime] (jsi::Runtime &rt, const jsi::Value &receiver, const jsi::Value *args, size_t count) -> jsi::Value {
    throw jsi::JSError(runtime, "nope");
  };

  auto function = jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forAscii(runtime, "fn"), 0, host);

  try {
    function.call(runtime);

    assert(false);
  } catch (const jsi::JSError &error) {
    assert(error.getMessage() == "nope");
  }
}
