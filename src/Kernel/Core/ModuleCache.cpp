// ModuleCache.cpp
#include "ModuleCache.h"
#include <mbedtls/md5.h>
#include "HarixKernel.h"

std::vector<ModuleCache::CacheEntry> ModuleCache::_cache;
String ModuleCache::_appDirectory = "";

ModuleCache::ProgressCallback ModuleCache::_progressCb = nullptr;
int ModuleCache::_progressCurrent = 0;
int ModuleCache::_progressTotal   = 0;

// ============================================
// Helpers internos
// ============================================

// Remove o prefixo _appDirectory de um caminho absoluto,
// devolvendo o caminho relativo (sem '/' inicial).
static String makeRelativeToApp(const String& absPath) {
    String relative = absPath;
    if (ModuleCache::getAppDirectory().length() > 0 &&
        relative.startsWith(ModuleCache::getAppDirectory())) {
        relative = relative.substring(ModuleCache::getAppDirectory().length());
        if (relative.startsWith("/")) relative = relative.substring(1);
    }
    return relative;
}

// Remove a extensão (.lua / .luac) do caminho relativo.
static String stripExtension(const String& p) {
    int lastDot = p.lastIndexOf('.');
    int lastSlash = p.lastIndexOf('/');
    if (lastDot > lastSlash && lastDot > 0) {
        return p.substring(0, lastDot);
    }
    return p;
}

// Cria todos os diretórios pais de um caminho.
static bool ensureParentDirs(const String& path) {
    int idx = path.indexOf('/', 1);
    while (idx > 0) {
        String dir = path.substring(0, idx);
        if (!FileSystem::exists(dir.c_str())) {
            if (!FileSystem::mkdir(dir.c_str())) {
                Serial.printf("[FS] Falha ao criar %s\n", dir.c_str());
                return false;
            }
        }
        idx = path.indexOf('/', idx + 1);
    }
    return true;
}

void ModuleCache::setProgressCallback(ProgressCallback cb) {
    _progressCb = cb;
}

void ModuleCache::reportProgress(const char* action, const char* name) {
    if (_progressCb) {
        _progressCb(action, name, _progressCurrent, _progressTotal);
    
    }
}

void ModuleCache::setProgressTotal(int total) {
    _progressTotal = total;
    _progressCurrent = 0;
}

// ============================================
// Caminhos
// ============================================

// app/main.lua -> app/bin/main.luac
// app/mod/foo.lua -> app/bin/mod/foo.luac
String ModuleCache::getBinPath(const String& luaPath) {
    if (_appDirectory.length() == 0) return "";

    if (luaPath.endsWith(".luac")) {
        // Já é .luac: se estiver em bin/, devolve como está.
        // Se estiver no root (produção), devolve como está também.
        return luaPath;
    }

    String relative = stripExtension(makeRelativeToApp(luaPath));
    return _appDirectory + "/bin/" + relative + ".luac";
}

// app/main.lua -> app/meta/main.meta
// app/mod/foo.lua -> app/meta/mod/foo.meta
String ModuleCache::getMetaPath(const String& luaPath) {
    if (_appDirectory.length() == 0) return "";

    String relative = stripExtension(makeRelativeToApp(luaPath));
    return _appDirectory + "/meta/" + relative + ".meta";
}

// Alias público: sempre aponta para o .luac de destino (bin/ em dev).
String ModuleCache::getLuacPath(const String& luaPath) {
    if (luaPath.endsWith(".luac")) return luaPath;
    return getBinPath(luaPath);
}

// ============================================
// Resolução de módulos
// ============================================

String ModuleCache::resolveModulePath(lua_State* /*L*/, const char* moduleName) {
    if (moduleName == nullptr) return "";

    String modulePath = String(moduleName);
    modulePath.replace(".", "/");

    if (modulePath.indexOf("..") >= 0) {
        Serial.println("[ModuleCache] ERRO: Path traversal detectado!");
        return "";
    }

    if (_appDirectory.length() == 0) {
        Serial.println("[ModuleCache] ERRO: Diretório do app não configurado!");
        return "";
    }

    String candidates[4];
    candidates[0] = _appDirectory + "/bin/" + modulePath + ".luac";    // dev
    candidates[1] = _appDirectory + "/"     + modulePath + ".luac";    // prod
    candidates[2] = _appDirectory + "/bin/" + modulePath + ".lua";     // dev fonte
    candidates[3] = _appDirectory + "/"     + modulePath + ".lua";     // prod fonte

    for (int i = 0; i < 4; i++) {
        if (FileSystem::exists(candidates[i].c_str()) &&
            FileSystem::isFile(candidates[i].c_str())) {
            Serial.printf("[ModuleCache] '%s' -> %s\n",
                          moduleName, candidates[i].c_str());
            return candidates[i];
        }
    }

    Serial.printf("[ModuleCache] Módulo '%s' não encontrado\n", moduleName);
    return "";
}

// ============================================
// Invalidação de cache
// ============================================

