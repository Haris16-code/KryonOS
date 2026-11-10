#pragma once

#include <Arduino.h>
#include <vector>

class CryptoEngine {
public:
    // Hardware SHA Hash Engines
    static String sha256(const String& data);
    static String sha512(const String& data);

    // Hardware-Assisted HMAC
    static String hmacSha256(const String& key, const String& message);
    static void hmacSha256Raw(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t outHmac[32]);
    static void sha256Raw(const uint8_t* msg, size_t msgLen, uint8_t outHash[32]);

    // Hardware AES-CBC (128/256-bit with PKCS#7 padding)
    static String aesEncrypt(const String& plainText, const String& keyHex, const String& ivHex);
    static String aesDecrypt(const String& base64Cipher, const String& keyHex, const String& ivHex);
    static bool aesCbcEncryptRaw(const uint8_t* plain, size_t plainLen, const uint8_t key[32], const uint8_t iv[16], std::vector<uint8_t>& outCipher);
    static bool aesCbcDecryptRaw(const uint8_t* cipher, size_t cipherLen, const uint8_t key[32], const uint8_t iv[16], std::vector<uint8_t>& outPlain);

    // Hardware True Random Number Generator (TRNG)
    static String randomBytes(size_t length);

    // Device-Unique Hardware Silicon Key & AES-256 Storage
    static bool getDeviceKey(uint8_t keyOut[32]);
    static bool getLegacyNVSKey(uint8_t keyOut[32]);
    static String deviceEncrypt(const String& plaintext);
    static String deviceDecrypt(const String& ciphertext);

    // Backup & Disaster Recovery Cryptographic Helpers
    static bool pbkdf2HmacSha256(const String& passphrase, const uint8_t* salt, size_t saltLen, uint32_t iterations, uint8_t outKey[32]);
    static void deriveBackupSubkeys(const uint8_t masterKey[32], uint8_t encKey[32], uint8_t macKey[32]);

    // Hardware-Accelerated Streaming File Hash
    static String sha256File(const char* path);

    // Hardware RSA Digital Signature Verification (PKCS#1 v1.5 with SHA-256)
    static bool rsaVerify(const String& pubKeyPem, const String& message, const String& sigBase64);

    // Utilities
    static bool hexToBytes(const String& hex, std::vector<uint8_t>& out);
    static String bytesToHex(const uint8_t* data, size_t length);
};
