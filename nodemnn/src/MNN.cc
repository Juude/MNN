#include <napi.h>
#include <MNN/MNNDefine.h>
#include <MNN/Interpreter.hpp>
#include <MNN/Tensor.hpp>
#include <vector>
#include <iostream>

class TensorWrapper : public Napi::ObjectWrap<TensorWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::Object NewInstance(Napi::Env env, MNN::Tensor* tensor, bool owned);
    TensorWrapper(const Napi::CallbackInfo& info);
    ~TensorWrapper();
    MNN::Tensor* GetInternalTensor();

private:
    static Napi::FunctionReference constructor;
    MNN::Tensor* m_tensor;
    bool m_owned;

    Napi::Value GetShape(const Napi::CallbackInfo& info);
    Napi::Value GetData(const Napi::CallbackInfo& info);
    Napi::Value SetData(const Napi::CallbackInfo& info);
    Napi::Value Print(const Napi::CallbackInfo& info);
    Napi::Value CopyFrom(const Napi::CallbackInfo& info);
    Napi::Value CopyTo(const Napi::CallbackInfo& info);

    friend class InterpreterWrapper;
};

Napi::FunctionReference TensorWrapper::constructor;

Napi::Object TensorWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function func = DefineClass(env, "Tensor", {
        InstanceMethod("shape", &TensorWrapper::GetShape),
        InstanceMethod("getData", &TensorWrapper::GetData),
        InstanceMethod("setData", &TensorWrapper::SetData),
        InstanceMethod("print", &TensorWrapper::Print),
        InstanceMethod("copyFrom", &TensorWrapper::CopyFrom),
        InstanceMethod("copyTo", &TensorWrapper::CopyTo),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("Tensor", func);
    return exports;
}

Napi::Object TensorWrapper::NewInstance(Napi::Env env, MNN::Tensor* tensor, bool owned) {
    Napi::EscapableHandleScope scope(env);
    Napi::Value external = Napi::External<MNN::Tensor>::New(env, tensor);
    Napi::Object object = constructor.New({ external, Napi::Boolean::New(env, owned) });
    return scope.Escape(object).As<Napi::Object>();
}

TensorWrapper::TensorWrapper(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<TensorWrapper>(info), m_tensor(nullptr), m_owned(false) {

    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);

    if (info.Length() >= 1 && info[0].IsExternal()) {
        m_tensor = info[0].As<Napi::External<MNN::Tensor>>().Data();
        if (info.Length() >= 2 && info[1].IsBoolean()) {
            m_owned = info[1].As<Napi::Boolean>().Value();
        } else {
            m_owned = false;
        }
        return;
    }

    if (info.Length() < 1 || !info[0].IsArray()) {
         Napi::TypeError::New(env, "Expected array of numbers for shape").ThrowAsJavaScriptException();
         return;
    }

    Napi::Array shapeArray = info[0].As<Napi::Array>();
    std::vector<int> shape;
    for(uint32_t i=0; i<shapeArray.Length(); ++i) {
        Napi::Value val = shapeArray[i];
        if (val.IsNumber()) {
            shape.push_back(val.As<Napi::Number>().Int32Value());
        }
    }

    // Default to float, but allow specifying dimension type (e.g. Tensorflow/Caffe)
    // For manual creation from JS, usually we want NCHW or TensorFlow layout.
    // MNN::Tensor::create uses TensorFlow layout by default if not specified.
    m_tensor = MNN::Tensor::create(shape, halide_type_of<float>());
    m_owned = true;
}

TensorWrapper::~TensorWrapper() {
    if (m_owned && m_tensor) {
        delete m_tensor;
    }
}

MNN::Tensor* TensorWrapper::GetInternalTensor() {
    return m_tensor;
}

Napi::Value TensorWrapper::GetShape(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_tensor) return env.Null();

    std::vector<int> shape = m_tensor->shape();
    Napi::Array arr = Napi::Array::New(env, shape.size());
    for(size_t i=0; i<shape.size(); ++i) {
        arr[i] = Napi::Number::New(env, shape[i]);
    }
    return arr;
}

Napi::Value TensorWrapper::GetData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_tensor) return env.Null();

    // Only support float for now safely
    if (m_tensor->getType() != halide_type_of<float>()) {
         Napi::Error::New(env, "Only float tensors are supported for GetData currently").ThrowAsJavaScriptException();
         return env.Null();
    }

    float* data = m_tensor->host<float>();
    size_t size = m_tensor->elementSize();

    if (!data) return env.Null();

    Napi::Float32Array arr = Napi::Float32Array::New(env, size);
    for(size_t i=0; i<size; ++i) {
        arr[i] = data[i];
    }
    return arr;
}

