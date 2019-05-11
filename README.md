# AntiDebug
android apk反调试工具，反hook、反xposed、反virtual xposed、反substrate

该项目主要实现android app反第三方调试功能，主要功能用c++实现，因为考虑到用java实现会被xposed等功能拦截，导致反调试功能失效。

主要的思路和步骤如下：
native层主要工作：
1.检测进程status文件TracerPid字段状态，如果该状态不为0，表示native层代码被调试
2.检测进程的maps文件，app进程中加载的所有模块信息地址均会写入该文件中。我们分析是否有com.saurik.substrate/io.va.exposed/de.robv.android.xposed等一系列敏感模块信息来判断是否被反调试
3.检测进程的调试器状态，首先检测app是否处于Debug模式，然后检测java层调试器是否打开。如果处于Release模式并且调试器打开，说明被反调试

java层主要工作：
在关键业务的地方调用AntiDeubg.isInject()方法，判断进程的堆栈是否包含两次com.android.internal.os.ZygoteInit信息。正常情况只会调用一次，如果app被xposed注入，被调用两次。


使用方法：
在Activity中调用次方法注册监听即可，AntiDebug.setAntiDebugCallback(this);
Native层会实时监测进程状态，如果监测到被反调试会回调到beInjectedDebug方法中，详情见Demo。

如有问题欢迎提Issues。
