#include "AntiDebug.h"
#include "Log.h"
#include <sys/ptrace.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string>
#include <cstring>


using namespace std;

extern jobject g_callbackRef;
extern jmethodID g_MethodCallback;

MACRO_HIDE_SYMBOL JavaVM* g_jvm = NULL;
MACRO_HIDE_SYMBOL bool g_bAttached = false;
MACRO_HIDE_SYMBOL jobject g_context = 0;

MACRO_HIDE_SYMBOL AntiDebug* AntiDebug::s_instance = NULL;
MACRO_HIDE_SYMBOL int AntiDebug::mAppFlags = 0;

MACRO_HIDE_SYMBOL JNIEnv *GetEnv()
{
    if(g_jvm == NULL)
        return NULL;

    int status;
    JNIEnv *env = NULL;
    status = g_jvm->GetEnv((void **)&env, JNI_VERSION_1_4);
    if(status < 0)
    {
        status = g_jvm->AttachCurrentThread(&env, NULL);
        if(status < 0)
        {
            return NULL;
        }
    }

    g_bAttached = true;
    return env;
}
 
MACRO_HIDE_SYMBOL void DetachCurrent()
{
    if(g_bAttached && g_jvm != NULL)
    {
        g_jvm->DetachCurrentThread();
    }
}

MACRO_HIDE_SYMBOL jobject getGlobalAppContext(JNIEnv *env)
{
    if(env == NULL)
        return NULL;

    if(g_context != NULL)
        return g_context;
    
    //获取Activity Thread的实例对象
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if(currentActivityThread == NULL)
        return NULL;
        
    jobject at = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    if(at == NULL)
        return NULL;

    //获取Application，也就是全局的Context
    jmethodID getApplication = env->GetMethodID(activityThread, "getApplication", "()Landroid/app/Application;");
    if(getApplication == NULL)
        return NULL;

    g_context = env->CallObjectMethod(at, getApplication);
    return g_context;
}

MACRO_HIDE_SYMBOL void string_replace( std::string &strBig, const std::string &strsrc, const std::string &strdst)
{
    string::size_type pos = 0;
    string::size_type srclen = strsrc.size();
    string::size_type dstlen = strdst.size();

    while( (pos=strBig.find(strsrc, pos)) != string::npos )
    {
        strBig.replace( pos, srclen, strdst );
        pos += dstlen;
    }
}

MACRO_HIDE_SYMBOL AntiDebug::AntiDebug(){
    mDebugGlobalRef = 0;
    mXPosedGlobalRef = 0;
    mExceptionGlobalRef = 0;
    mStackElementRef = 0;
}

//检测进程状态
MACRO_HIDE_SYMBOL bool AntiDebug::readStatus(){
    const int bufSize = 1024;
    char fileName[bufSize];
    char contentLine[bufSize];
    int ppid = 0;
    int pid = getpid();
    sprintf(fileName, "/proc/%d/status", pid);
    FILE* fd = fopen(fileName, "r");
    if (fd != NULL)
    {
        while (fgets(contentLine, bufSize, fd))
        {
            if (strncmp(contentLine, "PPid", 4) == 0)
            {
                ppid = atoi(&contentLine[5]);
            }
            
            if (strncmp(contentLine, "TracerPid", 9) == 0)
            {
                int statue = atoi(&contentLine[10]);
                if (statue != 0 && ppid != statue)
                {
                    LOG_PRINT_E("app be debug by ida or lldb.");
                    fclose(fd);
                    return true;
                }
                break;
            }
        }
        fclose(fd);
    }
    else
    {
        LOG_PRINT_E("status file open %s fail...", fileName);
    }

    return false;
}

