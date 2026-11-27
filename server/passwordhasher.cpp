#include "passwordhasher.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>

std::string PasswordHasher::generateSaltHex() {
    quint32 randomWords[kSaltBytes / sizeof(quint32)];
    QRandomGenerator::system()->fillRange(randomWords, kSaltBytes / sizeof(quint32));
    QByteArray saltBytes(reinterpret_cast<const char*>(randomWords), sizeof(randomWords));
    return saltBytes.toHex().toStdString();
}

std::string PasswordHasher::hashPassword(const std::string& password,
                                        const std::string& saltHex,
                                        int iterations) {
    if (iterations <= 0) {
        iterations = kDefaultIterations;
    }

    // Qt's own PBKDF2-HMAC-SHA256 (RFC 8018), 32 bytes.
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                              QByteArray::fromStdString(password),
                                              QByteArray::fromHex(QByteArray::fromStdString(saltHex)),
                                              iterations, 32)
        .toHex()
        .toStdString();
}

bool PasswordHasher::verifyPassword(const std::string& password,
                                    const std::string& saltHex,
                                    int iterations,
                                    const std::string& expectedHashHex) {
    if (password.empty() || saltHex.empty() || expectedHashHex.empty() || iterations <= 0) {
        return false;
    }

    const std::string computedHex = hashPassword(password, saltHex, iterations);
    if (computedHex.size() != expectedHashHex.size()) {
        return false;
    }

    // Constant-time comparison to prevent timing side channels.
    int diff = 0;
    for (std::size_t i = 0; i < computedHex.size(); ++i) {
        diff |= (computedHex[i] ^ expectedHashHex[i]);
    }
    return diff == 0;
}