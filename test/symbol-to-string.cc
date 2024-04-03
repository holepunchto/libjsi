#include <assert.h>

#include "../include/jsi.h"

int
main () {
  JSIPlatform platform;

  JSIRuntime runtime(platform);

  jsi::Scope scope(runtime);

  auto symbol = runtime
                  .evaluateJavaScript(std::make_shared<jsi::StringBuffer>("Symbol('foo')"), "test.js")
                  .asSymbol(runtime);

  assert(symbol.toString(runtime) == "Symbol(foo)");
}
