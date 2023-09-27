#pragma once

#include <memory>
#include <string>
#include <vector>

#include <assert.h>
#include <js.h>
#include <utf.h>
#include <uv.h>

#include <jsi/instrumentation.h>
#include <jsi/jsi.h>

namespace jsi = facebook::jsi;

struct JSIInstrumentation : jsi::Instrumentation {
  static JSIInstrumentation instance;

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

struct JSIPlatform {
  uv_loop_t loop;
  js_platform_t *platform;

  JSIPlatform() {
    int err;

    err = uv_loop_init(&loop);
    assert(err == 0);

    err = js_create_platform(&loop, nullptr, &platform);
    assert(err == 0);
  }

  JSIPlatform(const JSIPlatform &) = delete;

  ~JSIPlatform() {
    int err;

    err = js_destroy_platform(platform);
    assert(err == 0);

    do {
      err = uv_loop_close(&loop);

      if (err == UV_EBUSY) {
        uv_run(&loop, UV_RUN_DEFAULT);
      }
    } while (err == UV_EBUSY);

    assert(err == 0);
  }

  JSIPlatform &
  operator=(const JSIPlatform &) = delete;
};

struct JSIRuntime : jsi::Runtime {
  uv_loop_t loop;
  js_platform_t *platform;
  js_env_t *env;

  JSIRuntime(js_platform_t *platform)
      : platform(platform) {
    int err;

    err = uv_loop_init(&loop);
    assert(err == 0);

    err = js_create_env(&loop, this->platform, nullptr, &env);
    assert(err == 0);
  }

  JSIRuntime(const JSIPlatform &platform)
      : JSIRuntime(platform.platform) {}

  JSIRuntime(const JSIRuntime &) = delete;

  ~JSIRuntime() override {
    int err;

    err = js_destroy_env(env);
    assert(err == 0);

    do {
      err = uv_loop_close(&loop);

      if (err == UV_EBUSY) {
        uv_run(&loop, UV_RUN_DEFAULT);
      }
    } while (err == UV_EBUSY);

    assert(err == 0);
  }

  JSIRuntime &
  operator=(const JSIRuntime &) = delete;

  jsi::Value
  evaluateJavaScript (
    const std::shared_ptr<const jsi::Buffer> &buffer,
    const std::string &file
  ) override {
    int err;

    js_value_t *source;
    err = js_create_string_utf8(env, buffer->data(), buffer->size(), &source);
    if (err < 0) throw lastException();

    js_value_t *result;
    err = js_run_script(env, file.data(), file.length(), 0, source, &result);
    if (err < 0) throw lastException();

    return as(result);
  }

  std::shared_ptr<const jsi::PreparedJavaScript>
  prepareJavaScript (
    const std::shared_ptr<const jsi::Buffer> &buffer,
    std::string file
  ) override {
    return std::make_shared<JSIPreparedJavaScript>(buffer, file);
  }

  jsi::Value
  evaluatePreparedJavaScript (
    const std::shared_ptr<const jsi::PreparedJavaScript> &js
  ) override {
    auto prepared = static_cast<const JSIPreparedJavaScript *>(js.get());

    return evaluateJavaScript(prepared->buffer, prepared->file);
  }

  bool
  drainMicrotasks (int maxMicrotasksHint = -1) override {
    return true; // Happens automatically at the JavaScript/C boundary
  }

  jsi::Object
  global () override {
    int err;

    js_value_t *global;
    err = js_get_global(env, &global);
    if (err < 0) throw lastException();

    return make<jsi::Object>(global);
  }

  std::string
  description () override {
    int err;

    const char *identifier;
    err = js_get_platform_identifier(platform, &identifier);
    if (err < 0) throw lastException();

    return std::string(identifier);
  }

  bool
  isInspectable () override {
    return false;
  }

  jsi::Instrumentation &
  instrumentation () override {
    return JSIInstrumentation::instance;
  }

protected:
  PointerValue *
  cloneSymbol (const PointerValue *pv) override {
    return new JSIPointerValue(env, as<JSIPointerValue>(pv)->ref);
  }

  PointerValue *
  cloneBigInt (const PointerValue *pv) override {
    return new JSIPointerValue(env, as<JSIPointerValue>(pv)->ref);
  }

