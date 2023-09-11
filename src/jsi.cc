#include <assert.h>
#include <js.h>
#include <utf.h>
#include <uv.h>

#include "jsi/instrumentation.h"
#include "jsi/jsi.h"

#include "../include/jsi.h"

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

struct JSIInstrumentation : jsi::Instrumentation {
  std::string
  getRecordedGCStats () override {
    return "";
  }

  std::unordered_map<std::string, int64_t>
  getHeapInfo (bool) override {
    return std::unordered_map<std::string, int64_t>{};
  }

  void
  collectGarbage (std::string) override {}

  void
  startTrackingHeapObjectStackTraces (
    std::function<void(
      uint64_t,
      std::chrono::microseconds,
      std::vector<HeapStatsUpdate>
    )>
  ) override {}

  void
  stopTrackingHeapObjectStackTraces () override {}

  void
  startHeapSampling (size_t) override {}

  void
  stopHeapSampling (std::ostream &) override {}

  void
  createSnapshotToFile (const std::string &) override {}

  void
  createSnapshotToStream (std::ostream &) override {}

  std::string
  flushAndDisableBridgeTrafficTrace () override {
    std::abort();
  }

  void
  writeBasicBlockProfileTraceToFile (const std::string &) const override {
    std::abort();
  }

  void
  dumpProfilerSymbolsToFile (const std::string &) const override {
    std::abort();
  }
};

struct JSIRuntime : jsi::Runtime {
  friend struct JSIStringValue;
  friend struct JSIObjectValue;

  uv_loop_t loop;
  js_env_t *env;

  JSIRuntime() {
    uv_once(&js_platform_guard, on_js_platform_init);

    int err;

    err = uv_loop_init(&loop);
    assert(err == 0);

    err = js_create_env(&loop, js_platform, nullptr, &env);
    assert(err == 0);
  }

  ~JSIRuntime() override {
    int err;

    err = js_destroy_env(env);
    assert(err == 0);
  }

  jsi::Value
  evaluateJavaScript (
    const std::shared_ptr<const jsi::Buffer> &buffer,
    const std::string &sourceURL
  ) override {
    return jsi::Value::null();
  }

  std::shared_ptr<const jsi::PreparedJavaScript>
  prepareJavaScript (
    const std::shared_ptr<const jsi::Buffer> &buffer,
    std::string sourceURL
  ) override {
    return nullptr;
  }

  jsi::Value
  evaluatePreparedJavaScript (
    const std::shared_ptr<const jsi::PreparedJavaScript> &js
  ) override {
    return jsi::Value::null();
  }

  bool
  drainMicrotasks (int maxMicrotasksHint = -1) override {
    return true;
  }

  jsi::Object
  global () override {
    int err;

    js_value_t *global;
    err = js_get_global(env, &global);
    assert(err == 0);

    return make<jsi::Object>(new JSIObjectValue(env, global));
  }

  std::string
  description () override {
    int err;

    const char *identifier;
    err = js_get_platform_identifier(js_platform, &identifier);
    assert(err == 0);

    return std::string(identifier);
  }

  bool
  isInspectable () override {
    return false;
  }

  jsi::Instrumentation &
  instrumentation () override {
    static JSIInstrumentation instrumentation;
    return instrumentation;
  }

protected:
  PointerValue *
  cloneSymbol (const PointerValue *pv) override {
    return nullptr;
  }

  PointerValue *
  cloneBigInt (const PointerValue *pv) override {
    return nullptr;
  }

  PointerValue *
  cloneString (const PointerValue *pv) override {
    return nullptr;
  }

  PointerValue *
  cloneObject (const PointerValue *pv) override {
    return nullptr;
  }

  PointerValue *
  clonePropNameID (const PointerValue *pv) override {
    return nullptr;
  }

  jsi::PropNameID
  createPropNameIDFromAscii (const char *str, size_t length) override {
    return make<jsi::PropNameID>(nullptr);
  }