Napi::Value TensorWrapper::SetData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_tensor) return env.Null();
    if (info.Length() < 1) return env.Null();

    // Only support float for now safely
    if (m_tensor->getType() != halide_type_of<float>()) {
         Napi::Error::New(env, "Only float tensors are supported for SetData currently").ThrowAsJavaScriptException();
         return env.Null();
    }

    float* data = m_tensor->host<float>();
    if (!data) {
        Napi::Error::New(env, "Tensor host data is null").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (info[0].IsTypedArray()) {
         if (info[0].As<Napi::TypedArray>().TypedArrayType() == napi_float32_array) {
             Napi::Float32Array arr = info[0].As<Napi::Float32Array>();
             size_t size = m_tensor->elementSize();
             if (arr.ElementLength() != size) {
                 Napi::Error::New(env, "Data size mismatch").ThrowAsJavaScriptException();
                 return env.Null();
             }

             for(size_t i=0; i<size; ++i) {
                 data[i] = arr[i];
             }
         } else {
             Napi::TypeError::New(env, "Expected Float32Array").ThrowAsJavaScriptException();
         }
    } else if (info[0].IsArray()) {
        Napi::Array arr = info[0].As<Napi::Array>();
        size_t size = m_tensor->elementSize();
        if (arr.Length() != size) {
             Napi::Error::New(env, "Data size mismatch").ThrowAsJavaScriptException();
             return env.Null();
        }
        for(size_t i=0; i<size; ++i) {
            Napi::Value val = arr[i];
            if(val.IsNumber()) {
                data[i] = val.As<Napi::Number>().FloatValue();
            }
        }
    }
    return env.Null();
}

Napi::Value TensorWrapper::Print(const Napi::CallbackInfo& info) {
    if (m_tensor) m_tensor->print();
    return info.Env().Null();
}

Napi::Value TensorWrapper::CopyFrom(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_tensor) return env.Null();
    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected Tensor object").ThrowAsJavaScriptException();
        return env.Null();
    }

    TensorWrapper* srcTensorWrap = Napi::ObjectWrap<TensorWrapper>::Unwrap(info[0].As<Napi::Object>());
    MNN::Tensor* srcTensor = srcTensorWrap->GetInternalTensor();
    if (!srcTensor) return env.Null();

    m_tensor->copyFromHostTensor(srcTensor);
    return env.Null();
}

Napi::Value TensorWrapper::CopyTo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_tensor) return env.Null();
    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected Tensor object").ThrowAsJavaScriptException();
        return env.Null();
    }

    TensorWrapper* dstTensorWrap = Napi::ObjectWrap<TensorWrapper>::Unwrap(info[0].As<Napi::Object>());
    MNN::Tensor* dstTensor = dstTensorWrap->GetInternalTensor();
    if (!dstTensor) return env.Null();

    m_tensor->copyToHostTensor(dstTensor);
    return env.Null();
}

// ------------------- InterpreterWrapper -------------------

class SessionWrapper : public Napi::ObjectWrap<SessionWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::Object NewInstance(Napi::Env env, MNN::Session* session);
    SessionWrapper(const Napi::CallbackInfo& info);
    MNN::Session* GetInternalSession() { return m_session; }

private:
    static Napi::FunctionReference constructor;
    MNN::Session* m_session;
};

Napi::FunctionReference SessionWrapper::constructor;

Napi::Object SessionWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);
    Napi::Function func = DefineClass(env, "Session", {});
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    return exports;
}

Napi::Object SessionWrapper::NewInstance(Napi::Env env, MNN::Session* session) {
    Napi::EscapableHandleScope scope(env);
    Napi::Value external = Napi::External<MNN::Session>::New(env, session);
    Napi::Object object = constructor.New({ external });
    return scope.Escape(object).As<Napi::Object>();
}

SessionWrapper::SessionWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<SessionWrapper>(info) {
    if (info.Length() >= 1 && info[0].IsExternal()) {
        m_session = info[0].As<Napi::External<MNN::Session>>().Data();
    } else {
        m_session = nullptr;
    }
}


class InterpreterWrapper : public Napi::ObjectWrap<InterpreterWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    InterpreterWrapper(const Napi::CallbackInfo& info);
    ~InterpreterWrapper();

private:
    static Napi::FunctionReference constructor;
    MNN::Interpreter* m_interpreter;

    static Napi::Value CreateFromFile(const Napi::CallbackInfo& info);
    Napi::Value CreateSession(const Napi::CallbackInfo& info);
    Napi::Value ResizeSession(const Napi::CallbackInfo& info);
    Napi::Value RunSession(const Napi::CallbackInfo& info);
    Napi::Value GetSessionInput(const Napi::CallbackInfo& info);
    Napi::Value GetSessionOutput(const Napi::CallbackInfo& info);
};

Napi::FunctionReference InterpreterWrapper::constructor;

