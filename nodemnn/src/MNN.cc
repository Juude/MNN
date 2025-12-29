#include <napi.h>
#include <MNN/MNNDefine.h>
#include <MNN/Interpreter.hpp>
#include <MNN/Tensor.hpp>
#include <MNN/ImageProcess.hpp>
#include <MNN/Matrix.h>
#include <vector>
#include <iostream>

using namespace MNN;

// ------------------- Helper Functions -------------------

// ------------------- TensorWrapper -------------------

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
    Napi::Value GetDataType(const Napi::CallbackInfo& info);
    Napi::Value GetDimensionType(const Napi::CallbackInfo& info);

    friend class InterpreterWrapper;
    friend class ImageProcessWrapper;
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
        InstanceMethod("getDataType", &TensorWrapper::GetDataType),
        InstanceMethod("getDimensionType", &TensorWrapper::GetDimensionType),
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

    halide_type_t type = halide_type_of<float>();
    if (info.Length() >= 2 && info[1].IsObject()) {
        // Using "halide_type_t" object passed from JS (dummy object or int)
        // For simplicity, let's accept int constants for now if we export them
        if (info[1].IsNumber()) {
            int typeId = info[1].As<Napi::Number>().Int32Value();
            // 0: int, 1: float, 2: uint8, 3: int64 ? Need to match constants exported
            // Easier to rely on user passing type name or similar?
            // Let's stick to default float unless specific args
        }
    }
    // Better: accept type as string or enum
    if (info.Length() >= 2 && info[1].IsNumber()) {
        int t = info[1].As<Napi::Number>().Int32Value();
        if (t == 0) type = halide_type_of<int32_t>();
        else if (t == 2) type = halide_type_of<uint8_t>();
        // else default float
    }

    m_tensor = MNN::Tensor::create(shape, type);
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

    size_t size = m_tensor->elementSize();
    auto type = m_tensor->getType();

    if (type == halide_type_of<float>()) {
        float* data = m_tensor->host<float>();
        if (!data) return env.Null();
        Napi::Float32Array arr = Napi::Float32Array::New(env, size);
        memcpy(arr.Data(), data, size * sizeof(float));
        return arr;
    } else if (type == halide_type_of<int32_t>()) {
        int32_t* data = m_tensor->host<int32_t>();
        if (!data) return env.Null();
        Napi::Int32Array arr = Napi::Int32Array::New(env, size);
        memcpy(arr.Data(), data, size * sizeof(int32_t));
        return arr;
    } else if (type == halide_type_of<uint8_t>()) {
        uint8_t* data = m_tensor->host<uint8_t>();
        if (!data) return env.Null();
        Napi::Uint8Array arr = Napi::Uint8Array::New(env, size);
        memcpy(arr.Data(), data, size * sizeof(uint8_t));
        return arr;
    }

    Napi::Error::New(env, "Unsupported tensor type for GetData").ThrowAsJavaScriptException();
    return env.Null();
}