  jsi::PropNameID
  createPropNameIDFromUtf8 (const uint8_t *utf8, size_t length) override {
    return make<jsi::PropNameID>(nullptr);
  }

  jsi::PropNameID
  createPropNameIDFromString (const jsi::String &str) override {
    return make<jsi::PropNameID>(nullptr);
  }

  jsi::PropNameID
  createPropNameIDFromSymbol (const jsi::Symbol &sym) override {
    return make<jsi::PropNameID>(nullptr);
  }

  std::string
  utf8 (const jsi::PropNameID &prop) override {
    return nullptr;
  }

  bool
  compare (const jsi::PropNameID &a, const jsi::PropNameID &b) override {
    return false;
  }

  std::string
  symbolToString (const jsi::Symbol &sym) override {
    return nullptr;
  }

  jsi::BigInt
  createBigIntFromInt64 (int64_t value) override {
    return make<jsi::BigInt>(nullptr);
  }

  jsi::BigInt
  createBigIntFromUint64 (uint64_t value) override {
    return make<jsi::BigInt>(nullptr);
  }

  bool
  bigintIsInt64 (const jsi::BigInt &bigint) override {
    return false;
  }

  bool
  bigintIsUint64 (const jsi::BigInt &bigint) override {
    return false;
  }

  uint64_t
  truncate (const jsi::BigInt &bigint) override {
    return 0;
  }

  jsi::String
  bigintToString (const jsi::BigInt &, int) override {
    return make<jsi::String>(nullptr);
  }

  jsi::String
  createStringFromAscii (const char *str, size_t length) override {
    return make<jsi::String>(new JSIStringValue(std::string(str, length)));
  }

  jsi::String
  createStringFromUtf8 (const uint8_t *str, size_t length) override {
    return make<jsi::String>(new JSIStringValue(std::string(reinterpret_cast<const char *>(str), length)));
  }

  std::string
  utf8 (const jsi::String &string) override {
    return reinterpret_cast<const JSIStringValue *>(getPointerValue(string))->str;
  }

  jsi::Value
  createValueFromJsonUtf8 (const uint8_t *json, size_t length) override {
    return jsi::Value::null();
  }

  jsi::Object
  createObject () override {
    int err;

    js_value_t *object;
    err = js_create_object(env, &object);
    assert(err == 0);

    return make<jsi::Object>(new JSIObjectValue(env, object));
  }

  jsi::Object
  createObject (std::shared_ptr<jsi::HostObject> ho) override {
    return make<jsi::Object>(nullptr);
  }

  std::shared_ptr<jsi::HostObject>
  getHostObject (const jsi::Object &object) override {
    return nullptr;
  }

  jsi::HostFunctionType hostFunction = [] (jsi::Runtime &rt, const jsi::Value &receiver, const jsi::Value *args, size_t count) -> jsi::Value {
    return jsi::Value::undefined();
  };

  jsi::HostFunctionType &
  getHostFunction (const jsi::Function &fn) override {
    return hostFunction;
  }

  bool
  hasNativeState (const jsi::Object &object) override {
    return false;
  }

  std::shared_ptr<jsi::NativeState>
  getNativeState (const jsi::Object &object) override {
    return nullptr;
  }

  void
  setNativeState (const jsi::Object &object, std::shared_ptr<jsi::NativeState> state) override {
    return;
  }

  jsi::Value
  getProperty (const jsi::Object &object, const jsi::PropNameID &name) override {
    return jsi::Value::undefined();
  }

  jsi::Value
  getProperty (const jsi::Object &object, const jsi::String &name) override {
    return jsi::Value::undefined();
  }

  bool
  hasProperty (const jsi::Object &object, const jsi::PropNameID &name) override {
    return false;
  }

  bool
  hasProperty (const jsi::Object &object, const jsi::String &name) override {
    return false;
  }

