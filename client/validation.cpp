#include "validation.h"

#include <cctype>

namespace {

// Same value as Recording::kMaxOffsetSeconds in headers/recording.h. Kept
// here until Recording moves into src/ for the final build.
constexpr long long kMaxRecordingOffsetSeconds = 24 * 60 * 60;

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWithIgnoringCase(const std::string& text, const std::string& suffix) {
    if (text.size() < suffix.size()) {
        return false;
    }
    const std::string tail = text.substr(text.size() - suffix.size());
    for (size_t i = 0; i < suffix.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(tail[i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

// Shared by the "text with a maximum" checks (course, session, question, answer).
Error checkText(const std::string& text, int maxLength, Error empty, Error tooLong) {
    if (text.empty()) {
        return empty;
    }
    if (Validation::getLength(text) > maxLength) {
        return tooLong;
    }
    return Error::None;
}

}  // namespace

std::string Validation::trim(const std::string& text) {
    const char* space = " \t\r\n";
    const auto first = text.find_first_not_of(space);
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(space);
    return text.substr(first, last - first + 1);
}

int Validation::getLength(const std::string& text) {
    // Count every byte that does not continue a UTF-8 character (10xxxxxx).
    int count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

std::string Validation::toUpper(const std::string& text) {
    std::string upper = text;
    for (char& c : upper) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return upper;
}

Error Validation::checkUsername(const std::string& name) {
    if (name.empty()) {
        return Error::EmptyName;
    }
    const int length = getLength(name);
    if (length < kMinNameLength) {
        return Error::NameTooShort;
    }
    if (length > kMaxNameLength) {
        return Error::NameTooLong;
    }
    for (unsigned char c : name) {
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-')) {
            return Error::NameBadCharacters;
        }
    }
    return Error::None;
}

Error Validation::checkPassword(const std::string& password) {
    if (password.empty()) {
        return Error::EmptyPassword;
    }
    const int length = getLength(password);
    if (length < kMinPasswordLength) {
        return Error::PasswordTooShort;
    }
    if (length > kMaxPasswordLength) {
        return Error::PasswordTooLong;
    }
    return Error::None;
}

Error Validation::checkEmail(const std::string& email) {
    if (email.empty()) {
        return Error::EmptyEmail;
    }
    const size_t at = email.find('@');
    if (getLength(email) > kMaxEmailLength || at == std::string::npos || at == 0 ||
        email.find('@', at + 1) != std::string::npos ||
        email.find_first_of(" \t\r\n") != std::string::npos) {
        return Error::EmailInvalid;
    }
    if (!endsWithIgnoringCase(email, kSchoolEmailDomain)) {
        return Error::EmailNotSchool;
    }
    return Error::None;
}

Error Validation::checkCourseName(const std::string& name) {
    return checkText(name, kMaxCourseNameLength, Error::EmptyCourseName, Error::CourseNameTooLong);
}

Error Validation::checkSessionName(const std::string& name) {
    return checkText(name, kMaxSessionNameLength, Error::EmptySessionName, Error::SessionNameTooLong);
}

Error Validation::checkQuestion(const std::string& text) {
    return checkText(text, kMaxQuestionLength, Error::EmptyQuestion, Error::QuestionTooLong);
}

Error Validation::checkAnswer(const std::string& text) {
    return checkText(text, kMaxAnswerLength, Error::EmptyAnswer, Error::AnswerTooLong);
}

Error Validation::checkRecordingUrl(const std::string& url) {
    const int length = getLength(url);
    if (length < 1 || length > kMaxUrlLength || url.find_first_of(" \t\r\n") != std::string::npos ||
        !(startsWith(url, "https://") || startsWith(url, "http://"))) {
        return Error::RecordingUrlInvalid;
    }
    return Error::None;
}

Error Validation::checkRecordingOffset(long long offsetSeconds) {
    if (offsetSeconds < -kMaxRecordingOffsetSeconds || offsetSeconds > kMaxRecordingOffsetSeconds) {
        return Error::RecordingOffsetInvalid;
    }
    return Error::None;
}
