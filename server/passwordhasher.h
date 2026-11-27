#pragma once

#include <string>

class PasswordHasher {
public:
    // New hashes use this many rounds (each row stores its own count, so
    // older accounts still log in). 100,000 takes about 0.15 s on a laptop:
    // slow for someone guessing passwords, fast enough that a class logging
    // in at once doesn't stall the one-threaded server. (OWASP suggests
    // 600,000; that would block the server ~1 s per login.)
    static constexpr int kDefaultIterations = 100000;
    static constexpr int kTestIterations = 10;
    static constexpr int kSaltBytes = 16;

    // 16 random bytes from the operating system's secure generator, as a
    // 32-character lowercase hex string.
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