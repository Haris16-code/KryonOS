#include "FileSystem.h"
#include "boards/Board.h"

fs::FS* FileSystem::_fs = nullptr;

bool FileSystem::init() {
    bool success = true;
    
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        success = false;
    } else {
        Serial.println("LittleFS Mount Successful");
        if (!LittleFS.exists("/apps")) LittleFS.mkdir("/apps");
    }

    _fs = initSD();

    if(_fs == nullptr){
        Serial.println("SD Mount Failed");
        success = false;
    };

    return success;
}

FSPath FileSystem::resolve(const char* path) {
    FSPath res;
    if (path == nullptr) return res;

    String cleanPath = sanitizePath(path);

    if (cleanPath.startsWith("/sd/") || cleanPath == "/sd") {
        if (_fs == nullptr) return res; // SD não montado
        res.fs = _fs;
        res.relPath = cleanPath.substring(3);
    } else if (cleanPath.startsWith("/local/") || cleanPath == "/local") {
        res.fs = &LittleFS;
        res.relPath = cleanPath.substring(6);
    }

    if (res.fs != nullptr && res.relPath.length() == 0) {
        res.relPath = "/";
    }

    return res;
}

size_t FileSystem::readBinaryFile(const char* path, uint8_t* buffer, size_t maxLen) {
    if (path == nullptr || buffer == nullptr || maxLen == 0) return 0;
    
    return withFile(path, FILE_READ, [buffer, maxLen](File& f) {
        if (f.isDirectory()) return (size_t)0;
        
        size_t fileSize = f.size();
        if (fileSize > maxLen) {
            fileSize = maxLen; // Limita ao buffer disponível
        }
        
        size_t bytesRead = f.read(buffer, fileSize);
        return bytesRead;
    });
}

bool FileSystem::writeBinaryFile(const char* path, const uint8_t* data, size_t len) {
    if (path == nullptr || data == nullptr || len == 0) return false;
    
    return withFile(path, FILE_WRITE, [data, len](File& f) {
        size_t written = f.write(data, len);
        return written == len;
    });
}

String FileSystem::readTextFile(const char* path) {
    return withFile(path, FILE_READ, [](File& f) {
        if (f.isDirectory() || f.size() == 0) return String("");
        
        String content;
        if (!content.reserve(f.size())) return String("");

        uint8_t buffer[512];
        while (f.available()) {
            size_t bytesRead = f.read(buffer, sizeof(buffer));
            for (size_t i = 0; i < bytesRead; i++) {
                content += (char)buffer[i];
            }
        }
        return content;
    });
}

bool FileSystem::writeTextFile(const char* path, const char* content) {
    if (content == nullptr) return false;
    return withFile(path, FILE_WRITE, [content](File& f) {
        size_t written = f.print(content);
        return written > 0 || strlen(content) == 0;
    });
}

String FileSystem::sanitizePath(const char* path) {
    String cleanPath = String(path);
        
    while (cleanPath.length() > 1 && cleanPath.endsWith("/")) {
        cleanPath.remove(cleanPath.length() - 1);
    }

    return cleanPath;
}

bool FileSystem::exists(const char* path) {
    FSPath p = resolve(path);
    return p.isValid() ? p.fs->exists(p.relPath.c_str()) : false;
}

bool FileSystem::deleteFile(const char* path) {
    FSPath p = resolve(path);
    if (!p.isValid() || p.relPath == "/") return false;
    return p.fs->remove(p.relPath.c_str());
}

bool FileSystem::formatLittleFS() {
    Serial.println("Formatting LittleFS...");
    return LittleFS.format();
}

