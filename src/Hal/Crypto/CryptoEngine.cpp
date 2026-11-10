#include "CryptoEngine.h"
#include "FileSystem/FileSystem.h"
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/base64.h>
#include <mbedtls/pkcs5.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <ArduinoJson.h>

bool CryptoEngine::hexToBytes(const String& hex, std::vector<uint8_t>& out) {
    out.clear();
    if (hex.length() % 2 != 0) return false;

    out.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        char h1 = hex.charAt(i);
        char h2 = hex.charAt(i + 1);

        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        int v1 = hexVal(h1);
        int v2 = hexVal(h2);
        if (v1 < 0 || v2 < 0) {
            out.clear();
            return false;
        }
        out.push_back((uint8_t)((v1 << 4) | v2));
    }
    return true;
}

String CryptoEngine::bytesToHex(const uint8_t* data, size_t length) {
    if (!data || length == 0) return "";
    static const char hexChars[] = "0123456789abcdef";
    String hexStr = "";
    hexStr.reserve(length * 2);
    for (size_t i = 0; i < length; i++) {
        hexStr += hexChars[(data[i] >> 4) & 0x0F];
        hexStr += hexChars[data[i] & 0x0F];
    }
    return hexStr;
}

String CryptoEngine::sha256(const String& data) {
    uint8_t hash[32];
    mbedtls_sha256((const unsigned char*)data.c_str(), data.length(), hash, 0 /* 0 = SHA-256 */);
    return bytesToHex(hash, 32);
}

String CryptoEngine::sha512(const String& data) {
    uint8_t hash[64];
    mbedtls_sha512((const unsigned char*)data.c_str(), data.length(), hash, 0 /* 0 = SHA-512 */);
    return bytesToHex(hash, 64);
}

String CryptoEngine::hmacSha256(const String& key, const String& message) {
    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!mdInfo) return "";

    mbedtls_md_context_t mdCtx;
    mbedtls_md_init(&mdCtx);

    if (mbedtls_md_setup(&mdCtx, mdInfo, 1 /* HMAC enabled */) != 0) {
        mbedtls_md_free(&mdCtx);
        return "";
    }

    uint8_t hmacOut[32];
    if (mbedtls_md_hmac_starts(&mdCtx, (const unsigned char*)key.c_str(), key.length()) != 0 ||
        mbedtls_md_hmac_update(&mdCtx, (const unsigned char*)message.c_str(), message.length()) != 0 ||
        mbedtls_md_hmac_finish(&mdCtx, hmacOut) != 0) {
        mbedtls_md_free(&mdCtx);
        return "";
    }

    mbedtls_md_free(&mdCtx);
    return bytesToHex(hmacOut, 32);
}

String CryptoEngine::aesEncrypt(const String& plainText, const String& keyHex, const String& ivHex) {
    // 1. Strict Input Key & IV Length Validation
    if (keyHex.length() != 32 && keyHex.length() != 64) {
        Serial.println("[Crypto] Error: AES keyHex must be 32 (128-bit) or 64 (256-bit) hex characters.");
        return "";
    }
    if (ivHex.length() != 32) {
        Serial.println("[Crypto] Error: AES ivHex must be 32 hex characters (16 bytes).");
        return "";
    }

    std::vector<uint8_t> keyBytes, ivBytes;
    if (!hexToBytes(keyHex, keyBytes) || !hexToBytes(ivHex, ivBytes) || ivBytes.size() != 16) {
        return "";
    }

    // 2. PKCS#7 Padding Alignment
    size_t plainLen = plainText.length();
    size_t padLen = 16 - (plainLen % 16);
    size_t totalLen = plainLen + padLen;

    std::vector<uint8_t> padded(totalLen);
    memcpy(padded.data(), plainText.c_str(), plainLen);
    memset(padded.data() + plainLen, (uint8_t)padLen, padLen);

    // 3. In-Place IV Mutation Guard (mbedTLS mutates IV in-place during CBC)
    unsigned char ivCopy[16];
    memcpy(ivCopy, ivBytes.data(), 16);

    // 4. Hardware AES-CBC Encryption
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    if (mbedtls_aes_setkey_enc(&aes, keyBytes.data(), (unsigned int)(keyBytes.size() * 8)) != 0) {
        mbedtls_aes_free(&aes);
        return "";
    }

    std::vector<uint8_t> cipher(totalLen);
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, totalLen, ivCopy, padded.data(), cipher.data());
    mbedtls_aes_free(&aes);

    if (ret != 0) {
        return "";
    }

    // 5. Base64 Encode Ciphertext
    size_t b64Len = 0;
    mbedtls_base64_encode(nullptr, 0, &b64Len, cipher.data(), cipher.size());
    if (b64Len == 0) return "";

    std::vector<unsigned char> b64Out(b64Len + 1, 0);
    size_t actualLen = 0;
    if (mbedtls_base64_encode(b64Out.data(), b64Out.size(), &actualLen, cipher.data(), cipher.size()) != 0) {
        return "";
    }

    return String((const char*)b64Out.data());
}