  PointerValue *
  cloneString (const PointerValue *pv) override {
    return new JSIPointerValue(env, as<JSIPointerValue>(pv)->ref);
  }

  PointerValue *
  cloneObject (const PointerValue *pv) override {
    return new JSIPointerValue(env, as<JSIPointerValue>(pv)->ref);
  }

  PointerValue *
  clonePropNameID (const PointerValue *pv) override {
    return new JSIPointerValue(env, as<JSIPointerValue>(pv)->ref);
  }

  jsi::PropNameID
  createPropNameIDFromAscii (const char *str, size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_string_utf8(env, reinterpret_cast<const utf8_t *>(str), len, &value);
    if (err < 0) throw lastException();

    return make<jsi::PropNameID>(value);
  }

  jsi::PropNameID
  createPropNameIDFromUtf8 (const uint8_t *str, size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_string_utf8(env, str, len, &value);
    if (err < 0) throw lastException();

    return make<jsi::PropNameID>(value);
  }

  jsi::PropNameID
  createPropNameIDFromString (const jsi::String &str) override {
    return Runtime::make<jsi::PropNameID>(cloneString(getPointerValue(str)));
  }

  jsi::PropNameID
  createPropNameIDFromSymbol (const jsi::Symbol &sym) override {
    return Runtime::make<jsi::PropNameID>(cloneSymbol(getPointerValue(sym)));
  }

  std::string
  utf8 (const jsi::PropNameID &prop) override {
    return as<JSIPointerValue>(prop)->toString(*this);
  }

  bool
  compare (const jsi::PropNameID &a, const jsi::PropNameID &b) override {
    return strictEquals(static_cast<const jsi::Pointer &>(a), b);
  }

  std::string
  symbolToString (const jsi::Symbol &sym) override {
    return global()
      .getPropertyAsFunction(*this, "String")
      .call(*this, jsi::Value(*this, sym))
      .asString(*this)
      .utf8(*this);
  }

  jsi::BigInt
  createBigIntFromInt64 (int64_t n) override {
    int err;

    js_value_t *value;
    err = js_create_bigint_int64(env, n, &value);
    if (err < 0) throw lastException();

    return make<jsi::BigInt>(value);
  }

  jsi::BigInt
  createBigIntFromUint64 (uint64_t n) override {
    int err;

    js_value_t *value;
    err = js_create_bigint_uint64(env, n, &value);
    if (err < 0) throw lastException();

    return make<jsi::BigInt>(value);
  }

  bool
  bigintIsInt64 (const jsi::BigInt &bigint) override {
    int err;

    bool lossless;
    err = js_get_value_bigint_int64(env, as(bigint), nullptr, &lossless);
    if (err < 0) throw lastException();

    return lossless;
  }

  bool
  bigintIsUint64 (const jsi::BigInt &bigint) override {
    int err;

    bool lossless;
    err = js_get_value_bigint_uint64(env, as(bigint), nullptr, &lossless);
    if (err < 0) throw lastException();

    return lossless;
  }

  uint64_t
  truncate (const jsi::BigInt &bigint) override {
    int err;

    uint64_t value;
    err = js_get_value_bigint_uint64(env, as(bigint), &value, nullptr);
    if (err < 0) throw lastException();

    return value;
  }

  jsi::String
  bigintToString (const jsi::BigInt &bigint, int radix) override {
    return global()
      .getPropertyAsFunction(*this, "String")
      .call(*this, jsi::Value(*this, bigint))
      .asString(*this);
  }

  jsi::String
  createStringFromAscii (const char *str, size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_string_utf8(env, reinterpret_cast<const utf8_t *>(str), len, &value);
    if (err < 0) throw lastException();

    return make<jsi::String>(value);
  }

  jsi::String
  createStringFromUtf8 (const uint8_t *str, size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_string_utf8(env, str, len, &value);
    if (err < 0) throw lastException();

    return make<jsi::String>(value);
  }

  std::string
  utf8 (const jsi::String &string) override {
    return as<JSIPointerValue>(string)->toString(*this);
  }

  jsi::Value
  createValueFromJsonUtf8 (const uint8_t *json, size_t length) override {
    return global()
      .getPropertyAsObject(*this, "JSON")
      .getPropertyAsFunction(*this, "parse")
      .call(*this, jsi::String::createFromUtf8(*this, json, length));
  }

