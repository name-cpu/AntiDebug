#include "AntiDebug.h"
#include "Log.h"
#include <sys/ptrace.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
//#include <cstring>
#include <string>
#include <cstring>


using namespace std;

JavaVM* g_jvm = NULL; 
jobject g_context = NULL;
bool g_bAttached = false;

extern jobject g_callbackRef;
extern jmethodID g_MethodCallback;

jclass AntiDebug::mBuildConfigGlobalRef = 0;
jclass AntiDebug::mDebugGlobalRef = 0;
jclass AntiDebug::mXPosedGlobalRef = 0;

JNIEnv *GetEnv()
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
 
void DetachCurrent()
{
    if(g_bAttached && g_jvm != NULL)
    {
        g_jvm->DetachCurrentThread();
    }
}

jobject getGlobalAppContext(JNIEnv *env)
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

void string_replace( std::string &strBig, const string &strsrc, const std::string &strdst)
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

//检测进程状态
bool AntiDebug::readStatus(){
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
            string string1;
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
        LOG_PRINT_E("antiDebugCheck open %s fail...", fileName);
    }

    return false;
}

//检测是否被xposed注入
bool AntiDebug::IsHookByXPosed(){
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
        string temp = buf;

        if(temp.find("com.saurik.substrate") != string::npos
           || temp.find("io.va.exposed") != string::npos
           || temp.find("de.robv.android.xposed") != string::npos){
            LOG_PRINT_E("app be injected by xposed or substrate.");
            fclose(fp);
            return true;
        }
    }
    fclose(fp);

    return false;
}

//检测调试器状态
bool AntiDebug::isBeDebug(){
    if(g_context == NULL || mDebugGlobalRef == 0 || mBuildConfigGlobalRef == 0)
        return false;

    JNIEnv* env = GetEnv();
     if(env == NULL)
        return false;
    
    jfieldID debugField = env->GetStaticFieldID(mBuildConfigGlobalRef, "DEBUG", "Z");
    jboolean jDebug = env->GetStaticBooleanField(mBuildConfigGlobalRef, debugField);

    jmethodID mthIsDebuggerConn = env->GetStaticMethodID(mDebugGlobalRef, "isDebuggerConnected", "()Z");
    jboolean jIsDebuggerConnected = env->CallBooleanMethod(mDebugGlobalRef, mthIsDebuggerConn);

    //DetachCurrent();
    if(!jDebug && jIsDebuggerConnected){
        LOG_PRINT_E("app be debug in release mode.");
        return true;
    }
    
    return false;
}


//反调试检测
void* AntiDebug::antiDebugCallback(void *arg)
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

void AntiDebug::getGlobalRef()
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
        sprintf(szClazzName, "%s/BuildConfig", strPackageName.c_str());
        jclass clazz = env->FindClass(szClazzName);
        mBuildConfigGlobalRef = (jclass)env->NewGlobalRef(clazz);

        memset(szClazzName, 0, 256);
        sprintf(szClazzName, "android/os/Debug");
        jclass jDebugClazz = env->FindClass(szClazzName);
        mDebugGlobalRef = (jclass)env->NewGlobalRef(jDebugClazz);

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

char* AntiDebug::getPackageName(JNIEnv* env)
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

void AntiDebug::antiDebug(JavaVM* jvm)
{
    g_jvm = jvm;
    getGlobalRef();
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    pthread_t ptid;
    pthread_create(&ptid, NULL, AntiDebug::antiDebugCallback, this);
    //pthread_join(ptid, NULL);
}

