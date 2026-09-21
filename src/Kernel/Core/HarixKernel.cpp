#include "ModuleCache.h"
#include "HarixKernel.h"
#include "Runtime/WrenAPI.h"
#include "Runtime/JSBindings.h"
#include "Runtime/LuaBindings.h"
#include "Runtime/WrenBindings.h"
#include "Runtime/WrenRuntime.h"
#include "Kernel/Core/EngineTaskRunner.h"
#include "../../File System/FileSystem.h"
#include "LoadingScreen.h"

lua_State *HarixKernel::L = nullptr;
duk_context *HarixKernel::ctx = nullptr;

// Função auxiliar de alocação com fallback para a memória interna
static void* psram_or_internal_malloc(size_t size) {
    if (size == 0) return nullptr;
    
    void* p = nullptr;
    if (psramFound()) {
        p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!p) { // Se a PSRAM não existir ou estiver cheia, usa a RAM interna
        p = malloc(size);
    }
    return p;
}

// Função auxiliar de realocação com fallback para a memória interna
static void* psram_or_internal_realloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return nullptr;
    }
    if (!ptr) {
        return psram_or_internal_malloc(size);
    }

    void* p = nullptr;
    if (psramFound()) {
        p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!p) { // Se a PSRAM falhar, tenta realocar na RAM interna
        p = realloc(ptr, size);
    }
    return p;
}

// Helper: conta arquivos .lua recursivamente, ignorando bin/ e meta/
static int countLuaFilesRecursive(const String& dir)
{
    int count = 0;
    const int kMaxFiles = 200;
    String* files = new String[kMaxFiles];
    if (!files) return 0;

    int n = FileSystem::listDir(dir.c_str(), files, kMaxFiles);
    if (n < 0) n = 0;

    for (int i = 0; i < n; i++) {
        String name = files[i];
        String full = dir;
        if (!full.endsWith("/")) full += "/";
        full += name;

        if (FileSystem::isDirectory(full.c_str())) {
            // Pula diretórios de saída
            if (name == "bin" || name == "meta") continue;
            count += countLuaFilesRecursive(full);
        } else if (name.endsWith(".lua")) {
            count++;
        }
    }

    delete[] files;
    return count;
}

// ============================================================
// Helpers de UI para erros
// ============================================================

// Desenha tela de OOM (Out Of Ram) e espera toque/tecla.
// Reutilizado por checkLuaError, checkJSError, my_lua_panic,
// my_fatal e showOutOfRamError.
static void showOomScreen(const char* engineName)
{
    Serial.printf("[%s] Out of RAM\n", engineName ? engineName : "?");

    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Out Of Ram Error", 10, 20, 4);
    tft.drawString("Please turn off WiFi in", 10, 60, 2);
    tft.drawString("setting to free the ram", 10, 80, 2);
    tft.drawString("and make this app running", 10, 100, 2);

    // Botão 'X'
    tft.fillRoundRect(200, 0, 40, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_RED, TFT_WHITE);
    tft.drawString("X", 215, 8, 2);

    uint16_t tx, ty;
    while (true) {
        if (getTouch(&tx, &ty) && tx >= 200 && ty <= 40) break;
        BoardKey key = getKeyInput();
        if (key == BOARD_KEY_ESC) break;
        delay(50);
    }
}

// Desenha tela de exceção genérica (LUA/JS EXCEPTION) e espera input.
static void showExceptionScreen(const char* title, const String& msg)
{
    Serial.printf("[%s] %s\n", title, msg.c_str());

    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(title, 10, 10, 4);

    tft.setTextWrap(true, true);
    tft.setTextFont(2);
    tft.setCursor(10, 45);
    tft.print(msg);

    // Botão 'X'
    tft.fillRoundRect(200, 0, 40, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_RED, TFT_WHITE);
    tft.drawString("X", 215, 8, 2);

    uint16_t tx, ty;
    while (true) {
        if (getTouch(&tx, &ty) && tx >= 200 && ty <= 40) break;
        BoardKey key = getKeyInput();
        if (key != BOARD_KEY_NONE) break;
        delay(50);
    }
}

// Detecta se a mensagem é sinal de OOM
static bool isOomMessage(const String& msg)
{
    return msg.indexOf("alloc") != -1 ||
           msg.indexOf("out of memory") != -1;
}

// Detecta sinal oculto de saída
static bool isOsExitMessage(const String& msg)
{
    return msg.indexOf("OS_EXIT") != -1;
}