//检测是否被xposed注入
MACRO_HIDE_SYMBOL bool AntiDebug::IsHookByXPosed(){
    char buf[1024] = {0};
    FILE *fp;
    int pid = getpid();
    sprintf(buf,"/proc/%d/maps",pid);
    fp = fopen(buf, "r");
    if(fp == NULL){
        LOG_PRINT_E("Error open maps file in progress %d",pid);
        return false;
    }

    if(mXPosedGlobalRef != 0){
        LOG_PRINT_E("app be injected by xposed or substrate.");
        return true;
    }

    while (fgets(buf,sizeof(buf),fp)){
        if(strstr(buf, "com.saurik.substrate") || strstr(buf, "io.va.exposed") || strstr(buf, "de.robv.android.xposed")){
            LOG_PRINT_E("app be injected by xposed or substrate.");
            fclose(fp);
            return true;
        }
    }
    fclose(fp);

    return false;
}

//分析java层堆栈，获取不到堆栈信息
MACRO_HIDE_SYMBOL bool AntiDebug::analyzeStackTrace(){
     JNIEnv* env = GetEnv();
     if(env == NULL || mExceptionGlobalRef == 0 || mStackElementRef == 0)
        return false;

	jmethodID  throwable_init = env->GetMethodID(mExceptionGlobalRef, "<init>", "(Ljava/lang/String;)V");
	jobject throwable_obj = env->NewObject(mExceptionGlobalRef, throwable_init, env->NewStringUTF("test"));

	jmethodID throwable_getStackTrace = env->GetMethodID(mExceptionGlobalRef, "getStackTrace", "()[Ljava/lang/StackTraceElement;");
	jobjectArray jStackElements = (jobjectArray)env->CallObjectMethod(throwable_obj, throwable_getStackTrace);
    
    jmethodID jMthGetClassName = env->GetMethodID(mStackElementRef, "getClassName", "()Ljava/lang/String;");
    int len = env->GetArrayLength(jStackElements);
    LOG_PRINT_E("jStackElements = %p, jMthGetClassName = %p, len = %d", jStackElements, jMthGetClassName, len);

    for(int i = 0; i < len; i++){
        jobject jStackElement = env->GetObjectArrayElement(jStackElements, i);
        jstring jClassName = (jstring)env->CallObjectMethod(jStackElement, jMthGetClassName);
        const char* szClassName = env->GetStringUTFChars(jClassName, 0);
        LOG_PRINT_I("szClassName = %s", szClassName);
    }

    return true;
}

//检测调试器状态
MACRO_HIDE_SYMBOL bool AntiDebug::isBeDebug(){
    if(g_context == NULL || mDebugGlobalRef == 0)
        return false;

    JNIEnv* env = GetEnv();
     if(env == NULL)
        return false;
    
    jclass jDebugClazz = env->FindClass("android/os/Debug");
    bool jDebug = ((mAppFlags & 2) != 0);
    jmethodID mthIsDebuggerConn = env->GetStaticMethodID(jDebugClazz, "isDebuggerConnected", "()Z");
    jboolean jIsDebuggerConnected = env->CallStaticBooleanMethod(jDebugClazz, mthIsDebuggerConn);

    //DetachCurrent();
    if(!jDebug && jIsDebuggerConnected){
        LOG_PRINT_E("app be debug in release mode jDebug = %d,jIsDebuggerConnected = %d", jDebug, jIsDebuggerConnected);
        return true;
    }
    
    return false;
}

//检测是否在虚拟机内运行
MACRO_HIDE_SYMBOL bool IsRunInVirtual(){
    return true;
}

//反调试检测
MACRO_HIDE_SYMBOL void* AntiDebug::antiDebugCallback(void *arg)
{
    if(arg == NULL)
        return NULL;

    AntiDebug* pAntiDebug = (AntiDebug*)arg;

    while (true)
    {
        try
        {
            bool bRet1 = pAntiDebug->readStatus();
            bool bRet2 = pAntiDebug->IsHookByXPosed();
            bool bRet3 = pAntiDebug->isBeDebug();
            if(bRet1 || bRet2 || bRet3){
                if(g_callbackRef != 0 && g_MethodCallback != 0){
                    JNIEnv* env = GetEnv();
                    if(env != NULL){
                        env->CallVoidMethod(g_callbackRef, g_MethodCallback);
                    }
                }
            }
        } catch (...)
        {

        }

        sleep(1);
    }
}