String CryptoEngine::aesDecrypt(const String& base64Cipher, const String& keyHex, const String& ivHex) {
    // 1. Strict Input Key & IV Length Validation
    if (keyHex.length() != 32 && keyHex.length() != 64) {
        Serial.println("[Crypto] Error: AES keyHex must be 32 (128-bit) or 64 (256-bit) hex characters.");
        return "";
    }
    if (ivHex.length() != 32) {
        Serial.println("[Crypto] Error: AES ivHex must be 32 hex characters (16 bytes).");
        return "";
    }

    std::vector<uint8_t> keyBytes, ivBytes;
    if (!hexToBytes(keyHex, keyBytes) || !hexToBytes(ivHex, ivBytes) || ivBytes.size() != 16) {
        return "";
    }

    // 2. Base64 Decode Ciphertext
    size_t outLen = 0;
    mbedtls_base64_decode(nullptr, 0, &outLen, (const unsigned char*)base64Cipher.c_str(), base64Cipher.length());
    if (outLen == 0) return "";

    std::vector<uint8_t> cipherBytes(outLen);
    size_t actualLen = 0;
    if (mbedtls_base64_decode(cipherBytes.data(), cipherBytes.size(), &actualLen, (const unsigned char*)base64Cipher.c_str(), base64Cipher.length()) != 0) {
        return "";
    }
    cipherBytes.resize(actualLen);

    // 3. Ciphertext Block Alignment Check (Must be multiple of 16)
    if (cipherBytes.empty() || (cipherBytes.size() % 16 != 0)) {
        Serial.println("[Crypto] Error: Decoded ciphertext length is not a multiple of 16 bytes.");
        return "";
    }

    // 4. In-Place IV Mutation Guard
    unsigned char ivCopy[16];
    memcpy(ivCopy, ivBytes.data(), 16);

    // 5. Hardware AES-CBC Decryption
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    if (mbedtls_aes_setkey_dec(&aes, keyBytes.data(), (unsigned int)(keyBytes.size() * 8)) != 0) {
        mbedtls_aes_free(&aes);
        return "";
    }

    std::vector<uint8_t> plain(cipherBytes.size());
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherBytes.size(), ivCopy, cipherBytes.data(), plain.data());
    mbedtls_aes_free(&aes);

    if (ret != 0) {
        return "";
    }

    // 6. Strict PKCS#7 Unpadding Validation & Underflow Guard
    size_t plainLen = plain.size();
    if (plainLen == 0) return "";

    uint8_t pad = plain[plainLen - 1];
    if (pad == 0 || pad > 16 || pad > plainLen) {
        return ""; // Invalid PKCS#7 padding
    }

    for (size_t i = 0; i < pad; i++) {
        if (plain[plainLen - 1 - i] != pad) {
            return ""; // Padding mismatch
        }
    }
    plainLen -= pad;

    return String((const char*)plain.data(), plainLen);
}

String CryptoEngine::randomBytes(size_t length) {
    if (length == 0 || length > 4096) return "";
    std::vector<uint8_t> buf(length);
    esp_fill_random(buf.data(), length);
    return bytesToHex(buf.data(), length);
}

bool CryptoEngine::rsaVerify(const String& pubKeyPem, const String& message, const String& sigBase64) {
    if (pubKeyPem.length() == 0 || message.length() == 0 || sigBase64.length() == 0) {
        return false;
    }

    // 1. Decode Base64 Signature
    size_t sigLen = 0;
    mbedtls_base64_decode(nullptr, 0, &sigLen, (const unsigned char*)sigBase64.c_str(), sigBase64.length());
    if (sigLen == 0) return false;

    std::vector<uint8_t> sigBytes(sigLen);
    size_t actualSigLen = 0;
    if (mbedtls_base64_decode(sigBytes.data(), sigBytes.size(), &actualSigLen, (const unsigned char*)sigBase64.c_str(), sigBase64.length()) != 0) {
        return false;
    }
    sigBytes.resize(actualSigLen);

    // 2. Parse PEM Public Key (Length MUST include null-terminator + 1)
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char*)pubKeyPem.c_str(), pubKeyPem.length() + 1);
    if (ret != 0) {
        Serial.printf("[Crypto] Error: mbedtls_pk_parse_public_key failed (ret = -0x%04X)\n", -ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    // 3. Compute SHA-256 Hash of Message
    uint8_t hash[32];
    mbedtls_sha256((const unsigned char*)message.c_str(), message.length(), hash, 0);

    // 4. Hardware RSA Signature Verification (PKCS#1 v1.5 with SHA-256)
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sigBytes.data(), sigBytes.size());
    mbedtls_pk_free(&pk);

    return (ret == 0);
}