  jsi::Object
  createObject () override {
    int err;

    js_value_t *result;
    err = js_create_object(env, &result);
    if (err < 0) throw lastException();

    return make<jsi::Object>(result);
  }

  jsi::Object
  createObject (std::shared_ptr<jsi::HostObject> object) override {
    int err;

    auto ref = new JSIHostObjectReference(*this, std::move(object));

    js_delegate_callbacks_t callbacks = {
      .get = JSIHostObjectReference::get,
      .set = JSIHostObjectReference::set,
      .own_keys = JSIHostObjectReference::ownKeys,
    };

    js_value_t *result;
    err = js_create_delegate(env, &callbacks, ref, finalize<JSIHostObjectReference>, ref, &result);
    if (err < 0) throw lastException();

    err = js_wrap(env, result, ref, nullptr, nullptr, nullptr);
    assert(err == 0);

    err = js_add_type_tag(env, result, &JSIHostObjectReference::tag);
    assert(err == 0);

    return make<jsi::Object>(result);
  }

  std::shared_ptr<jsi::HostObject>
  getHostObject (const jsi::Object &object) override {
    int err;

    JSIHostObjectReference *ref;
    err = js_unwrap(env, as(object), reinterpret_cast<void **>(&ref));
    assert(err == 0);

    return ref->object;
  }

  jsi::HostFunctionType &
  getHostFunction (const jsi::Function &function) override {
    int err;

    JSIHostFunctionReference *ref;
    err = js_unwrap(env, as(function), reinterpret_cast<void **>(&ref));
    assert(err == 0);

    return ref->function;
  }

  bool
  hasNativeState (const jsi::Object &object) override {
    int err;

    bool result;
    err = js_check_type_tag(env, as(object), &JSINativeStateReference::tag, &result);
    assert(err == 0);

    return result;
  }

  std::shared_ptr<jsi::NativeState>
  getNativeState (const jsi::Object &object) override {
    int err;

    JSINativeStateReference *ref;
    err = js_unwrap(env, as(object), reinterpret_cast<void **>(&ref));
    assert(err == 0);

    return ref->state;
  }

  void
  setNativeState (const jsi::Object &object, std::shared_ptr<jsi::NativeState> state) override {
    int err;

    auto ref = new JSINativeStateReference(std::move(state));

    auto result = as(object);

    err = js_wrap(env, result, ref, finalize<JSINativeStateReference>, ref, nullptr);
    if (err < 0) throw lastException();

    err = js_add_type_tag(env, result, &JSINativeStateReference::tag);
    assert(err == 0);
  }

  jsi::Value
  getProperty (const jsi::Object &object, const jsi::PropNameID &key) override {
    int err;

    js_value_t *value;
    err = js_get_property(env, as(object), as(key), &value);
    if (err < 0) throw lastException();

    return as(value);
  }

  jsi::Value
  getProperty (const jsi::Object &object, const jsi::String &key) override {
    int err;

    js_value_t *value;
    err = js_get_property(env, as(object), as(key), &value);
    if (err < 0) throw lastException();

    return as(value);
  }

  bool
  hasProperty (const jsi::Object &object, const jsi::PropNameID &key) override {
    int err;

    bool result;
    err = js_has_property(env, as(object), as(key), &result);
    if (err < 0) throw lastException();

    return result;
  }

  bool
  hasProperty (const jsi::Object &object, const jsi::String &key) override {
    int err;

    bool result;
    err = js_has_property(env, as(object), as(key), &result);
    if (err < 0) throw lastException();

    return result;
  }

  void
  setPropertyValue (const jsi::Object &object, const jsi::PropNameID &key, const jsi::Value &value) override {
    int err;

    bool result;
    err = js_set_property(env, as(object), as(key), as(value));
    if (err < 0) throw lastException();
  }

  void
  setPropertyValue (const jsi::Object &object, const jsi::String &key, const jsi::Value &value) override {
    int err;

    bool result;
    err = js_set_property(env, as(object), as(key), as(value));
    if (err < 0) throw lastException();
  }

  bool
  isArray (const jsi::Object &object) const override {
    int err;

    bool result;
    err = js_is_array(env, as(object), &result);
    assert(err == 0);

    return result;
  }

