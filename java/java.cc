#include "elf.hh"
#include <jni.h>
#include <string.h>

extern elf::program* prog;

#define JVM_PATH        "/usr/lib/jvm/jre/lib/amd64/server/libjvm.so"

JavaVMOption mkoption(const char* s)
{
    JavaVMOption opt;
    opt.optionString = strdup(s);
    return opt;
}

extern "C" int osv_main(int ac, char **av)
{
    prog->add_object(JVM_PATH);

    auto JNI_GetDefaultJavaVMInitArgs
        = prog->lookup_function<void (void*)>("JNI_GetDefaultJavaVMInitArgs");
    JavaVMInitArgs vm_args = {};
    vm_args.version = JNI_VERSION_1_6;
    JNI_GetDefaultJavaVMInitArgs(&vm_args);
    std::string jarfile;
    std::vector<JavaVMOption> options;
    options.push_back(mkoption("-Djava.class.path=/java"));
    while (ac > 0 && av[0][0] == '-') {
        if (std::string(av[0]) == "-jar") {
            ++av, --ac;
            jarfile = av[0];
        } else {
            options.push_back(mkoption(av[0]));
        }
        ++av, --ac;
    }
    vm_args.nOptions = options.size();
    vm_args.options = options.data();

    auto JNI_CreateJavaVM
        = prog->lookup_function<jint (JavaVM**, JNIEnv**, void*)>("JNI_CreateJavaVM");
    JavaVM* jvm = nullptr;
    JNIEnv *env;

    auto ret = JNI_CreateJavaVM(&jvm, &env, &vm_args);
    assert(ret == 0);
    std::string mainclassname;
    if (jarfile.empty()) {
        mainclassname = av[0];
        ++av, --ac;
    } else {
        mainclassname = "RunJar";
    }
    auto mainclass = env->FindClass(mainclassname.c_str());

    auto mainmethod = env->GetStaticMethodID(mainclass, "main", "([Ljava/lang/String;)V");
    auto stringclass = env->FindClass("java/lang/String");
    std::vector<std::string> newargs;
    if (!jarfile.empty()) {
        newargs.push_back(jarfile);
    }
    for (auto i = 0; i < ac; ++av) {
        newargs.push_back(av[i]);
    }

    auto args = env->NewObjectArray(newargs.size(), stringclass, nullptr);
    for (auto i = 0u; i < newargs.size(); ++i) {
        env->SetObjectArrayElement(args, i, env->NewStringUTF(newargs[i].c_str()));
    }
    env->CallStaticVoidMethod(mainclass, mainmethod, args);
    return 0;
}