bool CryptoEngine::getDeviceKey(uint8_t keyOut[32]) {
    if (!keyOut) return false;

    // Deterministic Hardware Silicon Identity:
    // Derives root key directly from immutable hardware registers burned into wafer eFuses at factory.
    // Survives full flash erasure (esptool.py erase_flash) while guaranteeing device uniqueness.
    struct __attribute__((packed)) {
        uint8_t factoryMac[6];
        uint8_t staMac[6];
        uint8_t btMac[6];
        uint64_t efuseMac;
        uint8_t chipRev;
        uint8_t chipCores;
        char chipModel[16];
        char domain[36];
    } hwIdentity;

    memset(&hwIdentity, 0, sizeof(hwIdentity));
    esp_efuse_mac_get_default(hwIdentity.factoryMac);
    esp_read_mac(hwIdentity.staMac, ESP_MAC_WIFI_STA);
    esp_read_mac(hwIdentity.btMac, ESP_MAC_BT);
    hwIdentity.efuseMac = ESP.getEfuseMac();
    hwIdentity.chipRev = (uint8_t)ESP.getChipRevision();
    hwIdentity.chipCores = (uint8_t)ESP.getChipCores();

    const char* model = ESP.getChipModel();
    if (model) {
        strncpy(hwIdentity.chipModel, model, sizeof(hwIdentity.chipModel) - 1);
    }
    strncpy(hwIdentity.domain, "KryonOS-Silicon-Hardware-Root-v1", sizeof(hwIdentity.domain) - 1);

    static const uint8_t KRYON_HW_DOMAIN_SALT[] = "KRYON_OS_DISASTER_RECOVERY_ROOT_SALT_V1";
    hmacSha256Raw(KRYON_HW_DOMAIN_SALT, sizeof(KRYON_HW_DOMAIN_SALT) - 1,
                  (const uint8_t*)&hwIdentity, sizeof(hwIdentity),
                  keyOut);

    return true;
}

bool CryptoEngine::getLegacyNVSKey(uint8_t keyOut[32]) {
    if (!keyOut) return false;
    Preferences prefs;
    if (!prefs.begin("kryon_sec", true)) { // Read-only
        return false;
    }
    if (!prefs.isKey("enc_key")) {
        prefs.end();
        return false;
    }
    size_t len = prefs.getBytes("enc_key", keyOut, 32);
    prefs.end();
    return (len == 32);
}

String CryptoEngine::deviceEncrypt(const String& plaintext) {
    if (plaintext.length() == 0) return "";
    uint8_t key[32];
    if (!getDeviceKey(key)) return "";

    String keyHex = bytesToHex(key, 32);
    String ivHex = randomBytes(16); // 16 bytes = 32 hex chars

    String cipherBase64 = aesEncrypt(plaintext, keyHex, ivHex);
    if (cipherBase64.length() == 0) return "";

    JsonDocument doc;
    doc["v"] = 1;
    doc["iv"] = ivHex;
    doc["data"] = cipherBase64;

    String output;
    serializeJson(doc, output);
    return output;
}

String CryptoEngine::deviceDecrypt(const String& ciphertext) {
    if (ciphertext.length() == 0) return "";
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ciphertext);
    if (err) {
        Serial.printf("[Crypto] JSON parse error in deviceDecrypt: %s\n", err.c_str());
        return "";
    }

    const char* ivHex = doc["iv"];
    const char* dataBase64 = doc["data"];
    if (!ivHex || !dataBase64) return "";

    uint8_t key[32];
    if (getDeviceKey(key)) {
        String keyHex = bytesToHex(key, 32);
        String decrypted = aesDecrypt(String(dataBase64), keyHex, String(ivHex));
        if (decrypted.length() > 0) {
            return decrypted;
        }
    }

    // Fallback to legacy NVS key if present
    if (getLegacyNVSKey(key)) {
        String keyHex = bytesToHex(key, 32);
        String decrypted = aesDecrypt(String(dataBase64), keyHex, String(ivHex));
        if (decrypted.length() > 0) {
            return decrypted;
        }
    }

    return "";
}

