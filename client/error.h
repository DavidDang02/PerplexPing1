// MODEL: every reason a request can be refused, as one enum shared by the
// model, the server, the protocol and the app. Plain C++ (no Qt).
//
// The model returns an Error (None = it worked). The server sends its code
// text ("QuestionTooLong") in an "error" message. The VIEW turns it into a
// sentence (view/viewtext.h); nobody else writes user-facing text.
//
// Owner: Model layer. Adding a value = add it here, in getErrorCode(), in
// ViewText::getErrorText() (View owner) and in CONTRACT.md 9.4. Rule numbers
// refer to CONTRACT.md.
//
// "NoSuch..." is also the answer when the thing exists but the user may not
// know that (not a member of the course): the server never confirms that a
// course, session, input or answer exists to someone who can't see it.

#pragma once

#include <string>
#include <utility>

enum class Error {
    None,

    // ---- Protocol / server (N rules) ----
    BadRequest,      // not JSON, no "type", or a field is missing / has the wrong type
    UnknownRequest,  // "type" is not in the catalogue
    NotLoggedIn,     // U9
    StorageError,    // the database refused; should never happen

    // ---- Accounts (U rules) ----
    EmptyName,
    NameTooShort,
    NameTooLong,
    NameBadCharacters,
    NameTaken,
    EmptyPassword,
    PasswordTooShort,
    PasswordTooLong,
    WrongLogin,     // U5: name or password wrong, never says which
    WrongPassword,  // U7/U8: the confirmation password is wrong
    EmptyEmail,     // U6: an email is required
    EmailInvalid,
    EmailNotSchool,
    EmailTaken,
    HostingLiveSession,  // U8: end your live session before deleting the account

    // ---- Courses (C rules) ----
    EmptyCourseName,
    CourseNameTooLong,
    NoSuchCourse,      // also "you are not a member" (C5)
    NoSuchCourseCode,
    NotCourseEducator,  // you are a student in this course

    // ---- Sessions (S rules) ----
    EmptySessionName,
    SessionNameTooLong,
    NoSuchSession,        // also "you are not a member of its course" (S6)
    NoSuchSessionCode,    // unknown code, or that session has ended (S5)
    CourseHasLiveSession, // S3
    SessionEnded,         // I2, I6 (freeze), S7
    SessionNotEnded,      // still live: A1 (answer), I7 (moderate), R1 (recording), S11 (publish)
    SessionNotPublished,  // S12: a student asked for a session that is still in review
    SessionAlreadyPublished,  // S11

    // ---- Inputs (I rules) ----
    NotAStudent,  // I1: educators don't send inputs
    EmptyQuestion,
    QuestionTooLong,
    LostTooSoon,  // I8 (stretch)
    NoSuchInput,  // no such Lost ping or question, or "you may not see it" (A2)
    NotYourInput, // a Lost ping or question someone else sent

    // ---- Answers (A rules) ----
    EmptyAnswer,
    AnswerTooLong,
    NoSuchAnswer,
    NotYourAnswer,

    // ---- Recording (R rules) ----
    RecordingUrlInvalid,
    RecordingOffsetInvalid,
    RecordingNotReady,  // R6: no Panopto recording overlaps the session (yet)
};

// The stable text sent over the network: exactly the enumerator's name,
// e.g. Error::QuestionTooLong -> "QuestionTooLong".
const char* getErrorCode(Error error);

// The reverse. Unknown text gives Error::BadRequest.
Error getErrorFromCode(const std::string& code);

// What a model call gives back: an error, or a value.
//   Result<Course> made = model.createCourse("CSCI 187", false, true);
//   if (!made.isOk()) { ...made.error... } else { ...made.value... }
// `value` is only meaningful when isOk().
template <typename T>
struct Result {
    Error error = Error::None;
    T value{};

    Result() = default;
    Result(Error error) : error(error) {}                           // failure
    Result(T value) : error(Error::None), value(std::move(value)) {}  // success

    bool isOk() const { return error == Error::None; }
    bool has_value() const { return isOk(); }
};