bool ModuleCache::needsRecompile(const String& luaPath, const String& luacPath) {
    // Sem fonte -> nada a recompilar (só usa o .luac existente)
    if (!FileSystem::exists(luaPath.c_str())) return false;

    // Sem bytecode -> precisa compilar
    if (!FileSystem::exists(luacPath.c_str())) {
        Serial.printf("[ModuleCache] %s não existe, recompilar\n", luacPath.c_str());
        return true;
    }

    // 1) mtime (quando o FS suporta)
    time_t luaTime  = FileSystem::getLastModified(luaPath.c_str());
    time_t luacTime = FileSystem::getLastModified(luacPath.c_str());
    if (luaTime > 0 && luacTime > 0 && luaTime > luacTime) {
        Serial.printf("[ModuleCache] mtime: %s mais novo\n", luaPath.c_str());
        return true;
    }

    // 2) MD5 contra meta
    String metaPath = getMetaPath(luaPath);
    if (FileSystem::exists(metaPath.c_str())) {
        String metaContent = FileSystem::readTextFile(metaPath.c_str());
        if (metaContent.length() >= 32) {
            String savedMD5 = metaContent.substring(0, 32);

            String currentMD5 = FileSystem::getFileMD5(luaPath.c_str());
            if (currentMD5.length() == 0) {
                // Falha ao calcular MD5 -> não confia no meta, recompila
                Serial.printf("[ModuleCache] MD5 indisponível para %s, recompilar\n",
                              luaPath.c_str());
                return true;
            }

            if (savedMD5 != currentMD5) {
                Serial.printf("[ModuleCache] MD5 mudou para %s\n", luaPath.c_str());
                return true;
            }
            return false;
        }
    }

    Serial.printf("[ModuleCache] Sem meta para %s, recompilar\n", luaPath.c_str());
    return true;
}

// ============================================
// Compilação / dump
// ============================================

static int moduleDumpWriter(lua_State* /*L*/, const void* p, size_t sz, void* ud) {
    LuaDumpBuffer* buf = (LuaDumpBuffer*)ud;

    if (buf->size + sz > buf->capacity) {
        size_t newCapacity = (buf->capacity == 0) ? 1024 : buf->capacity * 2;
        while (newCapacity < buf->size + sz) {
            newCapacity *= 2;
        }

        uint8_t* newData = (uint8_t*)realloc(buf->data, newCapacity);
        if (!newData) {
            return 1;
        }

        buf->data = newData;
        buf->capacity = newCapacity;
    }

    memcpy(buf->data + buf->size, p, sz);
    buf->size += sz;
    return 0;
}