// 1. Alocador Lua
static void *my_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud;
    (void)osize;
    if (nsize == 0) {
        free(ptr);
        return nullptr;
    }
    
    void *p = psram_or_internal_realloc(ptr, nsize);
    if (!p) {
        Serial.println("[Lua] Out of memory!");
    }
    return p;
}

// 2. Alocador Duktape (malloc)
static void *my_alloc(void *udata, duk_size_t size) {
    (void)udata;
    if (size == 0) return nullptr;

    void *p = psram_or_internal_malloc(size);
    if (!p) {
        Serial.println("[Duktape] Out of memory!");
    }
    return p;
}

// 3. Alocador Duktape (realloc)
static void *my_realloc(void *udata, void *ptr, duk_size_t size) {
    (void)udata;
    if (size == 0) {
        free(ptr);
        return nullptr;
    }

    void *p = psram_or_internal_realloc(ptr, size);
    if (!p) {
        Serial.println("[Duktape] Out of memory!");
    }
    return p;
}

// 2. Manipulador de pânico do Lua (equivalente ao my_fatal do Duktape)
static int my_lua_panic(lua_State *L) {
    const char* msg = lua_tostring(L, -1);
    Serial.printf("Lua fatal panic: %s\n", msg ? msg : "no message");
    showOomScreen("Lua");
    ESP.restart();
    return 0;
}

// 3. Verificador de erros do Lua com suporte a tela TFT e Touch/Teclado
void HarixKernel::checkLuaError(lua_State *L, int result) {
    if (result == LUA_OK) return;

    const char* msg = lua_tostring(L, -1);
    String errorMsg = msg ? msg : "Unknown Lua error";

    if (isOsExitMessage(errorMsg)) {
        lua_pop(L, 1);
        return;
    }

    if (isOomMessage(errorMsg)) {
        showOomScreen("Lua");
        lua_pop(L, 1);
        return;
    }

    showExceptionScreen("LUA EXCEPTION!", errorMsg);
    lua_pop(L, 1);
}

static void my_free(void *udata, void *ptr) {
    free(ptr);
}

// Dummy fatal error handler if duktape aborts
static void my_fatal(void* udata, const char* msg) {
    Serial.printf("Duktape fatal error: %s\n", msg ? msg : "no message");
    showOomScreen("Duktape");
    ESP.restart();
}

void HarixKernel::checkJSError(duk_context *ctx, duk_int_t result) {
    if (result == 0) return;

    // Extrai mensagem — Duktape: se for Error, pega `.stack`
    String errorMsg;
    if (duk_is_error(ctx, -1)) {
        duk_get_prop_string(ctx, -1, "stack");
        errorMsg = duk_safe_to_string(ctx, -1);
        duk_pop(ctx);  // remove stack
    } else {
        errorMsg = duk_safe_to_string(ctx, -1);
    }

    if (isOsExitMessage(errorMsg)) {
        duk_pop(ctx);
        return;
    }

    if (isOomMessage(errorMsg)) {
        showOomScreen("Duktape");
        duk_pop(ctx);
        return;
    }

    showExceptionScreen("JS EXCEPTION!", errorMsg);
    duk_pop(ctx);
}

void HarixKernel::executeJS(const char* jsCode) {
    if (!ctx) return;
    
    duk_int_t rc = duk_peval_string(ctx, jsCode);
    checkJSError(ctx, rc);
}

// Struct to pass data to the syntax check task
struct SyntaxCheckParams {
    const char* jsCode;
    String result;
    bool done;
};

static void syntaxCheckTask(void* param) {
    SyntaxCheckParams* p = (SyntaxCheckParams*)param;
    
    duk_context *tempCtx = duk_create_heap(my_alloc, my_realloc, my_free, nullptr, nullptr);
    if (!tempCtx) {
        p->result = "Out of Memory allocating JS heap";
        p->done = true;
        vTaskDelete(NULL);
        return;
    }
    
    duk_int_t rc = duk_pcompile_string(tempCtx, 0, p->jsCode);
    if (rc != 0) {
        p->result = duk_safe_to_string(tempCtx, -1);
        Serial.printf("Syntax Error: %s\n", p->result.c_str());
    } else {
        p->result = "";
    }
    duk_pop(tempCtx);
    duk_destroy_heap(tempCtx);
    
    p->done = true;
    vTaskDelete(NULL);
}

