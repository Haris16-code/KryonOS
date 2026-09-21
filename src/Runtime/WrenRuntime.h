#ifndef WREN_RUNTIME_H
#define WREN_RUNTIME_H

#include <Arduino.h>
extern "C" {
    #include <wren.h>
}

class WrenRuntime {
public:
    static bool begin();
    static bool execute(const char* source, int lineOffset = 0 );
    static void update();
    static void shutdown();

private:
    static WrenVM* vm;
    static int apiLineCount;

    static void writeFn(WrenVM* vm, const char* text);
    static void errorFn(
        WrenVM* vm,
        WrenErrorType type,
        const char* module,
        int line,
        const char* msg
    );

    static WrenLoadModuleResult loadModule(
        WrenVM* vm,
        const char* name
    );

    static WrenForeignMethodFn bindForeignMethod(
        WrenVM* vm,
        const char* module,
        const char* className,
        bool isStatic,
        const char* signature
    );

    static WrenForeignClassMethods bindForeignClass(
        WrenVM* vm,
        const char* module,
        const char* className
    );
};

#endif