  bool
  isArrayBuffer (const jsi::Object &object) const override {
    int err;

    bool result;
    err = js_is_arraybuffer(env, as(object), &result);
    assert(err == 0);

    return result;
  }

  bool
  isFunction (const jsi::Object &object) const override {
    int err;

    bool result;
    err = js_is_function(env, as(object), &result);
    assert(err == 0);

    return result;
  }

  bool
  isHostObject (const jsi::Object &object) const override {
    int err;

    bool result;
    err = js_check_type_tag(env, as(object), &JSIHostObjectReference::tag, &result);
    assert(err == 0);

    return result;
  }

  bool
  isHostFunction (const jsi::Function &function) const override {
    int err;

    bool result;
    err = js_check_type_tag(env, as(function), &JSIHostFunctionReference::tag, &result);
    assert(err == 0);

    return result;
  }

  jsi::Array
  getPropertyNames (const jsi::Object &object) override {
    int err;

    js_value_t *value;
    err = js_get_property_names(env, as(object), &value);
    if (err < 0) throw lastException();

    return make<jsi::Array>(value);
  }

  jsi::WeakObject
  createWeakObject (const jsi::Object &object) override {
    return make<jsi::WeakObject>(as(object));
  }

  jsi::Value
  lockWeakObject (const jsi::WeakObject &object) override {
    return make<jsi::Object>(as(object));
  }

  jsi::Array
  createArray (size_t len) override {
    int err;

    js_value_t *value;
    err = js_create_array_with_length(env, len, &value);
    if (err < 0) throw lastException();

    return make<jsi::Array>(value);
  }

  jsi::ArrayBuffer
  createArrayBuffer (std::shared_ptr<jsi::MutableBuffer> buffer) override {
    int err;

    auto ref = new JSIArrayBufferReference(std::move(buffer));

    js_value_t *value;
    err = js_create_external_arraybuffer(env, ref->buffer->data(), ref->buffer->size(), finalize<JSIArrayBufferReference>, ref, &value);
    if (err < 0) throw lastException();

    return make<jsi::ArrayBuffer>(value);
  }

  size_t
  size (const jsi::Array &array) override {
    int err;

    uint32_t len;
    err = js_get_array_length(env, as(array), &len);
    if (err < 0) throw lastException();

    return len;
  }

  size_t
  size (const jsi::ArrayBuffer &arraybuffer) override {
    int err;

    size_t len;
    err = js_get_arraybuffer_info(env, as(arraybuffer), nullptr, &len);
    if (err < 0) throw lastException();

    return len;
  }

  uint8_t *
  data (const jsi::ArrayBuffer &arraybuffer) override {
    int err;

    uint8_t *data;
    err = js_get_arraybuffer_info(env, as(arraybuffer), reinterpret_cast<void **>(data), nullptr);
    if (err < 0) throw lastException();

    return data;
  }

  jsi::Value
  getValueAtIndex (const jsi::Array &array, size_t i) override {
    int err;

    js_value_t *value;
    err = js_get_element(env, as(array), i, &value);
    if (err < 0) throw lastException();

    return as(value);
  }

  void
  setValueAtIndexImpl (const jsi::Array &array, size_t i, const jsi::Value &value) override {
    int err;

    err = js_set_element(env, as(array), i, as(value));
    if (err < 0) throw lastException();
  }

  jsi::Function
  createFunctionFromHostFunction (const jsi::PropNameID &name, unsigned int argc, jsi::HostFunctionType function) override {
    int err;

    auto ref = new JSIHostFunctionReference(*this, function);

    auto str = as<JSIPointerValue>(name)->toString(*this);

    js_value_t *result;
    err = js_create_function(env, str.data(), str.length(), JSIHostFunctionReference::call, ref, &result);
    if (err < 0) throw lastException();

    err = js_wrap(env, result, ref, finalize<JSIHostFunctionReference>, ref, nullptr);
    assert(err == 0);

    err = js_add_type_tag(env, result, &JSIHostFunctionReference::tag);
    assert(err == 0);

    return make<jsi::Function>(result);
  }

