#include "passwordhasher.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

std::string PasswordHasher::generateSaltHex() {
    quint32 randomWords[kSaltBytes / sizeof(quint32)];
    QRandomGenerator::global()->fillRange(randomWords, kSaltBytes / sizeof(quint32));
    QByteArray saltBytes(reinterpret_cast<const char*>(randomWords), sizeof(randomWords));
    return saltBytes.toHex().toStdString();
}

std::string PasswordHasher::hashPassword(const std::string& password,
                                        const std::string& saltHex,
                                        int iterations) {
    if (iterations <= 0) {
        iterations = kDefaultIterations;
    }

    const QByteArray passBytes = QByteArray::fromStdString(password);
    const QByteArray saltBytes = QByteArray::fromHex(QByteArray::fromStdString(saltHex));

    // PBKDF2-HMAC-SHA256: block index 1 (big endian 0x00000001) for 32 output bytes.
    QByteArray saltWithIndex = saltBytes;
    saltWithIndex.append('\0');
    saltWithIndex.append('\0');
    saltWithIndex.append('\0');
    saltWithIndex.append('\1');

    QByteArray u = QMessageAuthenticationCode::hash(saltWithIndex, passBytes, QCryptographicHash::Sha256);
    QByteArray result = u;

    for (int i = 1; i < iterations; ++i) {
        u = QMessageAuthenticationCode::hash(u, passBytes, QCryptographicHash::Sha256);
        for (int j = 0; j < result.size(); ++j) {
            result[j] = static_cast<char>(result[j] ^ u[j]);
        }
    }

    return result.toHex().toStdString();
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