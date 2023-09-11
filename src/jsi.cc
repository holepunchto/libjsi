#include <assert.h>
#include <js.h>
#include <utf.h>
#include <uv.h>

#include <jsi/instrumentation.h>
#include <jsi/jsi.h>

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
    const std::string &file
  ) override {
    int err;

    const utf8_t *str = buffer->data();
    size_t len = buffer->size();

    js_value_t *source;
    err = js_create_string_utf8(env, str, len, &source);
    assert(err == 0);

    js_value_t *result;
    err = js_run_script(env, file.data(), file.length(), 0, source, &result);
    assert(err == 0);

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
    auto prepared = reinterpret_cast<const JSIPreparedJavaScript *>(js.get());

    return evaluateJavaScript(prepared->buffer, prepared->file);
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
    return new JSIReferenceValue(env, as<JSIReferenceValue>(pv)->ref);
  }

  PointerValue *
  cloneBigInt (const PointerValue *pv) override {
    return new JSIReferenceValue(env, as<JSIReferenceValue>(pv)->ref);
  }

  PointerValue *
  cloneString (const PointerValue *pv) override {
    return new JSIReferenceValue(env, as<JSIReferenceValue>(pv)->ref);
  }

  PointerValue *
  cloneObject (const PointerValue *pv) override {
    return new JSIReferenceValue(env, as<JSIReferenceValue>(pv)->ref);
  }

  PointerValue *
  clonePropNameID (const PointerValue *pv) override {
    return new JSIReferenceValue(env, as<JSIReferenceValue>(pv)->ref);
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
    return as<JSIReferenceValue>(prop)->toString();
  }

  bool
  compare (const jsi::PropNameID &a, const jsi::PropNameID &b) override {
    return strictEquals(reinterpret_cast<const jsi::Pointer &>(a), b);
  }

  std::string
  symbolToString (const jsi::Symbol &sym) override {
    return as<JSIReferenceValue>(sym)->toString();
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
    return as<JSIReferenceValue>(string)->toString();
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
    int err;

    bool result;
    err = js_is_wrapped(env, as<JSIReferenceValue>(object)->value(), &result);
    assert(err == 0);

    return result;
  }

  std::shared_ptr<jsi::NativeState>
  getNativeState (const jsi::Object &object) override {
    int err;

    JSINativeStateReference *ref;
    err = js_unwrap(env, as<JSIReferenceValue>(object)->value(), reinterpret_cast<void **>(&ref));
    assert(err == 0);

    return ref->state;
  }

  void
  setNativeState (const jsi::Object &object, std::shared_ptr<jsi::NativeState> state) override {
    int err;

    auto ref = new JSINativeStateReference(std::move(state));

    err = js_wrap(env, as<JSIReferenceValue>(object)->value(), ref, on_js_finalize<JSINativeStateReference>, ref, nullptr);
    assert(err == 0);
  }

  jsi::Value
  getProperty (const jsi::Object &object, const jsi::PropNameID &key) override {
    int err;

    js_value_t *value;
    err = js_get_property(env, as<JSIReferenceValue>(object)->value(), as<JSIReferenceValue>(key)->value(), &value);
    assert(err == 0);

    return as(value);
  }

  jsi::Value
  getProperty (const jsi::Object &object, const jsi::String &key) override {
    int err;

    js_value_t *value;
    err = js_get_property(env, as<JSIReferenceValue>(object)->value(), as<JSIReferenceValue>(key)->value(), &value);
    assert(err == 0);

    return as(value);
  }

  bool
  hasProperty (const jsi::Object &object, const jsi::PropNameID &key) override {
    int err;

    bool result;
    err = js_has_property(env, as<JSIReferenceValue>(object)->value(), as<JSIReferenceValue>(key)->value(), &result);
    assert(err == 0);

    return result;
  }

  bool
  hasProperty (const jsi::Object &object, const jsi::String &key) override {
    int err;

    bool result;
    err = js_has_property(env, as<JSIReferenceValue>(object)->value(), as<JSIReferenceValue>(key)->value(), &result);
    assert(err == 0);

    return result;
  }

  void
  setPropertyValue (const jsi::Object &object, const jsi::PropNameID &key, const jsi::Value &value) override {
    int err;

    bool result;
    err = js_set_property(env, as<JSIReferenceValue>(object)->value(), as<JSIReferenceValue>(key)->value(), as(value));
    assert(err == 0);
  }

  void
  setPropertyValue (const jsi::Object &object, const jsi::String &key, const jsi::Value &value) override {
    int err;

    bool result;
    err = js_set_property(env, as<JSIReferenceValue>(object)->value(), as<JSIReferenceValue>(key)->value(), as(value));
    assert(err == 0);
  }

  bool
  isArray (const jsi::Object &object) const override {
    int err;

    bool result;
    err = js_is_array(env, as<JSIReferenceValue>(object)->value(), &result);
    assert(err == 0);

    return result;
  }

  bool
  isArrayBuffer (const jsi::Object &object) const override {
    int err;

    bool result;
    err = js_is_arraybuffer(env, as<JSIReferenceValue>(object)->value(), &result);
    assert(err == 0);

    return result;
  }

  bool
  isFunction (const jsi::Object &object) const override {
    int err;

    bool result;
    err = js_is_function(env, as<JSIReferenceValue>(object)->value(), &result);
    assert(err == 0);

    return result;
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
    int err;

    js_value_t *value;
    err = js_get_property_names(env, as<JSIReferenceValue>(object)->value(), &value);
    assert(err == 0);

    return make<jsi::Array>(new JSIReferenceValue(env, value));
  }

  jsi::WeakObject
  createWeakObject (const jsi::Object &object) override {
    return make<jsi::WeakObject>(new JSIWeakReferenceValue(env, as<JSIReferenceValue>(object)->value()));
  }

  jsi::Value
  lockWeakObject (const jsi::WeakObject &object) override {
    return make<jsi::Object>(new JSIReferenceValue(env, as<JSIWeakReferenceValue>(object)->value()));
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
    int err;

    uint32_t len;
    err = js_get_array_length(env, as<JSIReferenceValue>(array)->value(), &len);
    assert(err == 0);

    return len;
  }

  size_t
  size (const jsi::ArrayBuffer &arraybuffer) override {
    int err;

    size_t len;
    err = js_get_arraybuffer_info(env, as<JSIReferenceValue>(arraybuffer)->value(), nullptr, &len);
    assert(err == 0);

    return len;
  }

  uint8_t *
  data (const jsi::ArrayBuffer &arraybuffer) override {
    int err;

    uint8_t *data;
    err = js_get_arraybuffer_info(env, as<JSIReferenceValue>(arraybuffer)->value(), reinterpret_cast<void **>(data), nullptr);
    assert(err == 0);

    return data;
  }

  jsi::Value
  getValueAtIndex (const jsi::Array &array, size_t i) override {
    int err;

    js_value_t *value;
    err = js_get_element(env, as<JSIReferenceValue>(array)->value(), i, &value);
    assert(err == 0);

    return as(value);
  }

  void
  setValueAtIndexImpl (const jsi::Array &array, size_t i, const jsi::Value &value) override {
    int err;

    err = js_set_element(env, as<JSIReferenceValue>(array)->value(), i, as(value));
    assert(err == 0);
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
    return strictEquals(reinterpret_cast<const jsi::Pointer &>(a), b);
  }

  bool
  strictEquals (const jsi::BigInt &a, const jsi::BigInt &b) const override {
    return strictEquals(reinterpret_cast<const jsi::Pointer &>(a), b);
  }

  bool
  strictEquals (const jsi::String &a, const jsi::String &b) const override {
    return strictEquals(reinterpret_cast<const jsi::Pointer &>(a), b);
  }

  bool
  strictEquals (const jsi::Object &a, const jsi::Object &b) const override {
    return strictEquals(reinterpret_cast<const jsi::Pointer &>(a), b);
  }

  inline bool
  strictEquals (const jsi::Pointer &a, const jsi::Pointer &b) const {
    int err;

    bool result;
    err = js_strict_equals(env, as<JSIReferenceValue>(a)->value(), as<JSIReferenceValue>(b)->value(), &result);
    assert(err);

    return result;
  }

  bool
  instanceOf (const jsi::Object &object, const jsi::Function &constructor) override {
    int err;

    bool result;
    err = js_instanceof(env, as<JSIReferenceValue>(object)->value(), as<JSIReferenceValue>(constructor)->value(), &result);
    assert(err == 0);

    return result;
  }