  jsi::Value
  call (const jsi::Function &function, const jsi::Value &receiver, const jsi::Value *args, size_t count) override {
    int err;

    std::vector<js_value_t *> argv;

    argv.reserve(count);

    for (size_t i = 0; i < count; i++) {
      argv.push_back(as(args[i]));
    }

    js_value_t *result;
    err = js_call_function(env, as(receiver), as(function), count, argv.data(), &result);
    if (err < 0) throw lastException();

    return as(result);
  }

  jsi::Value
  callAsConstructor (const jsi::Function &constructor, const jsi::Value *args, size_t count) override {
    int err;

    std::vector<js_value_t *> argv;

    argv.reserve(count);

    for (size_t i = 0; i < count; i++) {
      argv.push_back(as(args[i]));
    }

    js_value_t *result;
    err = js_new_instance(env, as(constructor), count, argv.data(), &result);
    if (err < 0) throw lastException();

    return as(result);
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
    return strictEquals(static_cast<const jsi::Pointer &>(a), b);
  }

  bool
  strictEquals (const jsi::BigInt &a, const jsi::BigInt &b) const override {
    return strictEquals(static_cast<const jsi::Pointer &>(a), b);
  }

  bool
  strictEquals (const jsi::String &a, const jsi::String &b) const override {
    return strictEquals(static_cast<const jsi::Pointer &>(a), b);
  }

  bool
  strictEquals (const jsi::Object &a, const jsi::Object &b) const override {
    return strictEquals(static_cast<const jsi::Pointer &>(a), b);
  }

  inline bool
  strictEquals (const jsi::Pointer &a, const jsi::Pointer &b) const {
    int err;

    bool result;
    err = js_strict_equals(env, as(a), as(b), &result);
    assert(err == 0);

    return result;
  }

  bool
  instanceOf (const jsi::Object &object, const jsi::Function &constructor) override {
    int err;

    bool result;
    err = js_instanceof(env, as(object), as(constructor), &result);
    if (err < 0) throw lastException();

    return result;
  }

private:
  template <typename T>
  static void
  finalize (js_env_t *, void *data, void *finalize_hint) {
    delete static_cast<T *>(finalize_hint);
  }

  template <typename T>
  inline T
  make (js_value_t *value) const {
    return Runtime::make<T>(new JSIPointerValue(env, value));
  }

  template <>
  inline jsi::WeakObject
  make (js_value_t *value) const {
    return Runtime::make<jsi::WeakObject>(new JSIWeakPointerValue(env, value));
  }

  template <typename T>
  inline const T *
  as (const jsi::Pointer &p) const {
    return as<T>(getPointerValue(p));
  }

  template <typename T>
  inline const T *
  as (const PointerValue *pv) const {
    return static_cast<const T *>(pv);
  }

  inline js_value_t *
  as (const jsi::Pointer &p) const {
    return as(getPointerValue(p));
  }

  inline js_value_t *
  as (const PointerValue *pv) const {
    return as<JSIPointerValue>(pv)->value();
  }

  inline js_value_t *
  as (const jsi::Value &v) const {
    int err;

    js_value_t *value;

    if (v.isUndefined()) {
      err = js_get_undefined(env, &value);
      assert(err == 0);

      return value;
    }

    if (v.isNull()) {
      err = js_get_null(env, &value);
      assert(err == 0);

      return value;
    }

    if (v.isBool()) {
      err = js_get_boolean(env, v.getBool(), &value);
      assert(err == 0);

      return value;
    }

    if (v.isNumber()) {
      err = js_create_double(env, v.getNumber(), &value);
      assert(err == 0);

      return value;
    }

    js_escapable_handle_scope_t *scope;
    err = js_open_escapable_handle_scope(env, &scope);
    assert(err == 0);

    err = js_escape_handle(env, scope, as(getPointerValue(v)), &value);
    assert(err == 0);

    err = js_close_escapable_handle_scope(env, scope);
    assert(err == 0);

    return value;
  }

  inline js_value_t *
  as (const jsi::JSError &err) {
    return as(err.value());
  }

