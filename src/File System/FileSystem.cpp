#include "FileSystem.h"

SPIClass sdSPI(HSPI);

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

    // Initialize dedicated SPI bus for SD Card (HSPI pins: SCK=14, MISO=26, MOSI=13, CS=15)
    sdSPI.begin(14, 26, 13, 15);
    
    // Initialize SD Card
    if (!SD.begin(15, sdSPI, 4000000)) {
        Serial.println("SD Card Mount Failed");
        success = false;
    } else {
        Serial.println("SD Card Mount Successful");
    }

    return success;
}

String FileSystem::readTextFile(const char* path) {
    if (path == nullptr) return "";
    
    fs::FS* targetFS = nullptr;
    const char* relPath = "";
    if (strncmp(path, "/sd/", 4) == 0) {
        targetFS = &SD;
        relPath = path + 3;
    } else if (strncmp(path, "/local/", 7) == 0) {
        targetFS = &LittleFS;
        relPath = path + 6;
    } else {
        return ""; // Unknown prefix
    }
    
    if (strlen(relPath) == 0) relPath = "/";
    
    File file = targetFS->open(relPath);
    if (!file || file.isDirectory()) {
        return "";
    }
    
    size_t size = file.size();
    if (size == 0) {
        file.close();
        return "";
    }
    
    // Pre-allocate String to prevent massive heap fragmentation
    String content;
    if (!content.reserve(size)) {
        Serial.println("Memory allocation failed for reading file.");
        file.close();
        return "";
    }
    
    // Read in 512-byte chunks
    uint8_t buffer[512];
    while (file.available()) {
        size_t bytesRead = file.read(buffer, sizeof(buffer));
        for (size_t i = 0; i < bytesRead; i++) {
            content += (char)buffer[i];
        }
    }
    
    file.close();
    return content;
}

bool FileSystem::writeTextFile(const char* path, const char* content) {
    if (path == nullptr || content == nullptr) return false;
    
    fs::FS* targetFS = nullptr;
    const char* relPath = "";
    if (strncmp(path, "/sd/", 4) == 0) {
        targetFS = &SD;
        relPath = path + 3;
    } else if (strncmp(path, "/local/", 7) == 0) {
        targetFS = &LittleFS;
        relPath = path + 6;
    } else {
        return false;
    }
    
    if (strlen(relPath) == 0) relPath = "/";
    
    File file = targetFS->open(relPath, FILE_WRITE);
    if (!file) {
        return false;
    }
    
    if (file.print(content)) {
        file.close();
        return true;
    } else {
        file.close();
        return false;
    }
}

bool FileSystem::exists(const char* path) {
    if (path == nullptr) return false;
    
    if (strncmp(path, "/sd/", 4) == 0) {
        return SD.exists(path + 3);
    } else if (strncmp(path, "/local/", 7) == 0) {
        return LittleFS.exists(path + 6);
    }
    return false;
}

bool FileSystem::deleteFile(const char* path) {
    if (path == nullptr) return false;
    
    fs::FS* targetFS = nullptr;
    const char* relPath = "";
    if (strncmp(path, "/sd/", 4) == 0) {
        targetFS = &SD;
        relPath = path + 3;
    } else if (strncmp(path, "/local/", 7) == 0) {
        targetFS = &LittleFS;
        relPath = path + 6;
    } else {
        return false;
    }
    
    if (strlen(relPath) == 0) return false;
    return targetFS->remove(relPath);
}

bool FileSystem::formatLittleFS() {
    Serial.println("Formatting LittleFS...");
    return LittleFS.format();
}

int FileSystem::listDir(const char* dirPath, String* resultFiles, int maxFiles) {
    if (dirPath == nullptr || resultFiles == nullptr) return 0;
    
    fs::FS* targetFS = nullptr;
    const char* relativePath = "";
    if (strncmp(dirPath, "/sd", 3) == 0) {
        targetFS = &SD;
        relativePath = dirPath + 3; // e.g. "" or "/" or "/apps"
        if (strlen(relativePath) == 0) relativePath = "/";
    } else if (strncmp(dirPath, "/local", 6) == 0) {
        targetFS = &LittleFS;
        relativePath = dirPath + 6;
        if (strlen(relativePath) == 0) relativePath = "/";
    } else {
        return 0;
    }
    
    File dir = targetFS->open(relativePath);
    if (!dir || !dir.isDirectory()) {
        return 0;
    }

    int count = 0;
    File file = dir.openNextFile();
    while (file && count < maxFiles) {
        String name = file.name();
        // Return full absolute path e.g. /sd/apps/file.js
        String fullPath = String(dirPath);
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += name;
        resultFiles[count++] = fullPath;
        
        file = dir.openNextFile();
    }
    return count;
}