int FileSystem::listDir(const char* dirPath, String* resultFiles, int maxFiles) {
    if (dirPath == nullptr || resultFiles == nullptr) return 0;

    // 1. Sanitização inicial usando sanitizePath
    String cleanPath = sanitizePath(dirPath);

    fs::FS* targetFS = nullptr;
    String relPath;

    // 2. Identificação da rota e seleção do sistema de arquivos
    if (cleanPath.startsWith("/sd/") || cleanPath == "/sd") {
        if (_fs == nullptr) return 0; // Proteção contra ponteiro nulo do SD
        targetFS = _fs;
        relPath = cleanPath.substring(3); // Corta "/sd" (ex: "/sd/apps" -> "/apps")
    } else if (cleanPath.startsWith("/local/") || cleanPath == "/local") {
        targetFS = &LittleFS;
        relPath = cleanPath.substring(6); // Corta "/local" (ex: "/local/apps" -> "/apps")
    } else {
        return 0; // Prefixo desconhecido
    }

    if (relPath.length() == 0) relPath = "/";

    // 3. Abertura do diretório
    File dir = targetFS->open(relPath.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return 0;
    }

    int count = 0;
    File file = dir.openNextFile();

    // 4. Varredura dos arquivos/pastas
    while (file && count < maxFiles) {
        String name = String(file.name());

        // Extrai apenas o nome base caso o FatFs retorne caminhos pai
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) {
            name = name.substring(lastSlash + 1);
        }

        // Ignora arquivos do sistema e arquivos ocultos
        if (name.length() > 0 && !name.startsWith(".") && name != "System Volume Information") {
            String fullPath = cleanPath;
            if (!fullPath.endsWith("/")) fullPath += "/";
            fullPath += name;

            resultFiles[count++] = fullPath;
        }

        // Fecha o handler do arquivo para evitar vazamento de memória no FatFs
        file.close();
        file = dir.openNextFile();
    }

    dir.close();
    return count;
}

int FileSystem::listDirectory(const char* dirPath, FileEntry* entries, int maxEntries) {
    if (dirPath == nullptr || entries == nullptr) return 0;

    // 1. Sanitização inicial usando sanitizePath
    String cleanPath = sanitizePath(dirPath);

    fs::FS* targetFS = nullptr;
    String relPath;

    // 2. Identificação da rota e seleção do sistema de arquivos
    if (cleanPath.startsWith("/sd/") || cleanPath == "/sd") {
        if (_fs == nullptr) return 0; // Proteção contra ponteiro nulo do SD
        targetFS = _fs;
        relPath = cleanPath.substring(3); // Corta "/sd" (ex: "/sd/apps" -> "/apps")
    } else if (cleanPath.startsWith("/local/") || cleanPath == "/local") {
        targetFS = &LittleFS;
        relPath = cleanPath.substring(6); // Corta "/local" (ex: "/local/apps" -> "/apps")
    } else {
        return 0; // Prefixo desconhecido
    }

    if (relPath.length() == 0) relPath = "/";

    // 3. Abertura do diretório
    File dir = targetFS->open(relPath.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return 0;
    }

    int count = 0;
    File file = dir.openNextFile();

    // 4. Varredura dos arquivos/pastas
    while (file && count < maxEntries) {
        String name = String(file.name());

        // Extrai apenas o nome base caso o FatFs retorne caminhos pai
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) {
            name = name.substring(lastSlash + 1);
        }

        // Ignora arquivos do sistema e arquivos ocultos
        if (name.length() > 0 && !name.startsWith(".") && name != "System Volume Information") {
            entries[count].name = name;

            String fullPath = cleanPath;
            if (!fullPath.endsWith("/")) fullPath += "/";
            fullPath += name;

            entries[count].path = fullPath;
            entries[count].isDir = file.isDirectory();

            count++;
        }

        // Fecha o handler do arquivo para evitar vazamento de memória no FatFs
        file.close();
        file = dir.openNextFile();
    }

    dir.close();
    return count;
}