  jsi::Value
  as (js_value_t *v) const {
    int err;

    js_value_type_t type;
    err = js_typeof(env, v, &type);
    assert(err == 0);

    switch (type) {
    case js_undefined:
      return jsi::Value::undefined();

    case js_null:
    default:
      return jsi::Value::null();

    case js_boolean: {
      bool value;
      err = js_get_value_bool(env, v, &value);
      assert(err == 0);

      return jsi::Value(value);
    }

    case js_number: {
      double value;
      err = js_get_value_double(env, v, &value);
      assert(err == 0);

      return jsi::Value(value);
    }

    case js_string:
      return make<jsi::String>(v);

    case js_symbol:
      return make<jsi::Symbol>(v);

    case js_object:
    case js_external:
      return make<jsi::Object>(v);

    case js_function:
      return make<jsi::Function>(v);

    case js_bigint:
      return make<jsi::BigInt>(v);
    }
  }

  inline jsi::JSError
  lastException () {
    int err;

    js_value_t *error;
    err = js_get_and_clear_last_exception(env, &error);
    assert(err == 0);

    return jsi::JSError(*this, as(error));
  }

  struct JSIPreparedJavaScript : jsi::PreparedJavaScript {
    std::shared_ptr<const jsi::Buffer> buffer;
    std::string file;

    JSIPreparedJavaScript(std::shared_ptr<const jsi::Buffer> buffer, std::string file)
        : buffer(buffer),
          file(file) {}

    JSIPreparedJavaScript(const JSIPreparedJavaScript &) = delete;

    JSIPreparedJavaScript &
    operator=(const JSIPreparedJavaScript &) = delete;
  };

  struct JSIPointerValue : PointerValue {
    js_env_t *env;
    js_ref_t *ref;

    JSIPointerValue(js_env_t *env, js_value_t *value)
        : env(env) {
      int err;

      err = js_create_reference(env, value, 1, &ref);
      assert(err == 0);
    }

    JSIPointerValue(js_env_t *env, js_ref_t *ref)
        : env(env),
          ref(ref) {
      int err;

      err = js_reference_ref(env, ref, nullptr);
      assert(err == 0);
    }

    JSIPointerValue(const JSIPointerValue &) = delete;

    ~JSIPointerValue() override {
      int err;

      uint32_t refs;
      err = js_reference_unref(env, ref, &refs);
      assert(err == 0);

      if (refs == 0) {
        err = js_delete_reference(env, ref);
        assert(err == 0);
      }
    }

    JSIPointerValue &
    operator=(const JSIPointerValue &) = delete;

    inline js_value_t *
    value () const {
      int err;

      js_value_t *value;
      err = js_get_reference_value(env, ref, &value);
      assert(err == 0);

      return value;
    }

    inline std::string
    toString (JSIRuntime &runtime) const {
      int err;

      auto value = this->value();

      err = js_coerce_to_string(env, value, &value);
      if (err < 0) throw runtime.lastException();

      size_t len;
      err = js_get_value_string_utf8(env, value, nullptr, 0, &len);
      if (err < 0) throw runtime.lastException();

      std::string str(len, 0);

      err = js_get_value_string_utf8(env, value, reinterpret_cast<utf8_t *>(str.data()), len, nullptr);
      assert(err == 0);

      return std::move(str);
    }

  protected:
    void
    invalidate () override {
      delete this;
    }
  };

  struct JSIWeakPointerValue : PointerValue {
    js_env_t *env;
    js_ref_t *ref;

    JSIWeakPointerValue(js_env_t *env, js_value_t *value)
        : env(env) {
      int err;

      err = js_create_reference(env, value, 0, &ref);
      assert(err == 0);
    }

    JSIWeakPointerValue(const JSIWeakPointerValue &) = delete;

    ~JSIWeakPointerValue() override {
      int err;

      err = js_delete_reference(env, ref);
      assert(err == 0);
    }

    JSIWeakPointerValue &
    operator=(const JSIWeakPointerValue &) = delete;

    inline js_value_t *
    value () const {
      int err;

      js_value_t *value;
      err = js_get_reference_value(env, ref, &value);
      assert(err == 0);

      return value;
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

    JSIArrayBufferReference(const JSIArrayBufferReference &) = delete;

    JSIArrayBufferReference &
    operator=(const JSIArrayBufferReference &) = delete;
  };

  struct JSINativeStateReference {
    static constexpr js_type_tag_t tag = {0x5a84bf0d0e22401b, 0x858564a9aca352c2};

