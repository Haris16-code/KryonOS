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

    // Hardware AES-CBC (128/256-bit with PKCS#7 padding)
    static String aesEncrypt(const String& plainText, const String& keyHex, const String& ivHex);
    static String aesDecrypt(const String& base64Cipher, const String& keyHex, const String& ivHex);

    // Hardware True Random Number Generator (TRNG)
    static String randomBytes(size_t length);

    // Device-Unique Hardware RNG Key & AES-256 Storage
    static bool getDeviceKey(uint8_t keyOut[32]);
    static String deviceEncrypt(const String& plaintext);
    static String deviceDecrypt(const String& ciphertext);

    // Hardware-Accelerated Streaming File Hash
    static String sha256File(const char* path);

    // Hardware RSA Digital Signature Verification (PKCS#1 v1.5 with SHA-256)
    static bool rsaVerify(const String& pubKeyPem, const String& message, const String& sigBase64);

    // Utilities
    static bool hexToBytes(const String& hex, std::vector<uint8_t>& out);
    static String bytesToHex(const uint8_t* data, size_t length);
};
