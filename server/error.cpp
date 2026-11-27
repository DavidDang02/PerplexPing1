#include "error.h"

#include <array>

namespace {

struct ErrorEntry {
    Error error;
    const char* code;
};

constexpr std::array<ErrorEntry, 57> kErrorTable = {{
    {Error::None, "None"},
    {Error::BadRequest, "BadRequest"},
    {Error::UnknownRequest, "UnknownRequest"},
    {Error::NotLoggedIn, "NotLoggedIn"},
    {Error::StorageError, "StorageError"},
    {Error::ServerUnreachable, "ServerUnreachable"},
    {Error::EmptyName, "EmptyName"},
    {Error::NameTooShort, "NameTooShort"},
    {Error::NameTooLong, "NameTooLong"},
    {Error::NameBadCharacters, "NameBadCharacters"},
    {Error::NameTaken, "NameTaken"},
    {Error::EmptyPassword, "EmptyPassword"},
    {Error::PasswordTooShort, "PasswordTooShort"},
    {Error::PasswordTooLong, "PasswordTooLong"},
    {Error::WrongLogin, "WrongLogin"},
    {Error::WrongPassword, "WrongPassword"},
    {Error::EmptyEmail, "EmptyEmail"},
    {Error::EmailInvalid, "EmailInvalid"},
    {Error::EmailNotSchool, "EmailNotSchool"},
    {Error::EmailTaken, "EmailTaken"},
    {Error::HostingLiveSession, "HostingLiveSession"},
    {Error::EmptyCourseName, "EmptyCourseName"},
    {Error::CourseNameTooLong, "CourseNameTooLong"},
    {Error::NoSuchCourse, "NoSuchCourse"},
    {Error::NoSuchCourseCode, "NoSuchCourseCode"},
    {Error::NotCourseEducator, "NotCourseEducator"},
    {Error::EmptySessionName, "EmptySessionName"},
    {Error::SessionNameTooLong, "SessionNameTooLong"},
    {Error::NoSuchSession, "NoSuchSession"},
    {Error::NoSuchSessionCode, "NoSuchSessionCode"},
    {Error::CourseHasLiveSession, "CourseHasLiveSession"},
    {Error::SessionEnded, "SessionEnded"},
    {Error::SessionNotEnded, "SessionNotEnded"},
    {Error::SessionNotPublished, "SessionNotPublished"},
    {Error::SessionAlreadyPublished, "SessionAlreadyPublished"},
    {Error::NotAStudent, "NotAStudent"},
    {Error::EmptyQuestion, "EmptyQuestion"},
    {Error::QuestionTooLong, "QuestionTooLong"},
    {Error::LostTooSoon, "LostTooSoon"},
    {Error::NoSuchInput, "NoSuchInput"},
    {Error::NotYourInput, "NotYourInput"},
    {Error::EmptyAnswer, "EmptyAnswer"},
    {Error::AnswerTooLong, "AnswerTooLong"},
    {Error::NoSuchAnswer, "NoSuchAnswer"},
    {Error::NotYourAnswer, "NotYourAnswer"},
    {Error::RecordingUrlInvalid, "RecordingUrlInvalid"},
    {Error::RecordingOffsetInvalid, "RecordingOffsetInvalid"},
    {Error::PanoptoFolderInvalid, "PanoptoFolderInvalid"},
    {Error::PanoptoNotConfigured, "PanoptoNotConfigured"},
    {Error::PanoptoSignInFailed, "PanoptoSignInFailed"},
    {Error::PanoptoLinkInvalid, "PanoptoLinkInvalid"},
    {Error::PanoptoUnreachable, "PanoptoUnreachable"},
    {Error::PanoptoFolderNotLinked, "PanoptoFolderNotLinked"},
    {Error::PanoptoPortBusy, "PanoptoPortBusy"},
    {Error::PanoptoNoAccess, "PanoptoNoAccess"},
    {Error::TeachesCourse, "TeachesCourse"},
    {Error::RecordingNotReady, "RecordingNotReady"},
}};
// Adding an Error without a row here fails to compile, instead of silently
// sending "BadRequest" for it.
static_assert(kErrorTable.size() == static_cast<size_t>(Error::TeachesCourse) + 1,
              "every Error needs a row in kErrorTable");

}  // namespace

const char* getErrorCode(Error error) {
    for (const auto& entry : kErrorTable) {
        if (entry.error == error) {
            return entry.code;
        }
    }
    return "BadRequest";
}

Error getErrorFromCode(const std::string& code) {
    for (const auto& entry : kErrorTable) {
        if (code == entry.code) {
            return entry.error;
        }
    }
    return Error::BadRequest;
}