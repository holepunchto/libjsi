#include <assert.h>
#include <jsi/jsi.h>

#include "../include/jsi.h"

struct HostObject : jsi::HostObject {
  jsi::Value
  get (jsi::Runtime &runtime, const jsi::PropNameID &id) override {
    throw jsi::JSError(runtime, "get failed");
  }

  void
  set (jsi::Runtime &runtime, const jsi::PropNameID &id, const jsi::Value &value) override {
    throw jsi::JSError(runtime, "set failed");
  }
};

int
main () {
  JSIPlatform platform;

  JSIRuntime runtime(platform);

  jsi::Scope scope(runtime);

  auto host = std::make_shared<HostObject>();

  auto object = jsi::Object::createFromHostObject(runtime, host);

  try {
    object.getProperty(runtime, "foo");

    assert(false);
  } catch (const jsi::JSError &error) {
    assert(error.getMessage() == "get failed");
  }

  try {
    object.setProperty(runtime, "foo", jsi::Value(42));

    assert(false);
  } catch (const jsi::JSError &error) {
    assert(error.getMessage() == "set failed");
  }
}
