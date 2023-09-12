#include <assert.h>
#include <jsi/jsi.h>

#include "../include/jsi.h"

bool getCalled = false;
bool setCalled = false;

struct HostObject : jsi::HostObject {
  jsi::Value
  get (jsi::Runtime &runtime, const jsi::PropNameID &id) override {
    getCalled = true;

    assert(id.utf8(runtime) == "foo");

    return jsi::Value(42);
  }

  void
  set (jsi::Runtime &runtime, const jsi::PropNameID &id, const jsi::Value &value) override {
    setCalled = true;

    assert(id.utf8(runtime) == "foo");
    assert(value.isNumber());
    assert(value.getNumber() == 42);
  }
};

int
main () {
  JSIPlatform platform;

  JSIRuntime runtime(platform);

  auto object = runtime
                  .global()
                  .createFromHostObject(runtime, std::make_shared<HostObject>());

  assert(object.isHostObject(runtime));

  auto value = object.getProperty(runtime, "foo");
  assert(getCalled);
  assert(value.isNumber());
  assert(value.getNumber() == 42);

  object.setProperty(runtime, "foo", jsi::Value(42));
  assert(setCalled);
}