Napi::Value TensorWrapper::SetData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_tensor) return env.Null();
    if (info.Length() < 1) return env.Null();

    size_t size = m_tensor->elementSize();
    auto type = m_tensor->getType();

    if (type == halide_type_of<float>()) {
        float* data = m_tensor->host<float>();
        if (!data) {
             Napi::Error::New(env, "Tensor host data is null").ThrowAsJavaScriptException();
             return env.Null();
        }
        if (info[0].IsTypedArray() && info[0].As<Napi::TypedArray>().TypedArrayType() == napi_float32_array) {
            Napi::Float32Array arr = info[0].As<Napi::Float32Array>();
            if (arr.ElementLength() != size) {
                Napi::Error::New(env, "Data size mismatch").ThrowAsJavaScriptException();
                return env.Null();
            }
            memcpy(data, arr.Data(), size * sizeof(float));
        } else if (info[0].IsArray()) {
            Napi::Array arr = info[0].As<Napi::Array>();
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
        } else {
             Napi::TypeError::New(env, "Expected Float32Array or Array").ThrowAsJavaScriptException();
        }
    } else if (type == halide_type_of<int32_t>()) {
        int32_t* data = m_tensor->host<int32_t>();
        if (!data) {
             Napi::Error::New(env, "Tensor host data is null").ThrowAsJavaScriptException();
             return env.Null();
        }
        if (info[0].IsTypedArray() && info[0].As<Napi::TypedArray>().TypedArrayType() == napi_int32_array) {
            Napi::Int32Array arr = info[0].As<Napi::Int32Array>();
            if (arr.ElementLength() != size) {
                Napi::Error::New(env, "Data size mismatch").ThrowAsJavaScriptException();
                return env.Null();
            }
            memcpy(data, arr.Data(), size * sizeof(int32_t));
        } else {
             Napi::TypeError::New(env, "Expected Int32Array").ThrowAsJavaScriptException();
        }
    } else if (type == halide_type_of<uint8_t>()) {
        uint8_t* data = m_tensor->host<uint8_t>();
        if (!data) {
             Napi::Error::New(env, "Tensor host data is null").ThrowAsJavaScriptException();
             return env.Null();
        }
        if (info[0].IsTypedArray() && info[0].As<Napi::TypedArray>().TypedArrayType() == napi_uint8_array) {
            Napi::Uint8Array arr = info[0].As<Napi::Uint8Array>();
            if (arr.ElementLength() != size) {
                Napi::Error::New(env, "Data size mismatch").ThrowAsJavaScriptException();
                return env.Null();
            }
            memcpy(data, arr.Data(), size * sizeof(uint8_t));
        } else {
             Napi::TypeError::New(env, "Expected Uint8Array").ThrowAsJavaScriptException();
        }
    } else {
        Napi::Error::New(env, "Unsupported tensor type for SetData").ThrowAsJavaScriptException();
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

Napi::Value TensorWrapper::GetDataType(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_tensor) return env.Null();
    auto type = m_tensor->getType();
    if (type == halide_type_of<int32_t>()) return Napi::Number::New(env, 0); // Int
    if (type == halide_type_of<float>()) return Napi::Number::New(env, 1); // Float
    if (type == halide_type_of<uint8_t>()) return Napi::Number::New(env, 2); // Uint8
    return Napi::Number::New(env, -1);
}

Napi::Value TensorWrapper::GetDimensionType(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_tensor) return env.Null();
    return Napi::Number::New(env, m_tensor->getDimensionType());
}

// ------------------- SessionWrapper -------------------

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

// ------------------- InterpreterWrapper -------------------

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
    Napi::Value SetSessionMode(const Napi::CallbackInfo& info);
    Napi::Value SetSessionHint(const Napi::CallbackInfo& info);
    Napi::Value GetSessionInfo(const Napi::CallbackInfo& info);
    Napi::Value GetModelVersion(const Napi::CallbackInfo& info);
    Napi::Value SetCacheFile(const Napi::CallbackInfo& info);
    Napi::Value SetExternalFile(const Napi::CallbackInfo& info);
    Napi::Value UpdateCacheFile(const Napi::CallbackInfo& info);
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
        InstanceMethod("setSessionMode", &InterpreterWrapper::SetSessionMode),
        InstanceMethod("setSessionHint", &InterpreterWrapper::SetSessionHint),
        InstanceMethod("getSessionInfo", &InterpreterWrapper::GetSessionInfo),
        InstanceMethod("getModelVersion", &InterpreterWrapper::GetModelVersion),
        InstanceMethod("setCacheFile", &InterpreterWrapper::SetCacheFile),
        InstanceMethod("setExternalFile", &InterpreterWrapper::SetExternalFile),
        InstanceMethod("updateCacheFile", &InterpreterWrapper::UpdateCacheFile),
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
            config.type = (MNNForwardType)configObj.Get("backend").As<Napi::Number>().Int32Value();
        }
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

Napi::Value InterpreterWrapper::SetSessionMode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsNumber()) return env.Null();
    int mode = info[0].As<Napi::Number>().Int32Value();
    m_interpreter->setSessionMode((MNN::Interpreter::SessionMode)mode);
    return env.Null();
}

Napi::Value InterpreterWrapper::SetSessionHint(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) return env.Null();
    int mode = info[0].As<Napi::Number>().Int32Value();
    int hint = info[1].As<Napi::Number>().Int32Value();
    m_interpreter->setSessionHint((MNN::Interpreter::HintMode)mode, hint);
    return env.Null();
}

