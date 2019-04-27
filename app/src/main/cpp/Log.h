#include <jni.h>
#include <android/log.h>

#define LOG_TAG "AntiDebug"
#define LOG_PRINT_D(fmt,args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
#define LOG_PRINT_I(fmt,args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args)
#define LOG_PRINT_W(fmt,args...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##args)
#define LOG_PRINT_E(fmt,args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##args)
#define LOG_PRINT_F(fmt,args...) __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, fmt, ##args)