bool FileSystem::readCalData(uint16_t* calData) {
    Serial.println("[FileSystem] Iniciando leitura da calibracao do touch...");

    // 1. Protecao contra ponteiro nulo
    if (calData == nullptr) {
        Serial.println("[FileSystem] ERRO: Ponteiro calData e nulo!");
        return false;
    }

    // 2. Verificacao de existencia do arquivo
    if (!exists("/local/touch_cal_p.bin")) {
        Serial.println("[FileSystem] ERRO: Arquivo '/touch_cal_p.bin' nao encontrado.");
        return false;
    }

    File f = LittleFS.open("/touch_cal_p.bin", FILE_READ);
    if (!f) {
        Serial.println("[FileSystem] ERRO: Falha ao abrir '/touch_cal_p.bin' para leitura.");
        return false;
    }

    // 3. Leitura dos 10 bytes (5 valores uint16_t)
    size_t bytesRead = f.read((uint8_t*)calData, 10);
    f.close();

    if (bytesRead == 10) {
        Serial.println("[FileSystem] Calibracao lida com sucesso:");
        Serial.print("  -> X_MIN: "); Serial.println(calData[0]);
        Serial.print("  -> X_MAX: "); Serial.println(calData[1]);
        Serial.print("  -> Y_MIN: "); Serial.println(calData[2]);
        Serial.print("  -> Y_MAX: "); Serial.println(calData[3]);
        Serial.print("  -> Extra/Param: "); Serial.println(calData[4]);
        return true;
    } else {
        Serial.print("[FileSystem] ERRO: Bytes lidos incorretos. Esperado: 10, Lido: ");
        Serial.println(bytesRead);
        return false;
    }
}

bool FileSystem::writeCalData(uint16_t* calData) {
    Serial.println("[FileSystem] Iniciando gravacao da calibracao do touch...");

    // 1. Protecao contra ponteiro nulo
    if (calData == nullptr) {
        Serial.println("[FileSystem] ERRO: Ponteiro calData e nulo!");
        return false;
    }

    File f = LittleFS.open("/touch_cal_p.bin", FILE_WRITE);
    if (!f) {
        Serial.println("[FileSystem] ERRO: Falha ao criar/abrir '/touch_cal_p.bin' para escrita.");
        return false;
    }

    // 2. Gravacao dos 10 bytes (5 valores uint16_t)
    size_t bytesWritten = f.write((uint8_t*)calData, 10);
    f.close();

    if (bytesWritten == 10) {
        Serial.println("[FileSystem] Calibracao salva com sucesso na Flash:");
        Serial.print("  -> X_MIN: "); Serial.println(calData[0]);
        Serial.print("  -> X_MAX: "); Serial.println(calData[1]);
        Serial.print("  -> Y_MIN: "); Serial.println(calData[2]);
        Serial.print("  -> Y_MAX: "); Serial.println(calData[3]);
        Serial.print("  -> Extra/Param: "); Serial.println(calData[4]);
        return true;
    } else {
        Serial.print("[FileSystem] ERRO: Bytes gravados incorretos. Esperado: 10, Gravado: ");
        Serial.println(bytesWritten);
        return false;
    }
}

bool FileSystem::copyFile(const char* srcPath, const char* dstPath) {
    if (srcPath == nullptr || dstPath == nullptr) return false;
    
    // 1. Sanitização dos caminhos de origem e destino
    String cleanSrc = sanitizePath(srcPath);
    String cleanDst = sanitizePath(dstPath);
    
    // 2. Resolução do sistema de arquivos de ORIGEM
    fs::FS* srcFS = nullptr;
    String srcRel;

    if (cleanSrc.startsWith("/sd/") || cleanSrc == "/sd") {
        if (_fs == nullptr) return false; // Proteção se o SD não estiver montado
        srcFS = _fs;
        srcRel = cleanSrc.substring(3);
    } else if (cleanSrc.startsWith("/local/") || cleanSrc == "/local") {
        srcFS = &LittleFS;
        srcRel = cleanSrc.substring(6);
    } else {
        return false;
    }
    if (srcRel.length() == 0) srcRel = "/";

    // 3. Resolução do sistema de arquivos de DESTINO
    fs::FS* dstFS = nullptr;
    String dstRel;

    if (cleanDst.startsWith("/sd/") || cleanDst == "/sd") {
        if (_fs == nullptr) return false; // Proteção se o SD não estiver montado
        dstFS = _fs;
        dstRel = cleanDst.substring(3);
    } else if (cleanDst.startsWith("/local/") || cleanDst == "/local") {
        dstFS = &LittleFS;
        dstRel = cleanDst.substring(6);
    } else {
        return false;
    }
    if (dstRel.length() == 0) dstRel = "/";

    // 4. Abertura do arquivo de origem
    File srcFile = srcFS->open(srcRel.c_str(), FILE_READ);
    if (!srcFile || srcFile.isDirectory()) {
        if (srcFile) srcFile.close();
        return false;
    }
    
    // 5. Abertura do arquivo de destino
    File dstFile = dstFS->open(dstRel.c_str(), FILE_WRITE);
    if (!dstFile) {
        srcFile.close();
        return false;
    }
    
    // 6. Cópia em blocos de 512 bytes com verificação de escrita
    uint8_t buf[512];
    size_t bytesRead = 0;
    bool success = true;

    while ((bytesRead = srcFile.read(buf, sizeof(buf))) > 0) {
        if (dstFile.write(buf, bytesRead) != bytesRead) {
            success = false; // Falha na escrita (espaço insuficiente, erro no SD, etc)
            break;
        }
    }
    
    srcFile.close();
    dstFile.close();
    
    return success;
}