Napi::Value InterpreterWrapper::GetSessionInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return env.Null();
    SessionWrapper* sessionWrap = Napi::ObjectWrap<SessionWrapper>::Unwrap(info[0].As<Napi::Object>());
    int code = info[1].As<Napi::Number>().Int32Value();

    if (code == MNN::Interpreter::BACKENDS) {
        int backendType[2];
        m_interpreter->getSessionInfo(sessionWrap->GetInternalSession(), (MNN::Interpreter::SessionInfoCode)code, backendType);
        return Napi::Number::New(env, backendType[0]);
    }
    float result;
    m_interpreter->getSessionInfo(sessionWrap->GetInternalSession(), (MNN::Interpreter::SessionInfoCode)code, &result);
    return Napi::Number::New(env, result);
}

Napi::Value InterpreterWrapper::GetModelVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, m_interpreter->getModelVersion());
}

Napi::Value InterpreterWrapper::SetCacheFile(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) return env.Null();
    std::string path = info[0].As<Napi::String>().Utf8Value();
    m_interpreter->setCacheFile(path.c_str());
    return env.Null();
}

Napi::Value InterpreterWrapper::SetExternalFile(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) return env.Null();
    std::string path = info[0].As<Napi::String>().Utf8Value();
    m_interpreter->setExternalFile(path.c_str());
    return env.Null();
}

Napi::Value InterpreterWrapper::UpdateCacheFile(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();
    SessionWrapper* sessionWrap = Napi::ObjectWrap<SessionWrapper>::Unwrap(info[0].As<Napi::Object>());
    int flag = 0;
    if (info.Length() > 1 && info[1].IsNumber()) flag = info[1].As<Napi::Number>().Int32Value();
    return Napi::Number::New(env, m_interpreter->updateCacheFile(sessionWrap->GetInternalSession(), flag));
}

// ------------------- CVMatrixWrapper -------------------

class CVMatrixWrapper : public Napi::ObjectWrap<CVMatrixWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    CVMatrixWrapper(const Napi::CallbackInfo& info);
    ~CVMatrixWrapper();
    CV::Matrix* GetInternalMatrix() { return m_matrix; }

private:
    static Napi::FunctionReference constructor;
    CV::Matrix* m_matrix;

    Napi::Value SetScale(const Napi::CallbackInfo& info);
    Napi::Value SetTranslate(const Napi::CallbackInfo& info);
    Napi::Value SetRotate(const Napi::CallbackInfo& info);
    Napi::Value Invert(const Napi::CallbackInfo& info);
    Napi::Value Write(const Napi::CallbackInfo& info);
    Napi::Value Read(const Napi::CallbackInfo& info);
};

Napi::FunctionReference CVMatrixWrapper::constructor;

Napi::Object CVMatrixWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);
    Napi::Function func = DefineClass(env, "CVMatrix", {
        InstanceMethod("setScale", &CVMatrixWrapper::SetScale),
        InstanceMethod("setTranslate", &CVMatrixWrapper::SetTranslate),
        InstanceMethod("setRotate", &CVMatrixWrapper::SetRotate),
        InstanceMethod("invert", &CVMatrixWrapper::Invert),
        InstanceMethod("write", &CVMatrixWrapper::Write),
        InstanceMethod("read", &CVMatrixWrapper::Read),
    });
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("CVMatrix", func);
    return exports;
}

CVMatrixWrapper::CVMatrixWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<CVMatrixWrapper>(info) {
    m_matrix = new CV::Matrix();
}

CVMatrixWrapper::~CVMatrixWrapper() {
    delete m_matrix;
}

Napi::Value CVMatrixWrapper::SetScale(const Napi::CallbackInfo& info) {
    if (info.Length() >= 2 && info[0].IsNumber() && info[1].IsNumber()) {
        float sx = info[0].As<Napi::Number>().FloatValue();
        float sy = info[1].As<Napi::Number>().FloatValue();
        m_matrix->setScale(sx, sy);
    }
    return info.Env().Null();
}

Napi::Value CVMatrixWrapper::SetTranslate(const Napi::CallbackInfo& info) {
    if (info.Length() >= 2 && info[0].IsNumber() && info[1].IsNumber()) {
        float tx = info[0].As<Napi::Number>().FloatValue();
        float ty = info[1].As<Napi::Number>().FloatValue();
        m_matrix->setTranslate(tx, ty);
    }
    return info.Env().Null();
}

