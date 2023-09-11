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

template <typename T>
static void
on_js_finalize (js_env_t *, void *data, void *finalize_hint) {
  delete reinterpret_cast<T *>(finalize_hint);
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

static JSIInstrumentation instrumentation;

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

    return make<jsi::Object>(new JSIReferenceValue(env, global));
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
    return ::instrumentation;
  }

protected:
  PointerValue *
  cloneSymbol (const PointerValue *pv) override {
    return new JSIReferenceValue(env, reinterpret_cast<const JSIReferenceValue *>(pv)->ref);
  }

  PointerValue *
  cloneBigInt (const PointerValue *pv) override {
    return new JSIReferenceValue(env, reinterpret_cast<const JSIReferenceValue *>(pv)->ref);
  }

  PointerValue *
  cloneString (const PointerValue *pv) override {
    return new JSIReferenceValue(env, reinterpret_cast<const JSIReferenceValue *>(pv)->ref);
  }

  PointerValue *
  cloneObject (const PointerValue *pv) override {
    return new JSIReferenceValue(env, reinterpret_cast<const JSIReferenceValue *>(pv)->ref);
  }

  PointerValue *
  clonePropNameID (const PointerValue *pv) override {
    return new JSIReferenceValue(env, reinterpret_cast<const JSIReferenceValue *>(pv)->ref);
  }

  jsi::PropNameID
  createPropNameIDFromAscii (const char *str, size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_string_utf8(env, reinterpret_cast<const utf8_t *>(str), len, &value);
    assert(err == 0);

    return make<jsi::PropNameID>(new JSIReferenceValue(env, value));
  }

  jsi::PropNameID
  createPropNameIDFromUtf8 (const uint8_t *str, size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_string_utf8(env, str, len, &value);
    assert(err == 0);

    return make<jsi::PropNameID>(new JSIReferenceValue(env, value));
  }

  jsi::PropNameID
  createPropNameIDFromString (const jsi::String &str) override {
    return make<jsi::PropNameID>(cloneString(getPointerValue(str)));
  }

  jsi::PropNameID
  createPropNameIDFromSymbol (const jsi::Symbol &sym) override {
    return make<jsi::PropNameID>(cloneSymbol(getPointerValue(sym)));
  }

  std::string
  utf8 (const jsi::PropNameID &prop) override {
    return reinterpret_cast<const JSIReferenceValue *>(getPointerValue(prop))->toString();
  }

  bool
  compare (const jsi::PropNameID &a, const jsi::PropNameID &b) override {
    return false;
  }

  std::string
  symbolToString (const jsi::Symbol &sym) override {
    return reinterpret_cast<const JSIReferenceValue *>(getPointerValue(sym))->toString();
  }

  jsi::BigInt
  createBigIntFromInt64 (int64_t n) override {
    int err;

    js_value_t *value;
    err = js_create_bigint_int64(env, n, &value);
    assert(err == 0);

    return make<jsi::BigInt>(new JSIReferenceValue(env, value));
  }

  jsi::BigInt
  createBigIntFromUint64 (uint64_t n) override {
    int err;

    js_value_t *value;
    err = js_create_bigint_uint64(env, n, &value);
    assert(err == 0);

    return make<jsi::BigInt>(new JSIReferenceValue(env, value));
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
  createStringFromAscii (const char *str, size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_string_utf8(env, reinterpret_cast<const utf8_t *>(str), len, &value);
    assert(err == 0);

    return make<jsi::String>(new JSIReferenceValue(env, value));
  }

  jsi::String
  createStringFromUtf8 (const uint8_t *str, size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_string_utf8(env, str, len, &value);
    assert(err == 0);

    return make<jsi::String>(new JSIReferenceValue(env, value));
  }

  std::string
  utf8 (const jsi::String &string) override {
    return reinterpret_cast<const JSIReferenceValue *>(getPointerValue(string))->toString();
  }

  jsi::Value
  createValueFromJsonUtf8 (const uint8_t *json, size_t length) override {
    return jsi::Value::null();
  }

  jsi::Object
  createObject () override {
    int err;

    js_value_t *value;
    err = js_create_object(env, &value);
    assert(err == 0);

    return make<jsi::Object>(new JSIReferenceValue(env, value));
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
  createArray (size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_array_with_length(env, len, &value);
    assert(err == 0);

    return make<jsi::Array>(new JSIReferenceValue(env, value));
  }

  jsi::ArrayBuffer
  createArrayBuffer (std::shared_ptr<jsi::MutableBuffer> buffer) override {
    int err;

    auto ref = new JSIArrayBufferReference(std::move(buffer));

    js_value_t *value;
    err = js_create_external_arraybuffer(env, ref->data(), ref->size(), on_js_finalize<JSIArrayBufferReference>, ref, &value);
    assert(err == 0);

    return make<jsi::ArrayBuffer>(new JSIReferenceValue(env, value));
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

private:
  struct JSIReferenceValue : PointerValue {
    friend struct JSIRuntime;

    js_env_t *env;
    js_ref_t *ref;

    JSIReferenceValue(js_env_t *env, js_value_t *value)
        : env(env) {
      int err;

      err = js_create_reference(env, value, 1, &ref);
      assert(err == 0);
    }

    JSIReferenceValue(js_env_t *env, js_ref_t *ref)
        : env(env),
          ref(ref) {
      int err;

      err = js_reference_ref(env, ref, nullptr);
      assert(err == 0);
    }

    ~JSIReferenceValue() override {
      int err;

      uint32_t refs;
      err = js_reference_unref(env, ref, &refs);
      assert(err == 0);

      if (refs == 0) {
        err = js_delete_reference(env, ref);
        assert(err == 0);
      }
    }

    inline js_value_t *
    value () const {
      int err;

      js_value_t *value;
      err = js_get_reference_value(env, ref, &value);
      assert(err == 0);

      return value;
    }

    inline std::string
    toString () const {
      int err;

      auto value = this->value();

      size_t len;
      err = js_get_value_string_utf8(env, value, nullptr, 0, &len);
      assert(err == 0);

      std::string str;

      str.reserve(len);

      err = js_get_value_string_utf8(env, value, reinterpret_cast<utf8_t *>(str.data()), len, nullptr);
      assert(err == 0);

      return str;
    }

  protected:
    void
    invalidate () override {
      delete this;
    }
  };

  struct JSIArrayBufferReference {
    std::shared_ptr<jsi::MutableBuffer> buffer;

    JSIArrayBufferReference(std::shared_ptr<jsi::MutableBuffer> &&buffer)
        : buffer(std::move(buffer)) {}

    size_t
    size () const {
      return buffer->size();
    }

    uint8_t *
    data () {
      return buffer->data();
    }
  };
};