Napi::Object InterpreterWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function func = DefineClass(env, "Interpreter", {
        StaticMethod("createFromFile", &InterpreterWrapper::CreateFromFile),
        InstanceMethod("createSession", &InterpreterWrapper::CreateSession),
        InstanceMethod("resizeSession", &InterpreterWrapper::ResizeSession),
        InstanceMethod("runSession", &InterpreterWrapper::RunSession),
        InstanceMethod("getSessionInput", &InterpreterWrapper::GetSessionInput),
        InstanceMethod("getSessionOutput", &InterpreterWrapper::GetSessionOutput),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("Interpreter", func);
    return exports;
}

InterpreterWrapper::InterpreterWrapper(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<InterpreterWrapper>(info), m_interpreter(nullptr) {

    if (info.Length() >= 1 && info[0].IsExternal()) {
        m_interpreter = info[0].As<Napi::External<MNN::Interpreter>>().Data();
    } else {
        m_interpreter = nullptr;
    }
}

InterpreterWrapper::~InterpreterWrapper() {
    if (m_interpreter) {
        delete m_interpreter;
    }
}

Napi::Value InterpreterWrapper::CreateFromFile(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected string for model path").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string path = info[0].As<Napi::String>().Utf8Value();
    MNN::Interpreter* net = MNN::Interpreter::createFromFile(path.c_str());
    if (!net) {
        Napi::Error::New(env, "Failed to create interpreter from file").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Value external = Napi::External<MNN::Interpreter>::New(env, net);
    return constructor.New({ external });
}

Napi::Value InterpreterWrapper::CreateSession(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    MNN::ScheduleConfig config;
    config.type = MNN_FORWARD_CPU;
    config.numThread = 4;

    if (info.Length() >= 1 && info[0].IsObject()) {
        Napi::Object configObj = info[0].As<Napi::Object>();
        if (configObj.Has("numThread")) {
            config.numThread = configObj.Get("numThread").As<Napi::Number>().Int32Value();
        }
        if (configObj.Has("backend")) {
            // Mapping simple backend names/IDs if needed. For now assuming int ID.
            config.type = (MNNForwardType)configObj.Get("backend").As<Napi::Number>().Int32Value();
        }
        // Add more config options as needed
    }

    MNN::Session* session = m_interpreter->createSession(config);
    return SessionWrapper::NewInstance(env, session);
}

Napi::Value InterpreterWrapper::ResizeSession(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();

    SessionWrapper* sessionWrap = Napi::ObjectWrap<SessionWrapper>::Unwrap(info[0].As<Napi::Object>());
    m_interpreter->resizeSession(sessionWrap->GetInternalSession());
    return env.Null();
}

Napi::Value InterpreterWrapper::RunSession(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();

    SessionWrapper* sessionWrap = Napi::ObjectWrap<SessionWrapper>::Unwrap(info[0].As<Napi::Object>());
    MNN::ErrorCode code = m_interpreter->runSession(sessionWrap->GetInternalSession());
    return Napi::Number::New(env, code);
}

Napi::Value InterpreterWrapper::GetSessionInput(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();

    SessionWrapper* sessionWrap = Napi::ObjectWrap<SessionWrapper>::Unwrap(info[0].As<Napi::Object>());
    const char* name = nullptr;
    if (info.Length() > 1 && info[1].IsString()) {
        std::string nameStr = info[1].As<Napi::String>().Utf8Value();
        name = nameStr.c_str();
        MNN::Tensor* tensor = m_interpreter->getSessionInput(sessionWrap->GetInternalSession(), name);
        if (!tensor) return env.Null();
        return TensorWrapper::NewInstance(env, tensor, false);
    }

    MNN::Tensor* tensor = m_interpreter->getSessionInput(sessionWrap->GetInternalSession(), nullptr);
    if (!tensor) return env.Null();
    return TensorWrapper::NewInstance(env, tensor, false);
}

Napi::Value InterpreterWrapper::GetSessionOutput(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();

    SessionWrapper* sessionWrap = Napi::ObjectWrap<SessionWrapper>::Unwrap(info[0].As<Napi::Object>());
    const char* name = nullptr;
    std::string nameStr;
    if (info.Length() > 1 && info[1].IsString()) {
        nameStr = info[1].As<Napi::String>().Utf8Value();
        name = nameStr.c_str();
    }

    MNN::Tensor* tensor = m_interpreter->getSessionOutput(sessionWrap->GetInternalSession(), name);
    if (!tensor) return env.Null();
    return TensorWrapper::NewInstance(env, tensor, false);
}

// ... Main Init ...
Napi::String GetVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, MNN::getVersion());
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "version"), Napi::Function::New(env, GetVersion));
    TensorWrapper::Init(env, exports);
    SessionWrapper::Init(env, exports);
    InterpreterWrapper::Init(env, exports);
    return exports;
}

NODE_API_MODULE(mnn_node, Init)
