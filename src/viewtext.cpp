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
    default:
      return QString("Something went wrong (%1).").arg(QString::fromLatin1(getErrorCode(error)));
  }
}

}  // namespace ViewText