private:
  template <typename T>
  inline const T *
  as (const jsi::Value &v) const {
    return as<T>(getPointerValue(v));
  }

  template <typename T>
  inline const T *
  as (const jsi::Pointer &p) const {
    return as<T>(getPointerValue(p));
  }

  template <typename T>
  inline const T *
  as (const PointerValue *pv) const {
    return reinterpret_cast<const T *>(pv);
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

    err = js_escape_handle(env, scope, as<JSIReferenceValue>(v)->value(), &value);
    assert(err == 0);

    err = js_close_escapable_handle_scope(env, scope);
    assert(err == 0);

    return value;
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
      return make<jsi::String>(new JSIReferenceValue(env, v));

    case js_symbol:
      return make<jsi::Symbol>(new JSIReferenceValue(env, v));

    case js_object:
    case js_external:
      return make<jsi::Object>(new JSIReferenceValue(env, v));

    case js_function:
      return make<jsi::Function>(new JSIReferenceValue(env, v));

    case js_bigint:
      return make<jsi::BigInt>(new JSIReferenceValue(env, v));
    }
  }

  struct JSIPreparedJavaScript : jsi::PreparedJavaScript {
    std::shared_ptr<const jsi::Buffer> buffer;
    std::string file;

    JSIPreparedJavaScript(std::shared_ptr<const jsi::Buffer> &&buffer, std::string file)
        : buffer(std::move(buffer)),
          file(file) {}
  };

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

  struct JSIWeakReferenceValue : PointerValue {
    friend struct JSIRuntime;

    js_env_t *env;
    js_ref_t *ref;

    JSIWeakReferenceValue(js_env_t *env, js_value_t *value)
        : env(env) {
      int err;

      err = js_create_reference(env, value, 0, &ref);
      assert(err == 0);
    }

    ~JSIWeakReferenceValue() override {
      int err;

      err = js_delete_reference(env, ref);
      assert(err == 0);
    }

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

    size_t
    size () const {
      return buffer->size();
    }

    uint8_t *
    data () {
      return buffer->data();
    }
  };

  struct JSINativeStateReference {
    std::shared_ptr<jsi::NativeState> state;

    JSINativeStateReference(std::shared_ptr<jsi::NativeState> &&state)
        : state(std::move(state)) {}
  };
};