#include <mbedtls/md5.h>

fs::FS* FileSystem::getTargetFS(const char* path, String& relPath) {
    if (path == nullptr) return nullptr;
    
    // 1. Sanitiza o caminho completo
    String cleanPath = sanitizePath(path);
    
    // 2. Identifica a rota e extrai o caminho relativo
    if (cleanPath.startsWith("/sd/") || cleanPath == "/sd") {
        if (_fs == nullptr) return nullptr; // Proteção contra ponteiro nulo do SD
        relPath = cleanPath.substring(3);    // Corta "/sd"
        if (relPath.length() == 0) relPath = "/";
        return _fs;
    } else if (cleanPath.startsWith("/local/") || cleanPath == "/local") {
        relPath = cleanPath.substring(6);    // Corta "/local"
        if (relPath.length() == 0) relPath = "/";
        return &LittleFS;
    }
    
    return nullptr; // Prefixo desconhecido
}

int FileSystem::countFilesInDir(const char* dirPath) {
    if (dirPath == nullptr) return 0;

    // 1. Obtém o FS alvo e o caminho relativo sanitizado
    String relPath;
    fs::FS* targetFS = getTargetFS(dirPath, relPath);
    if (!targetFS) return 0;

    // 2. Abertura do diretório
    File dir = targetFS->open(relPath.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return 0;
    }

    int count = 0;
    File f = dir.openNextFile();

    // 3. Varredura e contagem
    while (f) {
        String name = String(f.name());

        // Garante a extração apenas do nome base (evita caminhos duplicados no FatFs)
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) {
            name = name.substring(lastSlash + 1);
        }

        // Ignora pastas/arquivos ocultos e de sistema
        if (name.length() > 0 && !name.startsWith(".") && name != "System Volume Information") {
            if (!f.isDirectory()) {
                count++;
            } else {
                // Monta o caminho da subpasta mantendo a raiz do caminho original (/sd ou /local)
                String cleanDir = sanitizePath(dirPath);
                String subPath = cleanDir + "/" + name;
                
                // FECHA o handler do arquivo atual ANTES da recursão para economizar descritores no FatFs
                f.close();
                
                count += countFilesInDir(subPath.c_str());
                
                // Abre o próximo arquivo do diretório pai após retornar da recursão
                f = dir.openNextFile();
                continue;
            }
        }

        // Fecha o handler do arquivo/pasta iterado
        f.close();
        f = dir.openNextFile();
    }

    dir.close();
    return count;
}

