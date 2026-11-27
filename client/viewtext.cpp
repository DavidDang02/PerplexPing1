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

QString formatDuration(Session::Clock::time_point start, Session::Clock::time_point end) {
  const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(end - start);
  if (minutes.count() < 1) {
    return "<1 min";
  }
  if (minutes.count() < 60) {
    return QString("%1 min").arg(minutes.count());
  }
  return QString("%1 h %2 min").arg(minutes.count() / 60).arg(minutes.count() % 60);
}

QString formatOffset(long long seconds) {
  if (seconds < 0) {
    seconds = 0;
  }
  if (seconds >= 3600) {
    return QString("%1:%2:%3")
        .arg(seconds / 3600)
        .arg((seconds / 60) % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
  }
  return QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
}

QString formatStatus(SessionStatus status) {
  switch (status) {
    case SessionStatus::Live:      return "Live now";
    case SessionStatus::InReview:  return "In review";
    case SessionStatus::Published: return "Published";
  }
  return "";
}

QString formatRole(CourseRole role) {
  switch (role) {
    case CourseRole::Educator: return "Educator";
    case CourseRole::Student:  return "Student";
    case CourseRole::None:     return "";
  }
  return "";
}

QString formatAuthor(AuthorState state, const std::string& name) {
  switch (state) {
    case AuthorState::Known:   return QString::fromStdString(name);
    case AuthorState::Hidden:  return "Anonymous";
    case AuthorState::Deleted: return "Deleted user";
  }
  return "";
}

QString formatSessionRow(const SessionSummary& session) {
  QString row = QString::fromStdString(session.name) + "  ·  " + formatClockTime(session.startTime);
  if (session.isLive()) {
    return row + "  ·  " + formatStatus(session.status);
  }
  row += "  ·  " + formatDuration(session.startTime, session.endTime);
  if (session.isInReview() && session.lostCount == 0 && session.questionCount == 0 &&
      !session.hasRecording) {
    // A student's placeholder (S12) or an empty session: just the status.
    return row + "  ·  " + formatStatus(session.status);
  }
  return row + QString("  ·  Lost: %1  ·  Questions: %2  ·  %3")
                   .arg(session.lostCount)
                   .arg(session.questionCount)
                   .arg(formatStatus(session.status));
}

QString formatLiveStatus(const Session& session) {
  return QString("Live  ·  Lost: %1  ·  Questions: %2")
      .arg(session.getPressCount())
      .arg(session.getQuestionCount());
}

QString getNoticeText(AppController::Notice notice) {
  using N = AppController::Notice;
  switch (notice) {
    case N::Registered:       return "Account created. Welcome!";
    case N::LoggedOut:        return "You're logged out.";
    case N::CourseCreated:    return "Course created. Share its course code with your students.";
    case N::CourseJoined:     return "You joined the course.";
    case N::CourseUpdated:    return "Course settings saved.";
    case N::CourseDeleted:    return "The course was deleted.";
    case N::SessionStarted:   return "Session started. Share the session code with your class.";
    case N::SessionJoined:    return "You're in. Press I'm Lost whenever you're confused.";
    case N::LostSent:         return "Got it. Your instructor will see this moment on the timeline.";
    case N::QuestionSent:     return "Question sent.";
    case N::QuestionUpdated:  return "Question updated.";
    case N::QuestionDeleted:  return "Question deleted.";
    case N::SessionEndedByEducator:
      return "Your instructor ended the session. You'll see the timeline and answers once it's published.";
    case N::SessionEnded:     return "Session ended. Review it, then publish it to your students.";
    case N::SessionPublished: return "Published. Your students can see the timeline and answers now.";
    case N::SessionRenamed:   return "Session renamed.";
    case N::SessionDeleted:   return "The session was deleted.";
    case N::AnswerSent:       return "Answer posted.";
    case N::AnswerUpdated:    return "Answer updated.";
    case N::AnswerDeleted:    return "Answer deleted.";
    case N::RecordingAttached: return "Recording attached. Click the timeline to jump to that moment.";
    case N::RecordingRemoved: return "Recording removed.";
    case N::PanoptoLinked:    return "Panopto folder linked. Use Find recording after each class.";
    case N::PasswordChanged:  return "Password changed.";
    case N::EmailUpdated:     return "Email updated.";
    case N::AccountDeleted:   return "Your account was deleted.";
    case N::Reconnected:      return "Reconnected.";
  }
  return "";
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
    case Error::CourseHasLiveSession: return "A session is still running in this course. End it first.";
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
    case Error::RecordingNotReady: return "No recording found for this session yet. Panopto may still be processing it. Try again later, or paste the link.";
    case Error::PanoptoFolderInvalid: return "That isn't a Panopto folder.";
    case Error::PanoptoNotConfigured: return "Panopto isn't set up in this copy of the app. Paste the recording link instead.";
    case Error::PanoptoSignInFailed: return "Panopto sign-in didn't finish. Try again.";
    case Error::PanoptoLinkInvalid: return "Paste a link to one of this course's lectures in Panopto (it has Viewer.aspx?id= in it).";
    case Error::PanoptoUnreachable: return "Couldn't reach Panopto. Check your connection and try again.";
    case Error::PanoptoFolderNotLinked: return "Link this course's Panopto folder first.";
    // ---- App ----
    case Error::ServerUnreachable: return "Can't reach the PerplexPing server. Reconnecting...";
  }
  // Every Error has a sentence above (tests check this); this is only
  // reached for a value outside the enum.
  return QString("Something went wrong (%1).").arg(QString::fromLatin1(getErrorCode(error)));
}

}  // namespace ViewText
