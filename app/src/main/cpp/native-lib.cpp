#include <jni.h>
#include <string>
#include "AntiDebug.h"

jobject g_callbackRef;
jmethodID  g_MethodCallback;

extern "C" JNIEXPORT void JNICALL
Java_com_android_antidebug_AntiDebug_setAntiDebugCallback(
        JNIEnv* env,
        jobject /* this */, jobject jCallback) {
    jclass jclazz = env->GetObjectClass(jCallback);
    g_callbackRef = env->NewGlobalRef(jCallback);
    g_MethodCallback = env->GetMethodID(jclazz, "beInjectedDebug", "()V");
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved){
    AntiDebug antiDebug;
    antiDebug.antiDebug(vm);
    return JNI_VERSION_1_4; //这里很重要，必须返回版本，否则加载会失败。
}

