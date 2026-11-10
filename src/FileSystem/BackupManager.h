#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <functional>

enum BackupMode {
    BACKUP_MODE_DEVICE = 0,    // Device-Locked (using hardware NVS key)
    BACKUP_MODE_PASSPHRASE = 1 // Portable (using user passphrase + PBKDF2)
};

enum BackupResult {
    BACKUP_SUCCESS = 0,
    BACKUP_ERR_NO_SD,
    BACKUP_ERR_SD_WRITE,
    BACKUP_ERR_FILE_NOT_FOUND,
    BACKUP_ERR_INVALID_MAGIC,
    BACKUP_ERR_CORRUPT_HEADER,
    BACKUP_ERR_MANIFEST_HMAC,
    BACKUP_ERR_JSON_PARSE,
    BACKUP_ERR_KEY_DERIVATION,
    BACKUP_ERR_INSUFFICIENT_STORAGE,
    BACKUP_ERR_SECURITY_VIOLATION,
    BACKUP_ERR_FILE_INTEGRITY,
    BACKUP_ERR_NO_FILES,
    BACKUP_ERR_WRONG_PASSWORD
};

struct BackupInfo {
    String filename;
    String fullPath;
    String dateStr;
    time_t timestamp;
    String board;
    String osVersion;
    BackupMode mode;
    size_t fileCount;
    size_t totalBytes;
    bool isDeviceMatch;
};

typedef std::function<void(int current, int total, const String& filename)> BackupProgressCb;

class BackupManager {
public:
    static BackupResult createBackup(BackupMode mode, const String& passphrase = "", BackupProgressCb progress = nullptr);
    static int scanBackups(std::vector<BackupInfo>& outList);
    static bool getLatestBackup(BackupInfo& outInfo);
    static BackupResult restoreBackup(const String& kbfPath, const String& passphrase = "", BackupProgressCb progress = nullptr);
    static bool deleteBackup(const String& kbfPath);
    
    // Path security & filesystem safety
    static bool isSafePath(const String& path);
    static void ensureParentDirsExist(const String& path);
    
    // Reboot helper
    static void executeAtomicReboot(int countdownSec = 3);
};

#endif // BACKUP_MANAGER_H

