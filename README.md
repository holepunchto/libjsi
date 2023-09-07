# libnapi

Node-API compatibility layer for https://github.com/holepunchto/libjs. It maintains ABI compatibility with a subset of the Node-API symbols, documented at https://nodejs.org/api/n-api.html; support for the entire set of Node-API symbols is, at least for the present, a non-goal.

## API

See [`include/napi.h`](include/napi.h) for the public API.

### Exceptions

Unlike the Node-API implementation in Node.js, the compatibility layer relies exclusively on JavaScript exceptions for error handling. The only possible return values for an API call is therefore `napi_ok` and `napi_pending_exception`. In case of the latter, make sure to call `napi_get_and_clear_last_exception()` to recover from the error if possible. The error may be rethrown with `napi_throw()`.

### Modules

Node-API module support is provided by the [`include/napi/module.h`](include/napi/module.h) header and depends on the runtime providing an implementation of the `napi_module_register()` function for constructor based registration:

```c
#include <napi/module.h>

void
napi_module_register (napi_module *mod) {
  // Do something with `mod`.
}
```

For symbol based registration, the known symbol `napi_register_module_v1()` will be exported by the module.

## License

Apache-2.0
