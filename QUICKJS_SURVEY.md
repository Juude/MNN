# QuickJS Integration Feasibility Survey

## Objective
Investigate whether it is possible to use QuickJS to call the existing MNN JavaScript binding (`js/src`) and execute the existing examples.

## Findings

### 1. Reuse of Existing Binding (`js/src`)
The existing MNN JavaScript binding is implemented using **Node-API (N-API)** via the `node-addon-api` C++ wrapper. This architecture relies on:
- The `<napi.h>` header and its associated symbols (e.g., `napi_create_object`, `napi_define_class`).
- The Node.js module loading mechanism for `.node` shared objects.
- V8-specific features abstracted by N-API.

**Feasibility:** **Infeasible**
- **No QuickJS N-API Implementation:** Extensive research confirms that there is currently no mature, open-source implementation of the Node-API (N-API) for QuickJS. While QuickJS is a compliant JavaScript engine, it does not natively expose the N-API ABI required by `node-addon-api`.
- **Binary Incompatibility:** Modules compiled for Node.js (`.node` files) cannot be loaded by QuickJS binaries (`qjs`) due to ABI differences and missing symbols.
- **Source Incompatibility:** Recompiling the `js/src/*.cc` files against QuickJS is impossible without a library that maps N-API calls to QuickJS C API calls. No such library currently exists in a usable state.

### 2. Execution of Examples
Since the existing examples (`js/test/*.js`, `js/examples/*.js`) depend on the N-API binding (e.g., `require('./mnn.node')`), they **cannot be executed** using QuickJS in their current form.

## Alternative Approach: QuickJS FFI + MNN C API

While "reusing" the N-API binding is not possible, using MNN from QuickJS is feasible through a different path:

1.  **MNN C API:** The repository contains a C-compatible wrapper for the MNN C++ API, located in `rust/csrc/mnn_c.h` and `rust/csrc/mnn_c.cpp`. This interface exposes key functionalities (Interpreter, Session, Tensor, LLM) as standard C functions.
2.  **QuickJS FFI:** Runtimes like `txiki.js` (based on QuickJS) or libraries like `quickjs-ffi` allow QuickJS code to call C functions in shared libraries (`.so`/`.dylib`) using `dlopen`/`dlsym`.

**Proposed Path for QuickJS Support:**
To support QuickJS, a new binding layer should be created that utilizes the C API via FFI, rather than attempting to port the N-API C++ binding.

### Example FFI Usage (Conceptual)
```javascript
// Using txiki.js FFI
const { dlopen, dlsym, ptr } = tjs.ffi;
const lib = dlopen('./libmnn_c.so'); // Compiled from rust/csrc
const mnn_create = dlsym(lib, 'mnn_interpreter_create_from_file');
// ... bind and call
```

## Conclusion
The request to "reuse the js binding" (N-API based) with QuickJS is not technically feasible due to the lack of N-API support in the QuickJS ecosystem. Future efforts should focus on a lightweight FFI-based binding using the C interface.
