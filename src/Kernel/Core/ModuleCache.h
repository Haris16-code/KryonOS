#ifndef MODULE_CACHE_H
#define MODULE_CACHE_H

#include <Arduino.h>
#include <lua.h>
#include <vector>
#include "../../File System/FileSystem.h"

class ModuleCache {
public:
    using ProgressCallback = void(*)(const char* action,
                                     const char* name,
                                     int current, int total);

private:
    struct CacheEntry {
        String moduleName;
        String luaPath;
        String luacPath;
        bool loaded;
    };

    // Agora ProgressCallback já é conhecido
    static ProgressCallback _progressCb;
    static int _progressCurrent;
    static int _progressTotal;

    static std::vector<CacheEntry> _cache;
    static String _appDirectory;

public:
    static void setAppDirectory(const String& dir) { _appDirectory = dir; }
    static String getAppDirectory() { return _appDirectory; }

    // Geração de caminhos
    static String getBinPath(const String& luaPath);
    static String getMetaPath(const String& luaPath);
    static String getLuacPath(const String& luaPath);
    static String resolveModulePath(lua_State* L, const char* moduleName);

    // Verificação e compilação
    static bool needsRecompile(const String& luaPath, const String& luacPath);
    static bool compileToLuac(lua_State* L, const String& luaPath, const String& luacPath);
    static bool loadLuac(lua_State* L, const String& luacPath);

    // Pré-compilação
    static void precompileDirectory(lua_State* L, const String& dirPath);

    // Progresso
    static void setProgressCallback(ProgressCallback cb);
    static void reportProgress(const char* action, const char* name);
    static void setProgressTotal(int total);
};

#endif