  void
  setPropertyValue (const jsi::Object &object, const jsi::PropNameID &name, const jsi::Value &value) override {
    return;
  }

  void
  setPropertyValue (const jsi::Object &object, const jsi::String &name, const jsi::Value &value) override {
    return;
  }

  bool
  isArray (const jsi::Object &object) const override {
    return false;
  }

  bool
  isArrayBuffer (const jsi::Object &object) const override {
    return false;
  }

  bool
  isFunction (const jsi::Object &object) const override {
    return false;
  }

  bool
  isHostObject (const jsi::Object &object) const override {
    return false;
  }

  bool
  isHostFunction (const jsi::Function &object) const override {
    return false;
  }

  jsi::Array
  getPropertyNames (const jsi::Object &object) override {
    return make<jsi::Array>(nullptr);
  }

  jsi::WeakObject
  createWeakObject (const jsi::Object &object) override {
    return make<jsi::WeakObject>(nullptr);
  }

  jsi::Value
  lockWeakObject (const jsi::WeakObject &object) override {
    return jsi::Value::null();
  }

  jsi::Array
  createArray (size_t length) override {
    return make<jsi::Array>(nullptr);
  }

  jsi::ArrayBuffer
  createArrayBuffer (std::shared_ptr<jsi::MutableBuffer> buffer) override {
    return make<jsi::ArrayBuffer>(nullptr);
  }

  size_t
  size (const jsi::Array &array) override {
    return 0;
  }

  size_t
  size (const jsi::ArrayBuffer &arraybuffer) override {
    return 0;
  }

  uint8_t *
  data (const jsi::ArrayBuffer &arraybuffer) override {
    return nullptr;
  }

  jsi::Value
  getValueAtIndex (const jsi::Array &array, size_t i) override {
    return jsi::Value::null();
  }

  void
  setValueAtIndexImpl (const jsi::Array &array, size_t i, const jsi::Value &value) override {
    return;
  }

  jsi::Function
  createFunctionFromHostFunction (const jsi::PropNameID &name, unsigned int argc, jsi::HostFunctionType fn) override {
    return make<jsi::Function>(nullptr);
  }

  jsi::Value
  call (const jsi::Function &fn, const jsi::Value &receiver, const jsi::Value *args, size_t count) override {
    return jsi::Value::undefined();
  }

  jsi::Value
  callAsConstructor (const jsi::Function &constructor, const jsi::Value *args, size_t count) override {
    return jsi::Value::undefined();
  }

  ScopeState *
  pushScope () override {
    return nullptr;
  }

  void
  popScope (ScopeState *) override {
    return;
  }

  bool
  strictEquals (const jsi::Symbol &a, const jsi::Symbol &b) const override {
    return false;
  }

  bool
  strictEquals (const jsi::BigInt &a, const jsi::BigInt &b) const override {
    return false;
  }

  bool
  strictEquals (const jsi::String &a, const jsi::String &b) const override {
    return false;
  }

  bool
  strictEquals (const jsi::Object &a, const jsi::Object &b) const override {
    return false;
  }

  bool
  instanceOf (const jsi::Object &o, const jsi::Function &f) override {
    return false;
  }

  struct JSIStringValue : PointerValue {
    friend struct JSIRuntime;

    std::string str;

    JSIStringValue(std::string str)
        : str(str) {}

    ~JSIStringValue() {}

  protected:
    void
    invalidate () {
      delete this;
    }
  };

  struct JSIObjectValue : PointerValue {
    friend struct JSIRuntime;

    js_env_t *env;
    js_ref_t *ref;

    JSIObjectValue(js_env_t *env, js_value_t *object)
        : env(env) {
      int err;

      err = js_create_reference(env, object, 1, &ref);
      assert(err == 0);
    }

    ~JSIObjectValue() {
      int err;

      err = js_delete_reference(env, ref);
      assert(err == 0);
    }

  protected:
    void
    invalidate () {
      delete this;
    }
  };
};