String CryptoEngine::sha256File(const char* path) {
    File f = FileSystem::openFile(path, "r");
    if (!f || f.isDirectory()) {
        return "";
    }

    mbedtls_sha256_context shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, 0 /* 0 = SHA-256 */);

    uint8_t buffer[512];
    while (f.available()) {
        size_t bytesRead = f.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            mbedtls_sha256_update(&shaCtx, buffer, bytesRead);
        }
    }
    f.close();

    uint8_t hash[32];
    mbedtls_sha256_finish(&shaCtx, hash);
    mbedtls_sha256_free(&shaCtx);

    return bytesToHex(hash, 32);
}

void CryptoEngine::hmacSha256Raw(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t outHmac[32]) {
    mbedtls_md_context_t mdCtx;
    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&mdCtx);
    if (mbedtls_md_setup(&mdCtx, mdInfo, 1) == 0) {
        mbedtls_md_hmac_starts(&mdCtx, key, keyLen);
        if (msg && msgLen > 0) {
            mbedtls_md_hmac_update(&mdCtx, msg, msgLen);
        }
        mbedtls_md_hmac_finish(&mdCtx, outHmac);
    } else {
        memset(outHmac, 0, 32);
    }
    mbedtls_md_free(&mdCtx);
}

void CryptoEngine::sha256Raw(const uint8_t* msg, size_t msgLen, uint8_t outHash[32]) {
    mbedtls_sha256_context shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, 0);
    if (msg && msgLen > 0) {
        mbedtls_sha256_update(&shaCtx, msg, msgLen);
    }
    mbedtls_sha256_finish(&shaCtx, outHash);
    mbedtls_sha256_free(&shaCtx);
}

bool CryptoEngine::pbkdf2HmacSha256(const String& passphrase, const uint8_t* salt, size_t saltLen, uint32_t iterations, uint8_t outKey[32]) {
    if (passphrase.length() == 0 || !salt || saltLen == 0 || iterations == 0) return false;
    
    mbedtls_md_context_t mdCtx;
    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&mdCtx);
    if (mbedtls_md_setup(&mdCtx, mdInfo, 1) != 0) {
        mbedtls_md_free(&mdCtx);
        return false;
    }
    
    esp_task_wdt_reset();
    int ret = mbedtls_pkcs5_pbkdf2_hmac(&mdCtx, 
                                        (const unsigned char*)passphrase.c_str(), passphrase.length(),
                                        salt, saltLen, 
                                        iterations, 32, outKey);
    esp_task_wdt_reset();
    mbedtls_md_free(&mdCtx);
    return (ret == 0);
}

void CryptoEngine::deriveBackupSubkeys(const uint8_t masterKey[32], uint8_t encKey[32], uint8_t macKey[32]) {
    const char* encSalt = "KRYON_BKP_ENC_KEY_v1";
    const char* macSalt = "KRYON_BKP_MAC_KEY_v1";
    hmacSha256Raw(masterKey, 32, (const uint8_t*)encSalt, strlen(encSalt), encKey);
    hmacSha256Raw(masterKey, 32, (const uint8_t*)macSalt, strlen(macSalt), macKey);
}

bool CryptoEngine::aesCbcEncryptRaw(const uint8_t* plain, size_t plainLen, const uint8_t key[32], const uint8_t iv[16], std::vector<uint8_t>& outCipher) {
    size_t padLen = 16 - (plainLen % 16);
    size_t totalLen = plainLen + padLen;
    std::vector<uint8_t> padded(totalLen);
    if (plainLen > 0) memcpy(padded.data(), plain, plainLen);
    memset(padded.data() + plainLen, (uint8_t)padLen, padLen);

    unsigned char ivCopy[16];
    memcpy(ivCopy, iv, 16);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 256) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    outCipher.resize(totalLen);
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, totalLen, ivCopy, padded.data(), outCipher.data());
    mbedtls_aes_free(&aes);
    return (ret == 0);
}

bool CryptoEngine::aesCbcDecryptRaw(const uint8_t* cipher, size_t cipherLen, const uint8_t key[32], const uint8_t iv[16], std::vector<uint8_t>& outPlain) {
    if (cipherLen == 0 || (cipherLen % 16 != 0)) return false;

    unsigned char ivCopy[16];
    memcpy(ivCopy, iv, 16);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, key, 256) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    std::vector<uint8_t> plain(cipherLen);
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLen, ivCopy, cipher, plain.data());
    mbedtls_aes_free(&aes);
    if (ret != 0) return false;

    uint8_t pad = plain[cipherLen - 1];
    if (pad == 0 || pad > 16 || pad > cipherLen) return false;
    for (size_t i = 0; i < pad; i++) {
        if (plain[cipherLen - 1 - i] != pad) return false;
    }

    size_t actualLen = cipherLen - pad;
    outPlain.resize(actualLen);
    if (actualLen > 0) memcpy(outPlain.data(), plain.data(), actualLen);
    return true;
}