MACRO_HIDE_SYMBOL void AntiDebug::getGlobalRef()
{
    int status;
    JNIEnv *env = NULL;
    status = g_jvm->GetEnv((void **)&env, JNI_VERSION_1_4);
    if(status >= 0){
        getGlobalAppContext(env);
    }

    char* szPackageName = getPackageName(env);
    if(env == NULL || szPackageName == NULL)
        return ;

    string strPackageName = szPackageName;
    string_replace(strPackageName, ".", "/");

    try{
        char szClazzName[256] = {0};
        jclass jApplication = env->GetObjectClass(g_context);
        jmethodID jMthApplicationInfo = env->GetMethodID(jApplication, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
        if(jMthApplicationInfo != 0){
            jobject jAppinfo = env->CallObjectMethod(g_context, jMthApplicationInfo);
            jclass jClazAppInfo = env->GetObjectClass(jAppinfo);
            jfieldID jfieldFlags = env->GetFieldID(jClazAppInfo, "flags", "I");
            mAppFlags = env->GetIntField(jAppinfo, jfieldFlags);
            env->DeleteLocalRef(jClazAppInfo);
        }
         env->DeleteLocalRef(jApplication);

        memset(szClazzName, 0, 256);
        sprintf(szClazzName, "android/os/Debug");
        jclass jDebugClazz = env->FindClass(szClazzName);
        if(jDebugClazz != 0){
            mDebugGlobalRef = (jclass)env->NewGlobalRef(jDebugClazz);
        }


        memset(szClazzName, 0, 256);
        sprintf(szClazzName, "de/robv/android/xposed/XposedBridge");
        jclass jXPosedClazz = env->FindClass(szClazzName);
        if(env->ExceptionCheck()){
            env->ExceptionClear();
        }
        if(jXPosedClazz != 0)
        {
           mXPosedGlobalRef = (jclass)env->NewGlobalRef(jXPosedClazz);
        }
    }
    catch(...)
    {

    }
}

MACRO_HIDE_SYMBOL bool AntiDebug::isDebugMode()
{
    return (mAppFlags & 2) != 0;
}

MACRO_HIDE_SYMBOL char* AntiDebug::getPackageName(JNIEnv* env)
{
    if(env == NULL || g_context == NULL)
        return NULL;

    jclass context_class = env->GetObjectClass(g_context);

    //反射获取PackageManager
    jmethodID methodId = env->GetMethodID(context_class, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject package_manager = env->CallObjectMethod(g_context, methodId);
    if (package_manager == NULL) {
        LOG_PRINT_E("checkPackageName package_manager is NULL");
        return NULL;
    }

    //反射获取包名
    methodId = env->GetMethodID(context_class, "getPackageName", "()Ljava/lang/String;");
    jstring package_name = (jstring)env->CallObjectMethod(g_context, methodId);
    if (package_name == NULL) {
        LOG_PRINT_E("checkPackageName package_name is NULL");
        return NULL;
    }
    env->DeleteLocalRef(context_class);

    char* szPackageName = (char*)env->GetStringUTFChars(package_name, 0);
    return szPackageName;
}

MACRO_HIDE_SYMBOL void AntiDebug::antiDebugInner()
{
    getGlobalRef();
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    pthread_t ptid;
    pthread_create(&ptid, NULL, AntiDebug::antiDebugCallback, this);
}

MACRO_HIDE_SYMBOL void AntiDebug::antiDebug(JavaVM* jvm)
{
    g_jvm = jvm;
    if(s_instance == NULL){
        s_instance = new AntiDebug();
        s_instance->antiDebugInner();
    }
}

