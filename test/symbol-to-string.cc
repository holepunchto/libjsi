#include <assert.h>
#include <jsi/jsi.h>

#include "../include/jsi.h"

int
main () {
  JSIPlatform platform;

  JSIRuntime runtime(platform);

  auto symbol = runtime
                  .evaluateJavaScript(std::make_shared<jsi::StringBuffer>("Symbol('foo')"), "test.js")
                  .asSymbol(runtime);

  assert(symbol.toString(runtime) == "Symbol(foo)");
}
