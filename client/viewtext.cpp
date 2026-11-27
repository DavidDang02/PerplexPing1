#include "viewtext.h"

#include <chrono>

#include <QDateTime>

#include "validation.h"

namespace ViewText {

QString formatClockTime(Session::Clock::time_point time) {
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      time.time_since_epoch());
  return QDateTime::fromMSecsSinceEpoch(ms.count()).toString("MMM d, h:mm AP");
}

QString formatDuration(const Session& session) {
  if (!session.isEnded()) {
    return "no end time";
  }
  const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
      session.getEndTime() - session.getStartTime());
  if (minutes.count() < 1) {
    return "<1 min";
  }
  return QString("%1 min").arg(minutes.count());
}

QString formatOffset(long long seconds) {
  return QString("%1:%2 in")
      .arg(seconds / 60, 2, 10, QChar('0'))
      .arg(seconds % 60, 2, 10, QChar('0'));
}

QString formatInput(const Session& session, const SessionInput& input) {
  const QString when = formatOffset(session.getOffsetSeconds(input));
  if (input.getType() == InputType::Question) {
    return when + " · Q: " + QString::fromStdString(input.getQuestionText());
  }
  return when + " · I'm lost";
}

QString formatSessionRow(const Session& session) {
  return QString("%1  ·  %2  ·  Lost: %3  ·  Questions: %4")
      .arg(formatClockTime(session.getStartTime()))
      .arg(formatDuration(session))
      .arg(session.getPressCount())
      .arg(session.getQuestionCount());
}

QString formatLiveStatus(const Session& session) {
  return QString("Session in progress  ·  Lost: %1  ·  Questions: %2")
      .arg(session.getPressCount())
      .arg(session.getQuestionCount());
}

QString getErrorText(Error error) {
  switch (error) {
    case Error::None:              return "";
    case Error::EmptyName:         return "Enter a username.";
    case Error::NameTooShort:
      return QString("Usernames need at least %1 characters.").arg(Validation::kMinNameLength);
    case Error::NameTooLong:
      return QString("Usernames can be at most %1 characters.").arg(Validation::kMaxNameLength);
    case Error::NameBadCharacters:
      return "Usernames can only use letters, numbers, dots, dashes and underscores.";
    case Error::NameTaken:         return "That username is already taken.";
    case Error::EmptyPassword:     return "Enter a password.";
    case Error::PasswordTooShort:
      return QString("Passwords need at least %1 characters.").arg(Validation::kMinPasswordLength);
    case Error::PasswordTooLong:
      return QString("Passwords can be at most %1 characters.").arg(Validation::kMaxPasswordLength);
    case Error::WrongLogin:        return "Wrong username or password.";
    case Error::NotLoggedIn:       return "Please log in again.";
    case Error::NoSuchSession:     return "That session doesn't exist.";
    case Error::SessionEnded:      return "This session has ended.";
    case Error::EmptyQuestion:     return "Type a question before sending.";
    case Error::QuestionTooLong:
      return QString("Questions can be at most %1 characters.").arg(Validation::kMaxQuestionLength);
    case Error::CourseHasLiveSession: return "A session is already running.";
    case Error::StorageError:      return "Couldn't save. Please try again.";
    // ---- Protocol ----
    case Error::BadRequest:        return "The app sent something the server didn't understand. Please update the app.";
    case Error::UnknownRequest:    return "The server doesn't know this action. Please update the app or the server.";
    // ---- Accounts ----
    case Error::WrongPassword:     return "That password isn't right.";
    case Error::EmptyEmail:        return "Enter your school email.";
    case Error::EmailInvalid:      return "That doesn't look like an email address.";
    case Error::EmailNotSchool:
      return QString("Use your school email (ending in %1).").arg(Validation::kSchoolEmailDomain);
    case Error::EmailTaken:        return "That email already has an account.";
    case Error::HostingLiveSession: return "End your live session before deleting your account.";
    // ---- Courses ----
    case Error::EmptyCourseName:   return "Enter a course name.";
    case Error::CourseNameTooLong:
      return QString("Course names can be at most %1 characters.").arg(Validation::kMaxCourseNameLength);
    case Error::NoSuchCourse:      return "That course doesn't exist, or you're not in it.";
    case Error::NoSuchCourseCode:  return "No course has that code. Check it with your instructor.";
    case Error::NotCourseEducator: return "Only the course's educator can do that.";
    // ---- Sessions ----
    case Error::EmptySessionName:  return "Enter a session name.";
    case Error::SessionNameTooLong:
      return QString("Session names can be at most %1 characters.").arg(Validation::kMaxSessionNameLength);
    case Error::NoSuchSessionCode: return "No live session has that code. It may have ended.";
    case Error::SessionNotEnded:   return "End the session first.";
    case Error::SessionNotPublished: return "Your instructor hasn't published this session yet.";
    case Error::SessionAlreadyPublished: return "This session is already published.";
    // ---- Inputs ----
    case Error::NotAStudent:       return "Only students can send I'm Lost or questions.";
    case Error::LostTooSoon:       return "You just pressed I'm Lost. Wait a few seconds.";
    case Error::NoSuchInput:       return "That item doesn't exist anymore.";
    case Error::NotYourInput:      return "You can only change your own items.";
    // ---- Answers ----
    case Error::EmptyAnswer:       return "Type an answer before sending.";
    case Error::AnswerTooLong:
      return QString("Answers can be at most %1 characters.").arg(Validation::kMaxAnswerLength);
    case Error::NoSuchAnswer:      return "That answer doesn't exist anymore.";
    case Error::NotYourAnswer:     return "You can only change your own answers.";
    // ---- Recording ----
    case Error::RecordingUrlInvalid: return "Paste a full http(s) link to the recording.";
    case Error::RecordingOffsetInvalid: return "The start time must be within 24 hours.";
    case Error::RecordingNotReady: return "No recording found for this session yet. Panopto may still be processing it.";
  }
  // Every Error has a sentence above (tests check this); this is only
  // reached for a value outside the enum.
  return QString("Something went wrong (%1).").arg(QString::fromLatin1(getErrorCode(error)));
}

}  // namespace ViewText