bool FileSystem::copyDirectory(const char* srcDir, const char* destDir, void (*progressCb)(int current, int total)) {
    if (!srcDir || !destDir) return false;

    // 1. Sanitização dos caminhos de origem e destino
    String cleanSrc = sanitizePath(srcDir);
    String cleanDst = sanitizePath(destDir);

    // Cria o diretório de destino (caso não exista)
    mkdir(cleanDst.c_str());

    // 2. Obtém o sistema de arquivos e o caminho relativo da origem
    String relPath;
    fs::FS* srcFS = getTargetFS(cleanSrc.c_str(), relPath);
    if (!srcFS) return false;

    File dir = srcFS->open(relPath.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return false;
    }

    // 3. Controle seguro de estado para chamadas recursivas
    static int copiedFiles = 0;
    static int totalFiles = 0;
    static int recursionDepth = 0;

    if (recursionDepth == 0) {
        copiedFiles = 0;
        totalFiles = countFilesInDir(cleanSrc.c_str());
        if (totalFiles == 0) totalFiles = 1;
    }

    recursionDepth++; // Incrementa o nível da recursão

    File file = dir.openNextFile();
    while (file) {
        String fileName = String(file.name());

        // Extrai apenas o nome base (evita caminhos pai duplicados do FatFs)
        int lastSlash = fileName.lastIndexOf('/');
        if (lastSlash >= 0) {
            fileName = fileName.substring(lastSlash + 1);
        }

        // Ignora pastas/arquivos ocultos e de sistema
        if (fileName.length() > 0 && !fileName.startsWith(".") && fileName != "System Volume Information") {
            String srcFilePath = cleanSrc + "/" + fileName;
            String dstFilePath = cleanDst + "/" + fileName;

            if (file.isDirectory()) {
                // FECHA o handler do arquivo atual antes da recursão para economizar descritores de arquivo no FatFs
                file.close();

                copyDirectory(srcFilePath.c_str(), dstFilePath.c_str(), progressCb);

                // Abre o próximo arquivo do diretório após o retorno da recursão
                file = dir.openNextFile();
                continue;
            } else {
                copyFile(srcFilePath.c_str(), dstFilePath.c_str());
                copiedFiles++;
                if (progressCb) progressCb(copiedFiles, totalFiles);
                yield(); // Evita acionamento do Watchdog Timer no ESP32
            }
        }

        file.close(); // Libera handler do FatFs
        file = dir.openNextFile();
    }

    dir.close();

    recursionDepth--; // Decrementa ao sair do nível atual
    if (recursionDepth == 0) {
        // Reseta os contadores estáticos ao finalizar a cópia completa
        copiedFiles = 0;
        totalFiles = 0;
    }

    return true;
}

String FileSystem::parseJsonValue(const String& json, const char* key) {
    String searchKey = String("\"" ) + key + "\"";
    int keyIdx = json.indexOf(searchKey);
    if (keyIdx == -1) return "";
    
    int colonIdx = json.indexOf(':', keyIdx + searchKey.length());
    if (colonIdx == -1) return "";
    
    int valStart = colonIdx + 1;
    while (valStart < (int)json.length() && (json[valStart] == ' ' || json[valStart] == '\t')) valStart++;
    
    if (valStart >= (int)json.length()) return "";
    
    if (json[valStart] == '"') {
        int valEnd = json.indexOf('"', valStart + 1);
        if (valEnd == -1) return "";
        return json.substring(valStart + 1, valEnd);
    } else {
        int valEnd = valStart;
        while (valEnd < (int)json.length() && json[valEnd] != ',' && json[valEnd] != '}' && json[valEnd] != '\n') valEnd++;
        String val = json.substring(valStart, valEnd);
        val.trim();
        return val;
    }
}

bool FileSystem::mkdir(const char* path) {
    if (path == nullptr) return false;

    // 1. Obtém o sistema de arquivos e o caminho relativo seguro e sanitizado
    String relPath;
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return false;

    // Se o caminho for a raiz ou vazio, não há diretório a ser criado
    if (relPath.isEmpty() || relPath == "/") return true;

    // 2. Se o diretório final já existe, retorna sucesso imediatamente
    if (targetFS->exists(relPath.c_str())) return true;

    // 3. Criação recursiva de diretórios pai (mkdir -p)
    int start = 1;
    int end = relPath.indexOf('/', start);

    while (end != -1) {
        String currentPath = relPath.substring(0, end);
        if (!currentPath.isEmpty() && !targetFS->exists(currentPath.c_str())) {
            if (!targetFS->mkdir(currentPath.c_str())) {
                return false; // Falha na criação de um diretório pai
            }
        }
        start = end + 1;
        end = relPath.indexOf('/', start);
    }

    // 4. Cria o diretório final
    return targetFS->mkdir(relPath.c_str());
}