Napi::Value CVMatrixWrapper::SetRotate(const Napi::CallbackInfo& info) {
    if (info.Length() >= 1 && info[0].IsNumber()) {
        float angle = info[0].As<Napi::Number>().FloatValue();
        m_matrix->setRotate(angle);
    }
    return info.Env().Null();
}

Napi::Value CVMatrixWrapper::Invert(const Napi::CallbackInfo& info) {
    m_matrix->invert(m_matrix);
    return info.Env().Null();
}

Napi::Value CVMatrixWrapper::Write(const Napi::CallbackInfo& info) {
    if (info.Length() >= 1 && info[0].IsArray()) {
        Napi::Array arr = info[0].As<Napi::Array>();
        for (uint32_t i = 0; i < 9 && i < arr.Length(); i++) {
            Napi::Value v = arr[i];
            if (v.IsNumber()) {
                m_matrix->set(i, v.As<Napi::Number>().FloatValue());
            }
        }
    }
    return info.Env().Null();
}

Napi::Value CVMatrixWrapper::Read(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Array arr = Napi::Array::New(env, 9);
    float buf[9];
    m_matrix->get9(buf);
    for (int i = 0; i < 9; i++) {
        arr[i] = Napi::Number::New(env, buf[i]);
    }
    return arr;
}

// ------------------- CVImageProcessWrapper -------------------

class CVImageProcessWrapper : public Napi::ObjectWrap<CVImageProcessWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    CVImageProcessWrapper(const Napi::CallbackInfo& info);
    ~CVImageProcessWrapper();

private:
    static Napi::FunctionReference constructor;
    CV::ImageProcess* m_imageProcess;

    Napi::Value SetMatrix(const Napi::CallbackInfo& info);
    Napi::Value Convert(const Napi::CallbackInfo& info);
    Napi::Value SetPadding(const Napi::CallbackInfo& info);
};

Napi::FunctionReference CVImageProcessWrapper::constructor;

Napi::Object CVImageProcessWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);
    Napi::Function func = DefineClass(env, "CVImageProcess", {
        InstanceMethod("setMatrix", &CVImageProcessWrapper::SetMatrix),
        InstanceMethod("convert", &CVImageProcessWrapper::Convert),
        InstanceMethod("setPadding", &CVImageProcessWrapper::SetPadding),
    });
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("CVImageProcess", func);
    return exports;
}

CVImageProcessWrapper::CVImageProcessWrapper(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<CVImageProcessWrapper>(info), m_imageProcess(nullptr) {

    Napi::Env env = info.Env();
    if (info.Length() >= 1 && info[0].IsObject()) {
        Napi::Object configObj = info[0].As<Napi::Object>();
        CV::ImageProcess::Config config;

        if (configObj.Has("filterType")) config.filterType = (CV::Filter)configObj.Get("filterType").As<Napi::Number>().Int32Value();
        if (configObj.Has("sourceFormat")) config.sourceFormat = (CV::ImageFormat)configObj.Get("sourceFormat").As<Napi::Number>().Int32Value();
        if (configObj.Has("destFormat")) config.destFormat = (CV::ImageFormat)configObj.Get("destFormat").As<Napi::Number>().Int32Value();
        if (configObj.Has("wrap")) config.wrap = (CV::Wrap)configObj.Get("wrap").As<Napi::Number>().Int32Value();

        if (configObj.Has("mean") && configObj.Get("mean").IsArray()) {
            Napi::Array mean = configObj.Get("mean").As<Napi::Array>();
            for(int i=0; i<4 && i<(int)mean.Length(); i++) {
                Napi::Value v = mean[i];
                config.mean[i] = v.As<Napi::Number>().FloatValue();
            }
        }
        if (configObj.Has("normal") && configObj.Get("normal").IsArray()) {
            Napi::Array normal = configObj.Get("normal").As<Napi::Array>();
            for(int i=0; i<4 && i<(int)normal.Length(); i++) {
                Napi::Value v = normal[i];
                config.normal[i] = v.As<Napi::Number>().FloatValue();
            }
        }

        m_imageProcess = CV::ImageProcess::create(config);
    } else {
        // Default config
        CV::ImageProcess::Config config;
        m_imageProcess = CV::ImageProcess::create(config);
    }
}

CVImageProcessWrapper::~CVImageProcessWrapper() {
    if (m_imageProcess) delete m_imageProcess;
}

Napi::Value CVImageProcessWrapper::SetMatrix(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsObject()) return env.Null();
    CVMatrixWrapper* matrixWrap = Napi::ObjectWrap<CVMatrixWrapper>::Unwrap(info[0].As<Napi::Object>());
    m_imageProcess->setMatrix(*matrixWrap->GetInternalMatrix());
    return env.Null();
}

