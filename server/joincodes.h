#pragma once

#include <string>
#include <QRandomGenerator>

class JoinCodes {
public:
    static std::string generateCourseCode() {
        // 6 characters from A-Z and 2-9 excluding 0, O, 1, I
        static constexpr char kChars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        static constexpr int kCharCount = sizeof(kChars) - 1;

        std::string code;
        code.reserve(6);
        for (int i = 0; i < 6; ++i) {
            quint32 index = QRandomGenerator::global()->bounded(kCharCount);
            code.push_back(kChars[index]);
        }
        return code;
    }

    static std::string generateSessionCode() {
        // 6 digits (000000 - 999999)
        quint32 number = QRandomGenerator::global()->bounded(1000000);
        char buf[7];
        std::snprintf(buf, sizeof(buf), "%06u", number);
        return std::string(buf);
    }
};