#include <jni.h>
#include <string>
#include "AntiDebug.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_android_antidebug_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {

    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved){
    AntiDebug antiDebug;
    antiDebug.antiDebug(vm);
    return JNI_VERSION_1_4; //这里很重要，必须返回版本，否则加载会失败。
}