int FileSystem::listDirectory(const char* dirPath, FileEntry* entries, int maxEntries) {
    fs::FS* targetFS = nullptr;
    const char* relativePath = "";

    if (strncmp(dirPath, "/sd", 3) == 0) {
        targetFS = &SD;
        relativePath = dirPath + 3; 
        if (strlen(relativePath) == 0) relativePath = "/";
    } else if (strncmp(dirPath, "/local", 6) == 0) {
        targetFS = &LittleFS;
        relativePath = dirPath + 6;
        if (strlen(relativePath) == 0) relativePath = "/";
    } else {
        return 0;
    }
    
    File dir = targetFS->open(relativePath);
    if (!dir || !dir.isDirectory()) {
        return 0;
    }

    int count = 0;
    File file = dir.openNextFile();
    while (file && count < maxEntries) {
        entries[count].name = String(file.name());
        
        String fullPath = String(dirPath);
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += entries[count].name;
        
        entries[count].path = fullPath;
        entries[count].isDir = file.isDirectory();
        
        count++;
        file = dir.openNextFile();
    }
    return count;
}

bool FileSystem::readCalData(uint16_t* calData) {
    if (!LittleFS.exists("/touch_cal_p.bin")) return false;
    File f = LittleFS.open("/touch_cal_p.bin", FILE_READ);
    if (!f) return false;
    if (f.read((uint8_t*)calData, 10) == 10) {
        f.close();
        return true;
    }
    f.close();
    return false;
}

bool FileSystem::writeCalData(uint16_t* calData) {
    File f = LittleFS.open("/touch_cal_p.bin", FILE_WRITE);
    if (!f) return false;
    f.write((uint8_t*)calData, 10);
    f.close();
    return true;
}

bool FileSystem::copyFile(const char* srcPath, const char* dstPath) {
    if (srcPath == nullptr || dstPath == nullptr) return false;
    
    fs::FS* srcFS = nullptr;
    const char* srcRel = "";
    if (strncmp(srcPath, "/sd/", 4) == 0) {
        srcFS = &SD; srcRel = srcPath + 3;
    } else if (strncmp(srcPath, "/local/", 7) == 0) {
        srcFS = &LittleFS; srcRel = srcPath + 6;
    } else return false;
    
    fs::FS* dstFS = nullptr;
    const char* dstRel = "";
    if (strncmp(dstPath, "/sd/", 4) == 0) {
        dstFS = &SD; dstRel = dstPath + 3;
    } else if (strncmp(dstPath, "/local/", 7) == 0) {
        dstFS = &LittleFS; dstRel = dstPath + 6;
    } else return false;

    if (strlen(srcRel) == 0) srcRel = "/";
    if (strlen(dstRel) == 0) dstRel = "/";

    File srcFile = srcFS->open(srcRel, FILE_READ);
    if (!srcFile || srcFile.isDirectory()) return false;
    
    File dstFile = dstFS->open(dstRel, FILE_WRITE);
    if (!dstFile) {
        srcFile.close();
        return false;
    }
    
    size_t n;
    uint8_t buf[512];
    while ((n = srcFile.read(buf, sizeof(buf))) > 0) {
        dstFile.write(buf, n);
    }
    
    srcFile.close();
    dstFile.close();
    return true;
}



#include <mbedtls/md5.h>

static fs::FS* getTargetFS(const char* path, const char*& relPath) {
    if (path == nullptr) return nullptr;
    if (strncmp(path, "/sd", 3) == 0) {
        relPath = path + 3;
        if (strlen(relPath) == 0) relPath = "/";
        return &SD;
    } else if (strncmp(path, "/local", 6) == 0) {
        relPath = path + 6;
        if (strlen(relPath) == 0) relPath = "/";
        return &LittleFS;
    }
    return nullptr;
}

int FileSystem::countFilesInDir(const char* dirPath) {
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(dirPath, relPath);
    if (!targetFS) return 0;
    File dir = targetFS->open(relPath);
    if (!dir || !dir.isDirectory()) return 0;
    int count = 0;
    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            count++;
        } else {
            String subPath = String(dirPath);
            if (!subPath.endsWith("/")) subPath += "/";
            subPath += f.name();
            count += countFilesInDir(subPath.c_str());
        }
        f = dir.openNextFile();
    }
    return count;
}

