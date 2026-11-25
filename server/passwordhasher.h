#pragma once

#include <string>

class PasswordHasher {
public:
    static constexpr int kDefaultIterations = 10000;
    static constexpr int kTestIterations = 10;
    static constexpr int kSaltBytes = 16;

    // Generates 16 random bytes as a 32-character lowercase hex string.
    static std::string generateSaltHex();

    // Generates a 64-character hex string representing 32 bytes of PBKDF2-HMAC-SHA256.
    static std::string hashPassword(const std::string& password,
                                    const std::string& saltHex,
                                    int iterations = kDefaultIterations);

    // Constant-time comparison between hashed password and expected hash.
    static bool verifyPassword(const std::string& password,
                               const std::string& saltHex,
                               int iterations,
                               const std::string& expectedHashHex);
};