String HarixKernel::checkSyntax(const char* jsCode) {
    SyntaxCheckParams params;
    params.jsCode = jsCode;
    params.result = "";
    params.done = false;
    
    // Run in a dedicated task with 16KB stack to avoid overflowing loopTask
    BaseType_t created = xTaskCreatePinnedToCore(
        syntaxCheckTask,
        "syntaxChk",
        16384,        // 16KB stack just for this task
        &params,
        1,            // Low priority
        NULL,
        1             // Run on Core 1
    );
    
    if (created != pdPASS) {
        return "Failed to create syntax check task";
    }
    
    // Block until the task finishes
    while (!params.done) {
        delay(10);
    }
    
    return params.result;
}

HarixKernel::Engine HarixKernel::detectEngine(const String& filePath)
{
    if (filePath.endsWith(".lua") || filePath.endsWith(".luac"))
        return Engine::Lua;

    if (filePath.endsWith(".wren"))
        return Engine::Wren;

    if (filePath.endsWith(".js"))
        return Engine::Duktape;

    return Engine::Unknown;
}

static void showOutOfRamError(const char* engineName)
{
    Serial.printf("Failed to create %s state/heap for app.\n", engineName);
    showOomScreen(engineName);
}

void HarixKernel::runFile(const char* filePath)
{
    String path(filePath);
    Engine engine = detectEngine(path);

    switch (engine)
    {
        case Engine::Lua:
            Serial.print("[App] Lua: ");
            Serial.println(path);
            runLuaFile(filePath);
            break;

        case Engine::Wren:
            Serial.print("[App] Wren: ");
            Serial.println(path);
            runWrenFile(filePath);
            break;

        case Engine::Duktape:
            Serial.print("[App] JavaScript: ");
            Serial.println(path);
            runJsFile(filePath);
            break;

        default:
            Serial.print("[App] Unknown file type: ");
            Serial.println(path);
            break;
    }
}

void HarixKernel::runWrenFile(const char* filePath)
{
    String content = FileSystem::readTextFile(filePath);

    if (content.length() == 0)
    {
        Serial.print("Failed to read Wren file: ");
        Serial.println(filePath);
        return;
    }

    String source;
    source.reserve(strlen(HARIX_WREN_API_SOURCE) + content.length() + 2);
    source += HARIX_WREN_API_SOURCE;
    source += "\n";
    source += content;

    int apiLines = 0;
    for (const char* p = HARIX_WREN_API_SOURCE; *p; p++)
        if (*p == '\n') apiLines++;
    apiLines++;

    EngineTaskRunner::run("wren_run", kWrenTaskStackSize, [source, apiLines]()
    {
        if (!WrenRuntime::begin())
        {
            showOutOfRamError("Wren");
            return;
        }

        WrenBindings::clearErrors();
        WrenRuntime::execute(source.c_str(), apiLines);
        WrenRuntime::shutdown();
    });
}

void HarixKernel::runJsFile(const char* filePath)
{
    String path(filePath);

    EngineTaskRunner::run("js_run", kJsTaskStackSize, [path]()
    {
        if (ctx)
        {
            duk_destroy_heap(ctx);
            ctx = nullptr;
        }

        ctx = duk_create_heap(my_alloc, my_realloc, my_free, nullptr, my_fatal);

        if (!ctx)
        {
            showOutOfRamError("Duktape");
            return; // Soft exit back to OS
        }

        JSBindings::init(ctx);

        {
            String content = FileSystem::readTextFile(path.c_str());

            if (content.length() == 0)
            {
                Serial.print("Failed to read JS file: ");
                Serial.println(path);
                duk_destroy_heap(ctx);
                ctx = nullptr;
                return;
            }

            duk_push_string(ctx, path.c_str());
            duk_int_t rc = duk_pcompile_string_filename(ctx, 0, content.c_str());

            if (rc != 0)
            {
                checkJSError(ctx, rc);
                duk_destroy_heap(ctx);
                ctx = nullptr;
                return;
            }
        } // `content` sai de escopo aqui, liberando RAM antes do app rodar

        duk_int_t rc = duk_pcall(ctx, 0);
        checkJSError(ctx, rc);

        duk_destroy_heap(ctx);
        ctx = nullptr;
    });
}