bool FileSystem::copyDirectory(const char* srcDir, const char* destDir, void (*progressCb)(int current, int total)) {
    if (!srcDir || !destDir) return false;
    mkdir(destDir);
    
    const char* relPath = "";
    fs::FS* srcFS = getTargetFS(srcDir, relPath);
    if (!srcFS) return false;
    
    File dir = srcFS->open(relPath);
    if (!dir || !dir.isDirectory()) return false;
    
    static int copiedFiles = 0;
    static int totalFiles = 0;
    static bool isTopLevel = true;
    
    if (isTopLevel) {
        copiedFiles = 0;
        totalFiles = countFilesInDir(srcDir);
        if (totalFiles == 0) totalFiles = 1;
        isTopLevel = false;
    }
    
    File file = dir.openNextFile();
    while (file) {
        String fileName = file.name();
        String srcFilePath = String(srcDir);
        if (!srcFilePath.endsWith("/")) srcFilePath += "/";
        srcFilePath += fileName;
        
        String dstFilePath = String(destDir);
        if (!dstFilePath.endsWith("/")) dstFilePath += "/";
        dstFilePath += fileName;
        
        if (file.isDirectory()) {
            bool wasTopLevel = isTopLevel;
            isTopLevel = false;
            copyDirectory(srcFilePath.c_str(), dstFilePath.c_str(), progressCb);
            isTopLevel = wasTopLevel;
        } else {
            copyFile(srcFilePath.c_str(), dstFilePath.c_str());
            copiedFiles++;
            if (progressCb) progressCb(copiedFiles, totalFiles);
            yield();
        }
        file = dir.openNextFile();
    }
    
    isTopLevel = true;
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
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return false;
    return targetFS->mkdir(relPath);
}

bool FileSystem::rmdir(const char* path) {
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return false;
    return targetFS->rmdir(relPath);
}

bool FileSystem::isDirectory(const char* path) {
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return false;
    File file = targetFS->open(relPath);
    if (!file) return false;
    bool isDir = file.isDirectory();
    file.close();
    return isDir;
}

bool FileSystem::isFile(const char* path) {
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return false;
    File file = targetFS->open(relPath);
    if (!file) return false;
    bool isFile = !file.isDirectory();
    file.close();
    return isFile;
}

bool FileSystem::appendTextFile(const char* path, const char* content) {
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return false;
    File file = targetFS->open(relPath, FILE_APPEND);
    if (!file) return false;
    bool res = file.print(content);
    file.close();
    return res;
}

bool FileSystem::renameFile(const char* pathFrom, const char* pathTo) {
    const char* relPathFrom = "";
    fs::FS* targetFSFrom = getTargetFS(pathFrom, relPathFrom);
    const char* relPathTo = "";
    fs::FS* targetFSTo = getTargetFS(pathTo, relPathTo);
    
    // Cannot rename across different file systems natively via targetFS->rename
    if (!targetFSFrom || !targetFSTo || targetFSFrom != targetFSTo) return false;
    
    return targetFSFrom->rename(relPathFrom, relPathTo);
}

size_t FileSystem::getFileSize(const char* path) {
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return 0;
    File file = targetFS->open(relPath);
    if (!file) return 0;
    size_t size = file.size();
    file.close();
    return size;
}

time_t FileSystem::getLastModified(const char* path) {
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return 0;
    File file = targetFS->open(relPath);
    if (!file) return 0;
    time_t mod = file.getLastWrite();
    file.close();
    return mod;
}

size_t FileSystem::getTotalSpace(const char* drive) {
    if (strncmp(drive, "/sd", 3) == 0) return SD.totalBytes();
    if (strncmp(drive, "/local", 6) == 0) return LittleFS.totalBytes();
    return 0;
}

size_t FileSystem::getUsedSpace(const char* drive) {
    if (strncmp(drive, "/sd", 3) == 0) return SD.usedBytes();
    if (strncmp(drive, "/local", 6) == 0) return LittleFS.usedBytes();
    return 0;
}

size_t FileSystem::getFreeSpace(const char* drive) {
    size_t total = getTotalSpace(drive);
    size_t used = getUsedSpace(drive);
    return total > used ? (total - used) : 0;
}

String FileSystem::getFileMD5(const char* path) {
    const char* relPath = "";
    fs::FS* targetFS = getTargetFS(path, relPath);
    if (!targetFS) return "";
    File file = targetFS->open(relPath, FILE_READ);
    if (!file || file.isDirectory()) return "";

    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);

    uint8_t buffer[512];
    size_t len;
    while ((len = file.read(buffer, sizeof(buffer))) > 0) {
        mbedtls_md5_update(&ctx, buffer, len);
    }
    file.close();

    uint8_t hash[16];
    mbedtls_md5_finish(&ctx, hash);
    mbedtls_md5_free(&ctx);

    String hexHash = "";
    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02x", hash[i]);
        hexHash += buf;
    }
    return hexHash;
}

bool FileSystem::mountSD() {
    return SD.begin(SD_CS_PIN, sdSPI, 4000000);
}

void FileSystem::unmountSD() {
    SD.end();
}

bool FileSystem::formatSD() {
    return false; // Not natively supported on standard Arduino core without custom FAT commands
}
