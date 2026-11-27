// MODEL (rules): every length limit and format check in one place.
// Plain C++ (no Qt). Pure functions: no database, no state.
//
// The server's AppModel calls these before saving anything. The app's
// controller may ALSO call them before sending, so the user gets feedback
// without a round trip - but the server's answer is the one that counts.
//
// All lengths are counted in characters (Unicode code points), not bytes,
// so "naïve" is 5 and an emoji is 1. Strings are UTF-8.
//
// Owner: Model layer (+ tests/test_validation.cpp).

#pragma once

#include <string>

#include "error.h"

class Validation {
public:
    // ---- The limits (CONTRACT.md section 5; also quoted in REQUIREMENTS.md) ----
    static constexpr int kMinNameLength = 3;         // U1
    static constexpr int kMaxNameLength = 20;        // U1 (username)
    static constexpr int kMinPasswordLength = 8;     // U3
    static constexpr int kMaxPasswordLength = 64;    // U3
    static constexpr int kMaxEmailLength = 254;      // U6
    static constexpr int kMaxCourseNameLength = 60;  // C2
    static constexpr int kMaxSessionNameLength = 60; // S2
    static constexpr int kMaxQuestionLength = 500;   // I3
    static constexpr int kMaxAnswerLength = 1000;    // A3
    static constexpr int kMaxUrlLength = 2048;       // R2
    // Emails must end in this, ignoring case (U6).
    static constexpr const char* kSchoolEmailDomain = "@scu.edu";

    // ---- Helpers ----
    // Removes spaces, tabs and line breaks from both ends.
    static std::string trim(const std::string& text);
    // Number of characters (code points) in UTF-8 text.
    static int getLength(const std::string& text);
    // ASCII upper case; other characters unchanged. Used for join codes.
    static std::string toUpper(const std::string& text);

    // ---- Checks. Each expects text that is ALREADY trimmed (except the
    // password, which is never trimmed) and returns Error::None if it is fine.
    // When several things are wrong, the first one in the listed order wins. ----

    // U1: EmptyName, NameTooShort, NameTooLong, NameBadCharacters
    // (allowed: A-Z a-z 0-9 . _ -).
    static Error checkUsername(const std::string& name);

    // U3: EmptyPassword, PasswordTooShort, PasswordTooLong. Any characters.
    static Error checkPassword(const std::string& password);

    // U6: EmptyEmail, EmailInvalid (too long, not exactly one @, empty part
    // before the @, spaces), then EmailNotSchool (does not end in
    // kSchoolEmailDomain, ignoring case).
    static Error checkEmail(const std::string& email);

    // C2: EmptyCourseName, CourseNameTooLong.
    static Error checkCourseName(const std::string& name);

    // S2: EmptySessionName, SessionNameTooLong.
    static Error checkSessionName(const std::string& name);

    // I3: EmptyQuestion, QuestionTooLong.
    static Error checkQuestion(const std::string& text);

    // A3: EmptyAnswer, AnswerTooLong.
    static Error checkAnswer(const std::string& text);

    // R2: RecordingUrlInvalid unless 1..kMaxUrlLength characters, no spaces,
    // and starts with "https://" or "http://".
    static Error checkRecordingUrl(const std::string& url);
    // R6: "" (unlink) or a Panopto folder id: 1-64 hex digits and dashes,
    // e.g. "1b2c3d4e-0000-4a5b-8c9d-0123456789ab". PanoptoFolderInvalid otherwise.
    static Error checkPanoptoFolderId(const std::string& folderId);

    // R3: RecordingOffsetInvalid unless within +/- Recording::kMaxOffsetSeconds.
    static Error checkRecordingOffset(long long offsetSeconds);
};