    std::shared_ptr<jsi::NativeState> state;

    JSINativeStateReference(std::shared_ptr<jsi::NativeState> &&state)
        : state(std::move(state)) {}

    JSINativeStateReference(const JSINativeStateReference &) = delete;

    JSINativeStateReference &
    operator=(const JSINativeStateReference &) = delete;
  };

  struct JSIHostObjectReference {
    static constexpr js_type_tag_t tag = {0xc7096ad6f55c4256, 0x8083785f64f282fc};

    JSIRuntime &runtime;
    std::shared_ptr<jsi::HostObject> object;

    JSIHostObjectReference(JSIRuntime &runtime, std::shared_ptr<jsi::HostObject> &&object)
        : runtime(runtime),
          object(std::move(object)) {}

    JSIHostObjectReference(const JSIHostObjectReference &) = delete;

    JSIHostObjectReference &
    operator=(const JSIHostObjectReference &) = delete;

    static js_value_t *
    get (js_env_t *env, js_value_t *property, void *data) {
      int err;

      auto ref = static_cast<JSIHostObjectReference *>(data);

      jsi::Value value;

      try {
        value = ref->object->get(ref->runtime, ref->runtime.make<jsi::PropNameID>(property));
      } catch (const jsi::JSError &error) {
        err = js_throw(env, ref->runtime.as(error));
        assert(err == 0);

        return nullptr;
      }

      return ref->runtime.as(value);
    }

    static bool
    set (js_env_t *env, js_value_t *property, js_value_t *value, void *data) {
      int err;

      auto ref = static_cast<JSIHostObjectReference *>(data);

      try {
        ref->object->set(ref->runtime, ref->runtime.make<jsi::PropNameID>(property), ref->runtime.as(value));
      } catch (const jsi::JSError &error) {
        err = js_throw(env, ref->runtime.as(error));
        assert(err == 0);

        return false;
      }

      return true;
    }

    static js_value_t *
    ownKeys (js_env_t *env, void *data) {
      int err;

      auto ref = static_cast<JSIHostObjectReference *>(data);

      std::vector<jsi::PropNameID> keys;

      try {
        keys = ref->object->getPropertyNames(ref->runtime);
      } catch (const jsi::JSError &error) {
        err = js_throw(env, ref->runtime.as(error));
        assert(err == 0);

        return nullptr;
      }

      js_value_t *result;
      err = js_create_array_with_length(env, keys.size(), &result);
      assert(err == 0);

      for (size_t i = 0, n = keys.size(); i < n; i++) {
        err = js_set_element(env, result, i, ref->runtime.as(keys[i]));
        assert(err == 0);
      }

      return result;
    }
  };

  struct JSIHostFunctionReference {
    static constexpr js_type_tag_t tag = {0xfab8af594a3d4e86, 0xbedeafddfc1ef064};

    JSIRuntime &runtime;
    jsi::HostFunctionType function;

    JSIHostFunctionReference(JSIRuntime &runtime, jsi::HostFunctionType function)
        : runtime(runtime),
          function(function) {}

    JSIHostFunctionReference(const JSIHostFunctionReference &) = delete;

    JSIHostFunctionReference &
    operator=(const JSIHostFunctionReference &) = delete;

    static js_value_t *
    call (js_env_t *env, js_callback_info_t *info) {
      int err;

      JSIHostFunctionReference *ref;

      size_t argc;
      err = js_get_callback_info(env, info, &argc, nullptr, nullptr, nullptr);
      assert(err == 0);

      std::vector<js_value_t *> argv;

      argv.reserve(argc);

      js_value_t *receiver;
      err = js_get_callback_info(env, info, &argc, argv.data(), &receiver, reinterpret_cast<void **>(&ref));
      assert(err == 0);

      std::vector<jsi::Value> args;

      args.reserve(argc);

      for (size_t i = 0; i < argc; i++) {
        args.push_back(ref->runtime.as(argv[i]));
      }

      jsi::Value value;

      try {
        value = ref->function(ref->runtime, ref->runtime.as(receiver), args.data(), argc);
      } catch (const jsi::JSError &error) {
        err = js_throw(env, ref->runtime.as(error));
        assert(err == 0);

        return nullptr;
      }

      return ref->runtime.as(value);
    }
  };
};
