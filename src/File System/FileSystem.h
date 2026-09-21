#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <SD.h>

struct FileEntry {
    String name;
    String path;
    bool isDir;
};

struct FSPath {
    fs::FS* fs = nullptr;
    String relPath = "";
    
    bool isValid() const { return fs != nullptr; }
};

class FileSystem {
public:
    static bool init();
    static String readTextFile(const char* path);
    static bool writeTextFile(const char* path, const char* content);
    static bool exists(const char* path);
    static int listDir(const char* dirPath, String* resultFiles, int maxFiles);
    static int listDirectory(const char* dirPath, FileEntry* entries, int maxEntries);
    static bool copyFile(const char* srcPath, const char* dstPath);
    static bool copyDirectory(const char* srcDir, const char* destDir, void (*progressCb)(int current, int total) = nullptr);
    static int countFilesInDir(const char* dirPath);
    static String parseJsonValue(const String& json, const char* key);
    static bool deleteFile(const char* path);
    static bool formatLittleFS();
    static bool readCalData(uint16_t* calData);
    static bool writeCalData(uint16_t* calData);
    
    // Directory Operations
    static bool mkdir(const char* path);
    static bool rmdir(const char* path);
    static bool isDirectory(const char* path);
    static bool isFile(const char* path);
    
    // Advanced File Operations
    static bool appendTextFile(const char* path, const char* content);
    static bool renameFile(const char* pathFrom, const char* pathTo);
    static size_t getFileSize(const char* path);
    static time_t getLastModified(const char* path);
    
    // Metrics
    static size_t getTotalSpace(const char* drive);
    static size_t getUsedSpace(const char* drive);
    static size_t getFreeSpace(const char* drive);
    
    // Cryptography
    static String getFileMD5(const char* path);

    static size_t readBinaryFile(const char* path, uint8_t* buffer, size_t maxLen);
    static bool writeBinaryFile(const char* path, const uint8_t* data, size_t len);
    
    // Mounting/Formatting
    static bool mountSD();
    static void unmountSD();
    static bool formatSD();

private:
    static fs::FS* _fs;
    static FSPath resolve(const char* path);
    static String sanitizePath(const char* path);
    static fs::FS* getTargetFS(const char* path, String& relPath);

    template <typename Func>
    static auto withFile(const char* path, const char* mode, Func action) 
        -> decltype(action(std::declval<File&>()));
};

template <typename Func>
auto FileSystem::withFile(const char* path, const char* mode, Func action) 
    -> decltype(action(std::declval<File&>())) 
{
    using ReturnType = decltype(action(std::declval<File&>()));
    
    FSPath target = resolve(path);
    if (!target.isValid()) return ReturnType{};

    File file = target.fs->open(target.relPath.c_str(), mode);
    if (!file) return ReturnType{};

    ReturnType result = action(file);
    file.close();
    return result;
}

#endif // FILE_SYSTEM_H