void HarixKernel::runLuaFile(const char* filePath)
{
    String path(filePath);

    EngineTaskRunner::run("lua_run", kLuaTaskStackSize, [path]()
    {
        if (L)
        {
            lua_close(L);
            L = nullptr;
        }

        L = lua_newstate(my_lua_alloc, nullptr);
        if (!L)
        {
            showOutOfRamError("Lua");
            return;
        }

        luaL_openlibs(L);
        lua_atpanic(L, my_lua_panic);
        LuaBindings::init(L);

        // ------------------------------------------------------------
        // 1) Diretório do app
        // ------------------------------------------------------------
        String appDirectory = path;
        int lastSlash = appDirectory.lastIndexOf('/');
        if (lastSlash > 0) {
            appDirectory = appDirectory.substring(0, lastSlash);
        } else {
            appDirectory = "/local";
        }

        ModuleCache::setAppDirectory(appDirectory);
        setupCustomRequire(L);

        Serial.print("App directory: ");
        Serial.println(appDirectory);

        // ------------------------------------------------------------
        // 2) Normaliza entrada (aceita .lua ou .luac)
        // ------------------------------------------------------------
        String luaPath;
        String luacPath;

        if (path.endsWith(".luac"))
        {
            luacPath = path;
            luaPath  = path.substring(0, path.length() - 5) + ".lua";
        }
        else if (path.endsWith(".lua"))
        {
            luaPath  = path;
            luacPath = ModuleCache::getBinPath(luaPath);
        }
        else
        {
            Serial.println("Invalid file extension");
            lua_close(L);
            L = nullptr;
            return;
        }

        Serial.print("Lua source:   ");
        Serial.println(luaPath);
        Serial.print("Lua bytecode: ");
        Serial.println(luacPath);
        Serial.print("Lua meta:     ");
        Serial.println(ModuleCache::getMetaPath(luaPath));

        // ------------------------------------------------------------
        // 3) Detecta o que existe
        // ------------------------------------------------------------
        bool hasLua  = FileSystem::exists(luaPath.c_str());

        String prodLuac = luaPath;
        if (prodLuac.endsWith(".lua"))
            prodLuac = prodLuac.substring(0, prodLuac.length() - 4) + ".luac";

        bool hasLuacBin  = FileSystem::exists(luacPath.c_str());
        bool hasLuacProd = (prodLuac != luacPath) && FileSystem::exists(prodLuac.c_str());

        Serial.print("Has .lua:        ");
        Serial.println(hasLua ? "YES" : "NO");
        Serial.print("Has .luac(bin):  ");
        Serial.println(hasLuacBin ? "YES" : "NO");
        Serial.print("Has .luac(prod): ");
        Serial.println(hasLuacProd ? "YES" : "NO");

        if (!hasLua && !hasLuacBin && !hasLuacProd)
        {
            Serial.println("No Lua files found!");
            lua_close(L);
            L = nullptr;
            return;
        }

        if (hasLuacProd && !hasLua && !hasLuacBin)
        {
            luacPath = prodLuac;
            hasLuacBin = true;
        }

        // ------------------------------------------------------------
        // 4) TELA DE LOADING
        //    - conta arquivos .lua no app
        //    - registra callback no ModuleCache
        // ------------------------------------------------------------
        int totalFiles = countLuaFilesRecursive(appDirectory);
        if (totalFiles <= 0) totalFiles = 1;   // pelo menos o main

        Serial.printf("[Loading] Total de módulos: %d\n", totalFiles);

        LoadingScreen::begin("Carregando app");
        LoadingScreen::setProgress(0, totalFiles);

        ModuleCache::setProgressTotal(totalFiles);
        ModuleCache::setProgressCallback(
            [](const char* action, const char* name, int cur, int total) {
                LoadingScreen::setStatus(action, name);
                LoadingScreen::setProgress(cur, total);
            });

        // ------------------------------------------------------------
        // 5) Fallback para fonte
        // ------------------------------------------------------------
        int  rc     = LUA_OK;
        bool loaded = false;

        auto fallbackToSource = [&]()
        {
            Serial.println("Falling back to source execution");
            String content = FileSystem::readTextFile(luaPath.c_str());
            if (content.length() > 0)
            {
                rc = luaL_loadbuffer(L, content.c_str(), content.length(),
                                     luaPath.c_str());
                loaded = (rc == LUA_OK);
            }
        };

        // ------------------------------------------------------------
        // 6) Decide: recompilar / usar cache / só bytecode
        // ------------------------------------------------------------
        bool needCompile = false;

        if (hasLua && !hasLuacBin)
        {
            Serial.println("Compiling source (no bytecode yet)...");
            needCompile = true;
        }
        else if (hasLua && hasLuacBin)
        {
            needCompile = ModuleCache::needsRecompile(luaPath, luacPath);
            if (needCompile)
                Serial.println("Source changed, recompiling...");
            else
                Serial.println("Using cached bytecode");
        }
        else
        {
            Serial.println("Loading bytecode directly...");
        }

        // Reporta o estado inicial para a tela de loading
        LoadingScreen::setStatus(
            needCompile ? "Compilando" :
            (hasLuacBin ? "Carregando" : "Lendo"),
            "main");
        LoadingScreen::setProgress(1, totalFiles);

        if (needCompile)
        {
            if (ModuleCache::compileToLuac(L, luaPath, luacPath))
            {
                loaded = ModuleCache::loadLuac(L, luacPath);
            }
            else
            {
                fallbackToSource();
            }
        }
        else if (hasLuacBin)
        {
            loaded = ModuleCache::loadLuac(L, luacPath);
            if (!loaded && hasLua)
            {
                fallbackToSource();
            }
        }
        else
        {
            fallbackToSource();
        }

        if (!loaded)
        {
            LoadingScreen::setStatus("Erro ao carregar", "main");
            LoadingScreen::end();
            ModuleCache::setProgressCallback(nullptr);

            if (rc != LUA_OK)
                checkLuaError(L, rc);
            else
                Serial.println("Failed to load script");

            lua_close(L);
            L = nullptr;
            return;
        }

        // ------------------------------------------------------------
        // 7) Executa o main.lua
        // ------------------------------------------------------------
        LoadingScreen::setStatus("Executando", "main");
        LoadingScreen::setProgress(totalFiles, totalFiles);

        rc = lua_pcall(L, 0, LUA_MULTRET, 0);

        // Encerra a tela de loading ANTES de rodar checkLuaError,
        // porque checkLuaError pode querer desenhar uma tela de exceção
        LoadingScreen::end();
        ModuleCache::setProgressCallback(nullptr);

        checkLuaError(L, rc);

        lua_close(L);
        L = nullptr;
    });
}

