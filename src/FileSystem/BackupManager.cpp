#include "BackupManager.h"
#include "FileSystem.h"
#include "../Hal/Crypto/CryptoEngine.h"
#include "../Hal/Boards/Board.h"
#include <LittleFS.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

static const char* MAGIC_HEADER = "KRYON_BKP_01"; // 12 bytes exactly

static uint8_t* allocStreamBuffer(size_t& bufferSize) {
    uint8_t* buf = nullptr;
#if defined(CONFIG_SPIRAM) || defined(BOARD_HAS_PSRAM)
    buf = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf) { bufferSize = 4096; return buf; }
#endif
    buf = (uint8_t*)heap_caps_malloc(2048, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (buf) { bufferSize = 2048; return buf; }
    bufferSize = 1024;
    return (uint8_t*)malloc(bufferSize);
}

static void freeStreamBuffer(uint8_t* buf) {
    if (buf) free(buf);
}

bool BackupManager::isSafePath(const String& rawPath) {
    if (rawPath.length() == 0 || rawPath.length() > 256) return false;

    // Prevent null-byte injection (mismatch between String length and C-string length)
    if (rawPath.length() != strlen(rawPath.c_str())) return false;

    // Normalize path by stripping leading slashes and dot-slashes
    String path = rawPath;
    while (path.startsWith("/") || path.startsWith("\\")) {
        path = path.substring(1);
    }
    while (path.startsWith("./") || path.startsWith(".\\")) {
        path = path.substring(2);
    }
    if (path.length() == 0) return false;

    // Check for directory traversal sequences (".." segments)
    if (path == ".." || path.startsWith("../") || path.startsWith("..\\") ||
        path.indexOf("/../") != -1 || path.indexOf("\\..\\") != -1 ||
        path.indexOf("/..\\") != -1 || path.indexOf("\\../") != -1 ||
        path.endsWith("/..") || path.endsWith("\\..")) {
        return false;
    }

    // Prevent drive letter traversal like "C:" or "D:"
    if (path.length() >= 2 && path.charAt(1) == ':') return false;

    return true;
}

void BackupManager::ensureParentDirsExist(const String& path) {
    int slashIdx = path.lastIndexOf('/');
    if (slashIdx <= 0) return;

    String dir = path.substring(0, slashIdx);
    int start = 0;
    while (start < dir.length()) {
        int nextSlash = dir.indexOf('/', start + 1);
        String sub = (nextSlash == -1) ? dir : dir.substring(0, nextSlash);
        if (!sub.startsWith("/")) sub = "/" + sub;
        if (!LittleFS.exists(sub.c_str())) {
            LittleFS.mkdir(sub.c_str());
        }
        if (nextSlash == -1) break;
        start = nextSlash;
    }
}

static void collectFilesRecursive(fs::FS& fs, const String& dirPath, std::vector<String>& files) {
    File dir = fs.open(dirPath);
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();
    while (file) {
        String path = file.path();
        if (file.isDirectory()) {
            if (path != "/tmp_download" && !path.endsWith("/tmp_download")) {
                collectFilesRecursive(fs, path, files);
            }
        } else {
            // Strip leading '/'
            String rel = path;
            while (rel.startsWith("/")) rel = rel.substring(1);
            if (rel.length() > 0 && !rel.startsWith("tmp_download/")) {
                files.push_back(rel);
            }
        }
        file = dir.openNextFile();
    }
}

static void computeManifestHmac(const uint8_t k_mac[32], const char* magic, uint32_t len, const uint8_t* jsonBytes, uint8_t outHmac[32]) {
    mbedtls_md_context_t mdCtx;
    mbedtls_md_init(&mdCtx);
    mbedtls_md_setup(&mdCtx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&mdCtx, k_mac, 32);
    mbedtls_md_hmac_update(&mdCtx, (const unsigned char*)magic, 12);
    mbedtls_md_hmac_update(&mdCtx, (const unsigned char*)&len, 4);
    mbedtls_md_hmac_update(&mdCtx, jsonBytes, len);
    mbedtls_md_hmac_finish(&mdCtx, outHmac);
    mbedtls_md_free(&mdCtx);
}

static String getDeviceFingerprint() {
    uint8_t devKey[32];
    if (!CryptoEngine::getDeviceKey(devKey)) return "unknown";
    uint8_t hash[32];
    CryptoEngine::sha256Raw(devKey, 32, hash);
    return CryptoEngine::bytesToHex(hash, 8); // 16 hex chars
}