bool ModuleCache::compileToLuac(lua_State* L, const String& luaPath, const String& luacPath) {
    Serial.printf("[ModuleCache] Compilando %s\n", luaPath.c_str());

    String name = luaPath;
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.endsWith(".lua")) name = name.substring(0, name.length() - 4);

    reportProgress("Compilando", name.c_str());

    // Lê código fonte
    String source = FileSystem::readTextFile(luaPath.c_str());
    if (source.length() == 0) {
        Serial.printf("[ModuleCache] ERRO: %s vazio\n", luaPath.c_str());
        return false;
    }

    // Compila
    int rc = luaL_loadbuffer(L, source.c_str(), source.length(), luaPath.c_str());
    if (rc != LUA_OK) {
        Serial.printf("[ModuleCache] ERRO de compilação: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    // Dump bytecode
    LuaDumpBuffer buf = {nullptr, 0, 0};
    rc = lua_dump(L, moduleDumpWriter, &buf);

    if (rc != 0) {
        Serial.println("[ModuleCache] ERRO ao gerar bytecode");
        lua_pop(L, 1);
        if (buf.data) free(buf.data);
        return false;
    }

    lua_pop(L, 1); // Remove chunk

    // Salva bytecode (cria app/bin/... se necessário)
    ensureParentDirs(luacPath);
    bool saved = FileSystem::writeBinaryFile(luacPath.c_str(), buf.data, buf.size);
    free(buf.data);

    if (!saved) {
        Serial.printf("[ModuleCache] ERRO ao salvar %s\n", luacPath.c_str());
        return false;
    }

    // Grava meta (cria app/meta/... se necessário)
    String metaPath = getMetaPath(luaPath);
    if (metaPath.length() == 0) {
        Serial.println("[ModuleCache] AVISO: getMetaPath vazio, meta não gravado");
        return true; // bytecode foi salvo, mesmo sem meta
    }

    ensureParentDirs(metaPath);

    String md5 = FileSystem::getFileMD5(luaPath.c_str());
    if (md5.length() == 0) {
        Serial.println("[ModuleCache] AVISO: MD5 vazio, meta pode não invalidar");
    }

    String metaContent = md5 + "|" + String(FileSystem::getFileSize(luaPath.c_str()));
    bool metaSaved = FileSystem::writeTextFile(metaPath.c_str(), metaContent.c_str());

    Serial.printf("[ModuleCache] %s -> %s (meta: %s %s)\n",
                  luaPath.c_str(), luacPath.c_str(), metaPath.c_str(),
                  metaSaved ? "OK" : "FALHOU");

    return true;
}

// ============================================
// Carregamento
// ============================================

// Assinatura e header mínimo de Lua 5.1
static const uint8_t kLuaSignature[4] = { 0x1B, 'L', 'u', 'a' };
static const uint8_t kLuaVersion51  = 0x51;
static const size_t  kLuaHeaderSize = 12;   // 5.1 header

bool ModuleCache::loadLuac(lua_State* L, const String& luacPath) {

    String name = luacPath;
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.endsWith(".luac")) name = name.substring(0, name.length() - 5);

    reportProgress("Carregando", name.c_str());

    // ------------------------------------------------------------
    // 1) Tamanho
    // ------------------------------------------------------------
    size_t fileSize = FileSystem::getFileSize(luacPath.c_str());
    if (fileSize < kLuaHeaderSize) {
        Serial.printf("[ModuleCache] Bytecode muito pequeno (%u bytes): %s\n",
                      (unsigned)fileSize, luacPath.c_str());
        return false;
    }

    // ------------------------------------------------------------
    // 2) Aloca buffer (tenta PSRAM primeiro, cai para RAM interna)
    // ------------------------------------------------------------
    uint8_t* buffer = nullptr;
    if (psramFound()) {
        buffer = (uint8_t*)heap_caps_malloc(
            fileSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!buffer) {
        buffer = (uint8_t*)malloc(fileSize);
    }
    if (!buffer) {
        Serial.printf("[ModuleCache] ERRO de alocação (%u bytes)\n",
                      (unsigned)fileSize);
        return false;
    }

    // ------------------------------------------------------------
    // 3) Lê arquivo
    // ------------------------------------------------------------
    size_t bytesRead = FileSystem::readBinaryFile(
        luacPath.c_str(), buffer, fileSize);

    if (bytesRead != fileSize) {
        Serial.printf("[ModuleCache] ERRO ao ler bytecode (%u/%u): %s\n",
                      (unsigned)bytesRead, (unsigned)fileSize, luacPath.c_str());
        free(buffer);
        return false;
    }

    // ------------------------------------------------------------
    // 4) Valida assinatura + versão Lua 5.1
    // ------------------------------------------------------------
    if (memcmp(buffer, kLuaSignature, 4) != 0) {
        Serial.printf("[ModuleCache] Assinatura inválida: %s\n",
                      luacPath.c_str());
        free(buffer);
        return false;
    }

    if (buffer[4] != kLuaVersion51) {
        Serial.printf("[ModuleCache] Versão Lua inesperada: 0x%02X "
                      "(esperado 0x%02X para 5.1): %s\n",
                      buffer[4], kLuaVersion51, luacPath.c_str());
        free(buffer);
        return false;
    }

    // (Opcional) valida tamanhos de tipo — útil para pegar .luac
    // gerado em outra plataforma. Como compilamos e carregamos na
    // mesma ESP32, isso é redundante, mas defensivo.
    // Em 5.1: buffer[8]=sizeof(int), buffer[9]=sizeof(size_t),
    //         buffer[10]=sizeof(Instruction), buffer[11]=sizeof(lua_Number)
    if (buffer[8] != sizeof(int) ||
        buffer[9] != sizeof(size_t)) {
        Serial.printf("[ModuleCache] Bytecode incompatível "
                      "(int=%u, size_t=%u): %s\n",
                      buffer[8], buffer[9], luacPath.c_str());
        free(buffer);
        return false;
    }

    // ------------------------------------------------------------
    // 5) Carrega o chunk (copia internamente)
    // ------------------------------------------------------------
    int rc = luaL_loadbuffer(L, (const char*)buffer, bytesRead,
                             luacPath.c_str());
    free(buffer);

    if (rc != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        Serial.printf("[ModuleCache] ERRO ao carregar %s: %s\n",
                      luacPath.c_str(),
                      err ? err : "(erro sem mensagem)");
        lua_pop(L, 1);
        return false;
    }

    return true;
}

// ============================================
// Pré-compilação
// ============================================

void ModuleCache::precompileDirectory(lua_State* L, const String& dirPath) {
    Serial.printf("[ModuleCache] Pré-compilando %s\n", dirPath.c_str());

    const int kMaxFiles = 200;
    String* files = new String[kMaxFiles];
    if (!files) {
        Serial.println("[ModuleCache] ERRO: sem memória para listar dir");
        return;
    }

    int count = FileSystem::listDir(dirPath.c_str(), files, kMaxFiles);
    if (count < 0) count = 0;

    for (int i = 0; i < count; i++) {
        String fullPath = dirPath;
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += files[i];

        // Se for subdiretório, recursão
        if (FileSystem::isDirectory(fullPath.c_str())) {
            // Evita recursão em bin/ e meta/ (já são saída)
            String base = files[i];
            if (base == "bin" || base == "meta") continue;

            precompileDirectory(L, fullPath);
            continue;
        }

        if (files[i].endsWith(".lua")) {
            String luacPath = getBinPath(fullPath);
            if (luacPath.length() == 0) continue;

            if (needsRecompile(fullPath, luacPath)) {
                compileToLuac(L, fullPath, luacPath);
            }
        }
    }

    delete[] files;
}