bool FileSystem::rmdir(const char* path) {
    FSPath p = resolve(path);
    if (!p.isValid() || p.relPath == "/") return false;
    return p.fs->rmdir(p.relPath.c_str());
}

bool FileSystem::isDirectory(const char* path) {
    return withFile(path, FILE_READ, [](File& f) { return f.isDirectory(); });
}

bool FileSystem::isFile(const char* path) {
    return withFile(path, FILE_READ, [](File& f) { return !f.isDirectory(); });
}

bool FileSystem::appendTextFile(const char* path, const char* content) {
    if (content == nullptr) return false;
    return withFile(path, FILE_APPEND, [content](File& f) {
        size_t written = f.print(content);
        return written > 0 || strlen(content) == 0;
    });
}

bool FileSystem::renameFile(const char* pathFrom, const char* pathTo) {
    FSPath src = resolve(pathFrom);
    FSPath dst = resolve(pathTo);

    // Garante que ambos são válidos e estão no mesmo sistema de arquivos
    if (!src.isValid() || !dst.isValid() || src.fs != dst.fs) return false;

    return src.fs->rename(src.relPath.c_str(), dst.relPath.c_str());
}

size_t FileSystem::getFileSize(const char* path) {
    return withFile(path, FILE_READ, [](File& f) { return f.size(); });
}

time_t FileSystem::getLastModified(const char* path) {
    return withFile(path, FILE_READ, [](File& f) { return f.getLastWrite(); });
}

size_t FileSystem::getTotalSpace(const char* drive) {
    FSPath p = resolve(drive);
    if (!p.isValid()) return 0;
    return (p.fs == &LittleFS) ? LittleFS.totalBytes() : getSDTotalBytes();
}

size_t FileSystem::getUsedSpace(const char* drive) {
    FSPath p = resolve(drive);
    if (!p.isValid()) return 0;
    return (p.fs == &LittleFS) ? LittleFS.usedBytes() : getSDUsedBytes();
}

size_t FileSystem::getFreeSpace(const char* drive) {
    size_t total = getTotalSpace(drive);
    size_t used = getUsedSpace(drive);
    return total > used ? (total - used) : 0;
}

String FileSystem::getFileMD5(const char* path) {
    if (path == nullptr) return "";

    // 1. Obtém o sistema de arquivos e o caminho relativo sanitizado
    String relPath;
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return "";

    // 2. Abertura do arquivo para leitura
    File file = targetFS->open(relPath.c_str(), FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return "";
    }

    // 3. Inicialização do contexto Mbed TLS MD5
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts_ret(&ctx); // OBRIGATÓRIO: Inicializa os registradores do hash

    uint8_t buffer[512];
    size_t len = 0;

    // 4. Leitura em blocos para não sobrecarregar a RAM do ESP32
    while ((len = file.read(buffer, sizeof(buffer))) > 0) {
        mbedtls_md5_update(&ctx, buffer, len);
    }
    file.close();

    // 5. Finaliza o cálculo e gera os 16 bytes do hash
    uint8_t hash[16];
    mbedtls_md5_finish(&ctx, hash);
    mbedtls_md5_free(&ctx);

    // 6. Converte para representação hexadecimal (32 caracteres)
    String hexHash = "";
    hexHash.reserve(32); // Evita fragmentação da Heap alocando espaço antecipadamente

    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02x", hash[i]);
        hexHash += buf;
    }

    return hexHash;
}

bool FileSystem::mountSD() {
        _fs = initSD(); // Atribui o ponteiro para o SD
        return _fs != nullptr;
}

void FileSystem::unmountSD() {
    deinitSD();
    _fs = nullptr;
}

bool FileSystem::formatSD() {
    // A API padrão do Arduino ESP32 não possui suporte nativo direto a formatação FAT em tempo de execução
    return false;
}