static String getLegacyDeviceFingerprint() {
    uint8_t devKey[32];
    if (!CryptoEngine::getLegacyNVSKey(devKey)) return "";
    uint8_t hash[32];
    CryptoEngine::sha256Raw(devKey, 32, hash);
    return CryptoEngine::bytesToHex(hash, 8); // 16 hex chars
}

BackupResult BackupManager::createBackup(BackupMode mode, const String& passphrase, BackupProgressCb progress) {
    if (!FileSystem::isSDMounted()) {
        if (!FileSystem::mountSD()) return BACKUP_ERR_NO_SD;
    }

    if (!SD.exists("/backups")) {
        SD.mkdir("/backups");
    }

    std::vector<String> files;
    collectFilesRecursive(LittleFS, "/", files);
    if (files.empty()) return BACKUP_ERR_NO_FILES;

    // Master Key & Subkeys Derivation
    uint8_t masterKey[32];
    uint8_t salt[16] = {0};
    String saltHex = "";

    if (mode == BACKUP_MODE_DEVICE) {
        if (!CryptoEngine::getDeviceKey(masterKey)) {
            return BACKUP_ERR_KEY_DERIVATION;
        }
    } else {
        esp_fill_random(salt, sizeof(salt));
        saltHex = CryptoEngine::bytesToHex(salt, sizeof(salt));
        if (!CryptoEngine::pbkdf2HmacSha256(passphrase, salt, sizeof(salt), 10000, masterKey)) {
            return BACKUP_ERR_KEY_DERIVATION;
        }
    }

    uint8_t k_enc[32], k_mac[32];
    CryptoEngine::deriveBackupSubkeys(masterKey, k_enc, k_mac);

    // Build Manifest JSON
    JsonDocument doc;
    doc["version"] = 1;
    doc["mode"] = (mode == BACKUP_MODE_DEVICE) ? "device" : "passphrase";
    if (mode == BACKUP_MODE_PASSPHRASE) {
        doc["salt"] = saltHex;
    }
    
    time_t now = time(nullptr);
    doc["timestamp"] = (uint32_t)now;

    char dateBuf[32];
    if (now > 100000) {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        snprintf(dateBuf, sizeof(dateBuf), "KryonOS Boot Session");
    }
    doc["date_str"] = dateBuf;

#if defined(LILYGO_T_DECK) || defined(BOARD_HAS_KEYBOARD)
    doc["board"] = "LilyGO T-Deck Plus";
#elif defined(CYD_ESP32_2432S028R)
    doc["board"] = "ESP32-2432S028R";
#else
    doc["board"] = "ESP32";
#endif
    doc["os_version"] = "3.0.0";
    doc["api_level"] = 3;
    doc["device_fingerprint"] = getDeviceFingerprint();
    doc["file_count"] = (int)files.size();

    size_t totalUncompressed = 0;
    JsonArray entries = doc["entries"].to<JsonArray>();

    size_t currentOffset = 0;
    for (const String& path : files) {
        JsonObject entry = entries.add<JsonObject>();
        entry["path"] = path;
        
        bool isEnc = path.startsWith("system/") || FileSystem::isSystemPath(("/" + path).c_str());
        entry["enc"] = isEnc;

        File f = LittleFS.open("/" + path, "r");
        size_t origSize = f ? f.size() : 0;
        if (f) f.close();

        entry["orig_size"] = origSize;
        totalUncompressed += origSize;

        if (isEnc) {
            size_t padLen = 16 - (origSize % 16);
            size_t cipherSize = origSize + padLen;
            entry["ciphertext_size"] = cipherSize;
            size_t storedSpan = 16 + cipherSize + 32; // 16B IV + Cipher + 32B HMAC
            entry["stored_span"] = storedSpan;
            entry["offset"] = currentOffset;
            currentOffset += storedSpan;
        } else {
            size_t storedSpan = origSize + 32; // Raw bytes + 32B SHA-256
            entry["stored_span"] = storedSpan;
            entry["offset"] = currentOffset;
            currentOffset += storedSpan;
        }
    }
    doc["total_uncompressed_bytes"] = totalUncompressed;

    String manifestJson;
    serializeJson(doc, manifestJson);
    uint32_t manifestLen = manifestJson.length();

    uint8_t manifestHmac[32];
    computeManifestHmac(k_mac, MAGIC_HEADER, manifestLen, (const uint8_t*)manifestJson.c_str(), manifestHmac);

    // Generate output file names
    char filenameBuf[64];
    if (now > 100000) {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(filenameBuf, sizeof(filenameBuf), "/backups/kryon_backup_%Y%m%d_%H%M%S.kbf", &timeinfo);
    } else {
        snprintf(filenameBuf, sizeof(filenameBuf), "/backups/kryon_backup_%lu.kbf", (unsigned long)millis());
    }

    const char* tmpPath = "/backups/kryon_backup.tmp";
    if (SD.exists(tmpPath)) SD.remove(tmpPath);

    File out = SD.open(tmpPath, FILE_WRITE);
    if (!out) return BACKUP_ERR_SD_WRITE;

    // 1. Magic Identifier (12 bytes)
    out.write((const uint8_t*)MAGIC_HEADER, 12);
    // 2. Manifest Length (4 bytes uint32_t)
    out.write((const uint8_t*)&manifestLen, 4);
    // 3. Manifest HMAC (32 bytes)
    out.write(manifestHmac, 32);
    // 4. Manifest JSON (manifestLen bytes)
    out.write((const uint8_t*)manifestJson.c_str(), manifestLen);

    // 5. Data Payload Stream
    size_t streamBufSize = 4096;
    uint8_t* streamBuffer = allocStreamBuffer(streamBufSize);
    if (!streamBuffer) {
        out.close();
        SD.remove(tmpPath);
        return BACKUP_ERR_INSUFFICIENT_STORAGE;
    }

    int fileIdx = 0;
    int totalFiles = files.size();

    for (const String& path : files) {
        if (progress) progress(fileIdx++, totalFiles, path);
        esp_task_wdt_reset();

        bool isEnc = path.startsWith("system/") || FileSystem::isSystemPath(("/" + path).c_str());
        File in = LittleFS.open("/" + path, "r");
        if (!in) continue;

        size_t fileSize = in.size();

        if (isEnc) {
            // Read full system file into memory
            std::vector<uint8_t> plain(fileSize);
            if (fileSize > 0) in.read(plain.data(), fileSize);
            in.close();

            // 16-byte random IV
            uint8_t iv[16];
            esp_fill_random(iv, 16);

            // AES-256-CBC Encrypt
            std::vector<uint8_t> cipher;
            if (!CryptoEngine::aesCbcEncryptRaw(plain.data(), fileSize, k_enc, iv, cipher)) {
                freeStreamBuffer(streamBuffer);
                out.close();
                SD.remove(tmpPath);
                return BACKUP_ERR_FILE_INTEGRITY;
            }

            // HMAC-SHA256 of IV || Ciphertext
            uint8_t fileHmac[32];
            mbedtls_md_context_t mdCtx;
            mbedtls_md_init(&mdCtx);
            mbedtls_md_setup(&mdCtx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
            mbedtls_md_hmac_starts(&mdCtx, k_mac, 32);
            mbedtls_md_hmac_update(&mdCtx, iv, 16);
            mbedtls_md_hmac_update(&mdCtx, cipher.data(), cipher.size());
            mbedtls_md_hmac_finish(&mdCtx, fileHmac);
            mbedtls_md_free(&mdCtx);

            // Write IV + Ciphertext + HMAC
            out.write(iv, 16);
            out.write(cipher.data(), cipher.size());
            out.write(fileHmac, 32);
        } else {
            // Plaintext Stream with SHA-256 Checksum
            mbedtls_sha256_context shaCtx;
            mbedtls_sha256_init(&shaCtx);
            mbedtls_sha256_starts(&shaCtx, 0);

            while (in.available()) {
                size_t toRead = std::min((size_t)in.available(), streamBufSize);
                size_t n = in.read(streamBuffer, toRead);
                if (n > 0) {
                    out.write(streamBuffer, n);
                    mbedtls_sha256_update(&shaCtx, streamBuffer, n);
                }
            }
            in.close();

            uint8_t fileHash[32];
            mbedtls_sha256_finish(&shaCtx, fileHash);
            mbedtls_sha256_free(&shaCtx);

            out.write(fileHash, 32);
        }
        out.flush();
    }

    freeStreamBuffer(streamBuffer);
    out.close();

    if (SD.exists(filenameBuf)) SD.remove(filenameBuf);
    SD.rename(tmpPath, filenameBuf);

    // Also update "kryon_backup_latest.kbf" pointer
    const char* latestPath = "/backups/kryon_backup_latest.kbf";
    if (SD.exists(latestPath)) SD.remove(latestPath);
    FileSystem::copyFile(filenameBuf, latestPath);

    if (progress) progress(totalFiles, totalFiles, "Done!");
    return BACKUP_SUCCESS;
}

int BackupManager::scanBackups(std::vector<BackupInfo>& outList) {
    outList.clear();
    if (!FileSystem::isSDMounted()) {
        if (!FileSystem::mountSD()) return 0;
    }

    String myFingerprint = getDeviceFingerprint();
    String myLegacyFingerprint = getLegacyDeviceFingerprint();

    auto scanDir = [&](const char* dirPath) {
        File dir = SD.open(dirPath);
        if (!dir || !dir.isDirectory()) return;

        File f = dir.openNextFile();
        while (f) {
            String name = f.name();
            if (!f.isDirectory() && name.endsWith(".kbf") && !name.endsWith("latest.kbf")) {
                String fullPath = String(dirPath);
                if (!fullPath.endsWith("/")) fullPath += "/";
                fullPath += name;

                File bkp = SD.open(fullPath, "r");
                if (bkp) {
                    char magic[13] = {0};
                    bkp.read((uint8_t*)magic, 12);
                    if (strcmp(magic, MAGIC_HEADER) == 0) {
                        uint32_t manifestLen = 0;
                        bkp.read((uint8_t*)&manifestLen, 4);

                        if (manifestLen > 0 && manifestLen <= 65536) {
                            bkp.seek(12 + 4 + 32); // skip magic + len + hmac to manifest json
                            std::vector<uint8_t> mBuf(manifestLen + 1);
                            bkp.read(mBuf.data(), manifestLen);
                            mBuf[manifestLen] = '\0';

                            JsonDocument doc;
                            if (!deserializeJson(doc, (const char*)mBuf.data())) {
                                BackupInfo info;
                                info.filename = name;
                                info.fullPath = fullPath;
                                info.dateStr = doc["date_str"] | "Unknown Date";
                                info.timestamp = doc["timestamp"] | 0;
                                info.board = doc["board"] | "Unknown Board";
                                info.osVersion = doc["os_version"] | "3.0.0";
                                const char* mStr = doc["mode"] | "device";
                                info.mode = (strcmp(mStr, "passphrase") == 0) ? BACKUP_MODE_PASSPHRASE : BACKUP_MODE_DEVICE;
                                info.fileCount = doc["file_count"] | 0;
                                info.totalBytes = doc["total_uncompressed_bytes"] | 0;

                                String devFp = doc["device_fingerprint"] | "";
                                info.isDeviceMatch = (devFp == myFingerprint || (!myLegacyFingerprint.isEmpty() && devFp == myLegacyFingerprint));

                                outList.push_back(info);
                            }
                        }
                    }
                    bkp.close();
                }
            }
            f = dir.openNextFile();
        }
    };

    scanDir("/backups");
    if (outList.empty()) scanDir("/");

    // Sort by timestamp descending (newest first). If timestamps match, sort by filename descending.
    std::sort(outList.begin(), outList.end(), [](const BackupInfo& a, const BackupInfo& b) {
        if (a.timestamp != b.timestamp) return a.timestamp > b.timestamp;
        return a.filename > b.filename;
    });

    return (int)outList.size();
}

bool BackupManager::getLatestBackup(BackupInfo& outInfo) {
    std::vector<BackupInfo> list;
    if (scanBackups(list) > 0) {
        outInfo = list[0];
        return true;
    }
    return false;
}

bool BackupManager::deleteBackup(const String& kbfPath) {
    if (!FileSystem::isSDMounted()) {
        if (!FileSystem::mountSD()) return false;
    }
    if (kbfPath.isEmpty() || !kbfPath.endsWith(".kbf")) return false;
    if (SD.exists(kbfPath)) {
        return SD.remove(kbfPath);
    }
    return false;
}

BackupResult BackupManager::restoreBackup(const String& kbfPath, const String& passphrase, BackupProgressCb progress) {
    if (!FileSystem::isSDMounted()) {
        if (!FileSystem::mountSD()) return BACKUP_ERR_NO_SD;
    }

    File bkp = SD.open(kbfPath, "r");
    if (!bkp) return BACKUP_ERR_FILE_NOT_FOUND;

    // 1. Read & verify Magic Header (12 bytes)
    char magic[13] = {0};
    bkp.read((uint8_t*)magic, 12);
    if (strcmp(magic, MAGIC_HEADER) != 0) {
        bkp.close();
        return BACKUP_ERR_INVALID_MAGIC;
    }

    // 2. Read Manifest Length (4 bytes) & Stored Manifest HMAC (32 bytes)
    uint32_t manifestLen = 0;
    bkp.read((uint8_t*)&manifestLen, 4);
    if (manifestLen == 0 || manifestLen > 65536) {
        bkp.close();
        return BACKUP_ERR_CORRUPT_HEADER;
    }

    uint8_t storedManifestHmac[32];
    bkp.read(storedManifestHmac, 32);

    // 3. Read Manifest JSON
    std::vector<uint8_t> mBuf(manifestLen + 1);
    bkp.read(mBuf.data(), manifestLen);
    mBuf[manifestLen] = '\0';

    // 4. Parse JSON
    JsonDocument doc;
    DeserializationError jsonErr = deserializeJson(doc, (const char*)mBuf.data());
    if (jsonErr) {
        bkp.close();
        return BACKUP_ERR_JSON_PARSE;
    }

    // 5. Derive Master Key & Subkeys
    const char* modeStr = doc["mode"] | "device";
    bool isPassphraseMode = (strcmp(modeStr, "passphrase") == 0);

    if (isPassphraseMode && passphrase.length() == 0) {
        bkp.close();
        return BACKUP_ERR_WRONG_PASSWORD;
    }

    uint8_t masterKey[32];
    uint8_t k_enc[32], k_mac[32];
    uint8_t calcHmac[32];
    bool verified = false;

    if (isPassphraseMode) {
        String saltHex = doc["salt"] | "";
        std::vector<uint8_t> saltBytes;
        if (!CryptoEngine::hexToBytes(saltHex, saltBytes) || saltBytes.size() != 16) {
            bkp.close();
            return BACKUP_ERR_KEY_DERIVATION;
        }
        if (!CryptoEngine::pbkdf2HmacSha256(passphrase, saltBytes.data(), saltBytes.size(), 10000, masterKey)) {
            bkp.close();
            return BACKUP_ERR_KEY_DERIVATION;
        }
        CryptoEngine::deriveBackupSubkeys(masterKey, k_enc, k_mac);
        computeManifestHmac(k_mac, MAGIC_HEADER, manifestLen, mBuf.data(), calcHmac);
        if (memcmp(calcHmac, storedManifestHmac, 32) == 0) {
            verified = true;
        }
    } else {
        // 1. Try Hardware Silicon Root Key (permanent, survives full flash erase)
        if (CryptoEngine::getDeviceKey(masterKey)) {
            CryptoEngine::deriveBackupSubkeys(masterKey, k_enc, k_mac);
            computeManifestHmac(k_mac, MAGIC_HEADER, manifestLen, mBuf.data(), calcHmac);
            if (memcmp(calcHmac, storedManifestHmac, 32) == 0) {
                verified = true;
            }
        }
        // 2. Fallback to Legacy NVS Key if available
        if (!verified) {
            uint8_t legacyKey[32];
            if (CryptoEngine::getLegacyNVSKey(legacyKey)) {
                CryptoEngine::deriveBackupSubkeys(legacyKey, k_enc, k_mac);
                computeManifestHmac(k_mac, MAGIC_HEADER, manifestLen, mBuf.data(), calcHmac);
                if (memcmp(calcHmac, storedManifestHmac, 32) == 0) {
                    verified = true;
                    Serial.println("[BackupManager] Verified backup using legacy NVS key fallback");
                }
            }
        }
    }

    // 6. Verify Manifest HMAC
    if (!verified) {
        bkp.close();
        if (isPassphraseMode) {
            return BACKUP_ERR_WRONG_PASSWORD; // Specifically wrong password entered!
        }
        return BACKUP_ERR_MANIFEST_HMAC; // Tamper detected or wrong device / password!
    }

    // 7. Pre-flight Storage Check
    size_t totalNeeded = doc["total_uncompressed_bytes"] | 0;
    size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
    if (freeBytes < totalNeeded + (32 * 1024)) {
        bkp.close();
        return BACKUP_ERR_INSUFFICIENT_STORAGE;
    }

    // 8. Stream & Extract Each Entry
    size_t streamBufSize = 4096;
    uint8_t* streamBuffer = allocStreamBuffer(streamBufSize);
    if (!streamBuffer) {
        bkp.close();
        return BACKUP_ERR_INSUFFICIENT_STORAGE;
    }

    JsonArray entries = doc["entries"].as<JsonArray>();
    int totalEntries = entries.size();
    int currentEntry = 0;

    size_t dataPayloadStart = 12 + 4 + 32 + manifestLen;
    bkp.seek(dataPayloadStart);

    for (JsonObject entry : entries) {
        String rawPath = entry["path"].as<String>();
        if (!isSafePath(rawPath)) {
            Serial.printf("[BackupManager] Security violation on entry path: '%s'\n", rawPath.c_str());
            freeStreamBuffer(streamBuffer);
            bkp.close();
            return BACKUP_ERR_SECURITY_VIOLATION;
        }

        // Normalize path to relative (strip any leading slashes)
        String path = rawPath;
        while (path.startsWith("/") || path.startsWith("\\")) {
            path = path.substring(1);
        }

        if (progress) progress(currentEntry++, totalEntries, path);
        esp_task_wdt_reset();

        bool isEnc = entry["enc"] | false;
        size_t origSize = entry["orig_size"] | 0;
        size_t storedSpan = entry["stored_span"] | 0;

        ensureParentDirsExist("/" + path);

        if (isEnc) {
            size_t cipherSize = entry["ciphertext_size"] | 0;
            if (cipherSize == 0 || (cipherSize % 16 != 0)) {
                freeStreamBuffer(streamBuffer);
                bkp.close();
                return BACKUP_ERR_FILE_INTEGRITY;
            }

            uint8_t iv[16];
            bkp.read(iv, 16);

            std::vector<uint8_t> cipher(cipherSize);
            bkp.read(cipher.data(), cipherSize);

            uint8_t storedFileHmac[32];
            bkp.read(storedFileHmac, 32);

            // Verify File HMAC
            uint8_t calcFileHmac[32];
            mbedtls_md_context_t mdCtx;
            mbedtls_md_init(&mdCtx);
            mbedtls_md_setup(&mdCtx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
            mbedtls_md_hmac_starts(&mdCtx, k_mac, 32);
            mbedtls_md_hmac_update(&mdCtx, iv, 16);
            mbedtls_md_hmac_update(&mdCtx, cipher.data(), cipher.size());
            mbedtls_md_hmac_finish(&mdCtx, calcFileHmac);
            mbedtls_md_free(&mdCtx);

            if (memcmp(calcFileHmac, storedFileHmac, 32) != 0) {
                freeStreamBuffer(streamBuffer);
                bkp.close();
                return BACKUP_ERR_FILE_INTEGRITY;
            }

            // Decrypt Payload
            std::vector<uint8_t> plain;
            if (!CryptoEngine::aesCbcDecryptRaw(cipher.data(), cipherSize, k_enc, iv, plain)) {
                freeStreamBuffer(streamBuffer);
                bkp.close();
                return BACKUP_ERR_FILE_INTEGRITY;
            }

            File out = LittleFS.open("/" + path, FILE_WRITE);
            if (out) {
                if (!plain.empty()) out.write(plain.data(), plain.size());
                out.close();
            }
        } else {
            // Plaintext with SHA-256 Checksum Verification
            mbedtls_sha256_context shaCtx;
            mbedtls_sha256_init(&shaCtx);
            mbedtls_sha256_starts(&shaCtx, 0);

            File out = LittleFS.open("/" + path, FILE_WRITE);
            size_t remaining = origSize;

            while (remaining > 0) {
                size_t toRead = std::min(remaining, streamBufSize);
                size_t n = bkp.read(streamBuffer, toRead);
                if (n == 0) break;

                mbedtls_sha256_update(&shaCtx, streamBuffer, n);
                if (out) out.write(streamBuffer, n);
                remaining -= n;
            }
            if (out) out.close();

            uint8_t storedHash[32];
            bkp.read(storedHash, 32);

            uint8_t calcHash[32];
            mbedtls_sha256_finish(&shaCtx, calcHash);
            mbedtls_sha256_free(&shaCtx);

            if (memcmp(calcHash, storedHash, 32) != 0) {
                freeStreamBuffer(streamBuffer);
                bkp.close();
                return BACKUP_ERR_FILE_INTEGRITY;
            }
        }
    }

    freeStreamBuffer(streamBuffer);
    bkp.close();

    if (progress) progress(totalEntries, totalEntries, "Restore complete!");
    return BACKUP_SUCCESS;
}

void BackupManager::executeAtomicReboot(int countdownSec) {
    // Unmount LittleFS & SD cleanly
    LittleFS.end();
    SD.end();

    for (int i = countdownSec; i > 0; i--) {
        delay(1000);
    }
    esp_restart();
}