// Loader "puro": retorna a função (não executa)
static int lua_custom_loader(lua_State* L) {
    size_t nameLen = 0;
    const char* namePtr = luaL_checklstring(L, 1, &nameLen);
    String moduleName(namePtr, nameLen);
    
    String modulePath = ModuleCache::resolveModulePath(L, moduleName.c_str());
    if (modulePath.length() == 0) {
        lua_pushfstring(L, "\n\tMódulo '%s' não encontrado", moduleName.c_str());
        return 1;   // loader retorna mensagem de erro
    }
    
    bool ok = false;
    if (modulePath.endsWith(".luac")) {
        ok = ModuleCache::loadLuac(L, modulePath);
    } else {
        String luacPath = ModuleCache::getLuacPath(modulePath);
        if (ModuleCache::needsRecompile(modulePath, luacPath)) {
            if (ModuleCache::compileToLuac(L, modulePath, luacPath)) {
                ok = ModuleCache::loadLuac(L, luacPath);
            } else {
                String source = FileSystem::readTextFile(modulePath.c_str());
                if (source.length() > 0) {
                    ok = (luaL_loadbuffer(L, source.c_str(), source.length(),
                                          modulePath.c_str()) == 0);
                }
            }
        } else {
            ok = ModuleCache::loadLuac(L, luacPath);
        }
    }
    
    if (!ok) {
        lua_pushfstring(L, "\n\tErro ao carregar '%s'", moduleName.c_str());
        return 1;
    }
    return 1;   // deixa a função na pilha
}