Napi::Value CVImageProcessWrapper::Convert(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    // args: sourceData (Uint8Array), width, height, stride, destTensor
    if (info.Length() < 5) return env.Null();

    if (!info[0].IsTypedArray()) {
        Napi::TypeError::New(env, "Expected Uint8Array for source").ThrowAsJavaScriptException();
        return env.Null();
    }
    Napi::Uint8Array sourceData = info[0].As<Napi::Uint8Array>();
    int w = info[1].As<Napi::Number>().Int32Value();
    int h = info[2].As<Napi::Number>().Int32Value();
    int stride = info[3].As<Napi::Number>().Int32Value();

    TensorWrapper* destTensorWrap = Napi::ObjectWrap<TensorWrapper>::Unwrap(info[4].As<Napi::Object>());
    MNN::Tensor* destTensor = destTensorWrap->GetInternalTensor();

    m_imageProcess->convert(sourceData.Data(), w, h, stride, destTensor);
    return env.Null();
}

Napi::Value CVImageProcessWrapper::SetPadding(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() >= 1 && info[0].IsNumber()) {
        int padding = info[0].As<Napi::Number>().Int32Value();
        m_imageProcess->setPadding((uint8_t)padding);
    }
    return env.Null();
}

// ... Main Init ...
Napi::String GetVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, MNN::getVersion());
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "version"), Napi::Function::New(env, GetVersion));

    // Constants
    // MNNForwardType
    exports.Set("MNN_FORWARD_CPU", Napi::Number::New(env, MNN_FORWARD_CPU));
    exports.Set("MNN_FORWARD_OPENCL", Napi::Number::New(env, MNN_FORWARD_OPENCL));
    exports.Set("MNN_FORWARD_OPENGL", Napi::Number::New(env, MNN_FORWARD_OPENGL));
    exports.Set("MNN_FORWARD_VULKAN", Napi::Number::New(env, MNN_FORWARD_VULKAN));
    exports.Set("MNN_FORWARD_CUDA", Napi::Number::New(env, MNN_FORWARD_CUDA));

    // Tensor Types
    exports.Set("Halide_Type_Int", Napi::Number::New(env, 0));
    exports.Set("Halide_Type_Float", Napi::Number::New(env, 1));
    exports.Set("Halide_Type_Uint8", Napi::Number::New(env, 2));

    // ImageFormat
    exports.Set("CV_ImageFormat_RGBA", Napi::Number::New(env, CV::RGBA));
    exports.Set("CV_ImageFormat_RGB", Napi::Number::New(env, CV::RGB));
    exports.Set("CV_ImageFormat_BGR", Napi::Number::New(env, CV::BGR));
    exports.Set("CV_ImageFormat_GRAY", Napi::Number::New(env, CV::GRAY));
    exports.Set("CV_ImageFormat_BGRA", Napi::Number::New(env, CV::BGRA));
    exports.Set("CV_ImageFormat_YUV_NV21", Napi::Number::New(env, CV::YUV_NV21));

    // Filter
    exports.Set("CV_Filter_NEAREST", Napi::Number::New(env, CV::NEAREST));
    exports.Set("CV_Filter_BILINEAL", Napi::Number::New(env, CV::BILINEAR));
    exports.Set("CV_Filter_BICUBIC", Napi::Number::New(env, CV::BICUBIC));

    // Wrap
    exports.Set("CV_Wrap_CLAMP_TO_EDGE", Napi::Number::New(env, CV::CLAMP_TO_EDGE));
    exports.Set("CV_Wrap_ZERO", Napi::Number::New(env, CV::ZERO));
    exports.Set("CV_Wrap_REPEAT", Napi::Number::New(env, CV::REPEAT));

    TensorWrapper::Init(env, exports);
    SessionWrapper::Init(env, exports);
    InterpreterWrapper::Init(env, exports);
    CVMatrixWrapper::Init(env, exports);
    CVImageProcessWrapper::Init(env, exports);

    return exports;
}

NODE_API_MODULE(mnn_node, Init)
