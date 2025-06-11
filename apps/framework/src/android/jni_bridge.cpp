#include <jni.h>
#include "calculator.h" // Assuming calculator.h is accessible in include path

extern "C" JNIEXPORT jint JNICALL
Java_com_example_framework_NativeBridge_add(JNIEnv *env, jobject /* this */, jint a, jint b) {
    Calculator calculator;
    return calculator.add(a, b);
}
