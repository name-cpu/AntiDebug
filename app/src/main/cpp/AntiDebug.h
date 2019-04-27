#ifndef _ANTI_DEBUG_H
#define _ANTI_DEBUG_H
#include <jni.h>

class AntiDebug{
public:
    void antiDebug(JavaVM* jvm);
private:
    static void* antiDebugCallback(void *arg);
    void getGlobalRef();
    char* getPackageName(JNIEnv* env);
    bool readStatus();
    bool isBeDebug();
    bool IsHookByXPosed();
private:
    static jclass mBuildConfigGlobalRef;
    static jclass mDebugGlobalRef;
    static jclass mXPosedGlobalRef;
};
#endif