static int lua_custom_require(lua_State* L) {
    // Garante que o nome é string (Lua 5.1: luaL_checkstring já converte)
    size_t nameLen = 0;
    const char* namePtr = luaL_checklstring(L, 1, &nameLen);
    String moduleName(namePtr, nameLen);
    
    // ------------------------------------------------------------
    // 1) Verifica cache _LOADED
    // ------------------------------------------------------------
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");   // [ _LOADED ]
    lua_getfield(L, -1, moduleName.c_str());         // [ _LOADED, _LOADED[name] ]
    
    if (lua_toboolean(L, -1)) {
        // Já carregado: devolve
        lua_remove(L, -2);                            // [ _LOADED[name] ]
        Serial.printf("[Require] '%s' do cache\n", moduleName.c_str());
        return 1;
    }
    
    lua_pop(L, 2);                                    // limpa
    
    // ------------------------------------------------------------
    // 2) Resolve caminho
    // ------------------------------------------------------------
    String modulePath = ModuleCache::resolveModulePath(L, moduleName.c_str());
    if (modulePath.length() == 0) {
        return luaL_error(L, "Módulo '%s' não encontrado", moduleName.c_str());
    }
    
    // ------------------------------------------------------------
    // 3) Sentinel contra loop circular
    //    _LOADED[name] = true  ANTES de rodar
    // ------------------------------------------------------------
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");   // [ _LOADED ]
    lua_pushboolean(L, 1);                            // [ _LOADED, true ]
    lua_setfield(L, -2, moduleName.c_str());          // _LOADED[name] = true
    lua_pop(L, 1);                                    // limpa
    
    // ------------------------------------------------------------
    // 4) Carrega a função (chunk) — deixa 1 valor na pilha
    // ------------------------------------------------------------
    bool ok = false;
    
    if (modulePath.endsWith(".luac")) {
        Serial.printf("[Require] Carregando bytecode: %s\n", modulePath.c_str());
        ok = ModuleCache::loadLuac(L, modulePath);
    } else {
        String luacPath = ModuleCache::getLuacPath(modulePath);
        
        if (ModuleCache::needsRecompile(modulePath, luacPath)) {
            Serial.printf("[Require] Compilando '%s'\n", moduleName.c_str());
            
            if (ModuleCache::compileToLuac(L, modulePath, luacPath)) {
                ok = ModuleCache::loadLuac(L, luacPath);
            } else {
                // fallback para fonte
                String source = FileSystem::readTextFile(modulePath.c_str());
                if (source.length() > 0) {
                    ok = (luaL_loadbuffer(L, source.c_str(), source.length(),
                                          modulePath.c_str()) == 0);
                }
            }
        } else {
            ok = ModuleCache::loadLuac(L, luacPath);
        }
    }
    
    if (!ok) {
        // Remove sentinel antes de propagar erro
        lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
        lua_pushnil(L);
        lua_setfield(L, -2, moduleName.c_str());
        lua_pop(L, 1);
        return luaL_error(L, "Erro ao carregar módulo '%s'", moduleName.c_str());
    }
    
    // ------------------------------------------------------------
    // 5) Executa (pcall) — agora tem [ chunk ]
    // ------------------------------------------------------------
    if (lua_pcall(L, 0, 1, 0) != 0) {
        // Erro: remove sentinel e propaga
        lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
        lua_pushnil(L);
        lua_setfield(L, -2, moduleName.c_str());
        lua_pop(L, 1);
        return lua_error(L);
    }
    
    // ------------------------------------------------------------
    // 6) Normaliza resultado: se nil, vira true
    // ------------------------------------------------------------
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    
    // ------------------------------------------------------------
    // 7) Salva em _LOADED[name]
    // ------------------------------------------------------------
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");   // [ result, _LOADED ]
    lua_pushvalue(L, -2);                             // [ result, _LOADED, result ]
    lua_setfield(L, -2, moduleName.c_str());          // _LOADED[name] = result
    lua_pop(L, 1);                                    // [ result ]
    
    Serial.printf("[Require] '%s' carregado\n", moduleName.c_str());
    return 1;
}

void HarixKernel::setupCustomRequire(lua_State* L) {
    // Substitui o require padrão
    lua_pushcfunction(L, lua_custom_require);
    lua_setglobal(L, "require");
    
    // Adiciona loader personalizado ao package.loaders
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaders");
    
    if (lua_istable(L, -1)) {
        int count = lua_objlen(L, -1);
        lua_pushcfunction(L, lua_custom_loader);
        lua_rawseti(L, -2, count + 1);
    }
    
    lua_pop(L, 2); // Remove package e loaders
    
    // Configura package.path para incluir o diretório do app
    String appDir = ModuleCache::getAppDirectory();
    if (appDir.length() > 0) {
        lua_getglobal(L, "package");
        lua_pushstring(L, (appDir + "/?.lua;" + appDir + "/?/init.lua;").c_str());
        lua_setfield(L, -2, "path");
        lua_pop(L, 1);
    }
}