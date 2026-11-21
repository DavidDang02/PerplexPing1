// PROTOCOL: the messages the app and the server send each other, as JSON
// text over a WebSocket, and the functions that turn model objects into
// JSON and back so both sides agree. Compiled into BOTH the app and the
// server. Uses Qt Core only (QJson...), no sockets.
//
// Every message is one JSON object with a "type":
//     app -> server   {"type":"addQuestion","sessionId":7,"text":"why?"}
//     server -> app   {"type":"inputAdded","sessionId":7,"input":{...}}
// The full catalogue (fields, who may send, who receives) is CONTRACT.md
// section 6. This header is its code form: use these constants, never a
// string typed by hand, so a typo is a compile error.
//
// JSON conventions:
//   * ids are integers; 0 means "none / hidden"
//   * times are integers: milliseconds since 1970-01-01 UTC
//   * an author is three fields: "authorState" ("known" | "hidden" |
//     "deleted"), "authorId" and "authorName" (0 and "" unless known) (V4)
//   * optional fields are simply left out (e.g. "endTime" of a live session)
//   * unknown extra fields are ignored by both sides
//
// Owner: Controller/Server layer (+ tests/test_protocol.cpp: every toJson -> fromJson round trip).

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <chrono>
#include <initializer_list>
#include <utility>
#include <vector>

#include "answer.h"
#include "course.h"
#include "error.h"
#include "recording.h"
#include "session.h"
#include "lostping.h"
#include "sessionquestion.h"
#include "user.h"

namespace Protocol {

// Sent in "hello". Bump when a message changes shape.
inline constexpr int kVersion = 2;
inline constexpr quint16 kDefaultPort = 12345;
inline const QString kDefaultServer = QStringLiteral("ws://localhost:12345");
// The server drops any message longer than this (bytes).
inline constexpr int kMaxMessageBytes = 64 * 1024;
// "checkRecording" carries at most this many candidates (newest first).
inline constexpr int kMaxCandidates = 50;

// ---- Requests: app -> server ----
namespace Request {
inline const QString kRegister = QStringLiteral("register");
inline const QString kLogin = QStringLiteral("login");
inline const QString kLogout = QStringLiteral("logout");
inline const QString kChangePassword = QStringLiteral("changePassword");
inline const QString kUpdateEmail = QStringLiteral("updateEmail");
inline const QString kDeleteAccount = QStringLiteral("deleteAccount");

inline const QString kListCourses = QStringLiteral("listCourses");
inline const QString kCreateCourse = QStringLiteral("createCourse");
inline const QString kJoinCourse = QStringLiteral("joinCourse");
inline const QString kGetCourse = QStringLiteral("getCourse");
inline const QString kUpdateCourse = QStringLiteral("updateCourse");
inline const QString kDeleteCourse = QStringLiteral("deleteCourse");    // stretch

inline const QString kStartSession = QStringLiteral("startSession");
inline const QString kJoinSession = QStringLiteral("joinSession");
inline const QString kOpenSession = QStringLiteral("openSession");
inline const QString kCloseSession = QStringLiteral("closeSession");
inline const QString kEndSession = QStringLiteral("endSession");
inline const QString kPublishSession = QStringLiteral("publishSession");
inline const QString kRenameSession = QStringLiteral("renameSession");
inline const QString kDeleteSession = QStringLiteral("deleteSession");

inline const QString kAddLost = QStringLiteral("addLost");
inline const QString kDeleteLost = QStringLiteral("deleteLost");
inline const QString kAddQuestion = QStringLiteral("addQuestion");
inline const QString kEditQuestion = QStringLiteral("editQuestion");
inline const QString kDeleteQuestion = QStringLiteral("deleteQuestion");

inline const QString kAddAnswer = QStringLiteral("addAnswer");
inline const QString kEditAnswer = QStringLiteral("editAnswer");
inline const QString kDeleteAnswer = QStringLiteral("deleteAnswer");

inline const QString kAttachRecording = QStringLiteral("attachRecording");
inline const QString kRemoveRecording = QStringLiteral("removeRecording");
inline const QString kCheckRecording = QStringLiteral("checkRecording");  // auto-match
}  // namespace Request

// ---- Replies and broadcasts: server -> app ----
namespace Reply {
inline const QString kHello = QStringLiteral("hello");
inline const QString kError = QStringLiteral("error");

inline const QString kAuth = QStringLiteral("auth");
inline const QString kLoggedOut = QStringLiteral("loggedOut");
inline const QString kPasswordChanged = QStringLiteral("passwordChanged");
inline const QString kProfileUpdated = QStringLiteral("profileUpdated");

inline const QString kCourses = QStringLiteral("courses");
inline const QString kCourseCreated = QStringLiteral("courseCreated");
inline const QString kCourseJoined = QStringLiteral("courseJoined");
inline const QString kCourseDetail = QStringLiteral("courseDetail");
inline const QString kCourseUpdated = QStringLiteral("courseUpdated");
inline const QString kCourseDeleted = QStringLiteral("courseDeleted");    // stretch
inline const QString kCourseChanged = QStringLiteral("courseChanged");

inline const QString kSessionOpened = QStringLiteral("sessionOpened");
inline const QString kSessionClosed = QStringLiteral("sessionClosed");
inline const QString kSessionEnded = QStringLiteral("sessionEnded");
inline const QString kSessionPublished = QStringLiteral("sessionPublished");
inline const QString kSessionRenamed = QStringLiteral("sessionRenamed");
inline const QString kSessionDeleted = QStringLiteral("sessionDeleted");
inline const QString kParticipants = QStringLiteral("participants");

inline const QString kLostAdded = QStringLiteral("lostAdded");
inline const QString kLostDeleted = QStringLiteral("lostDeleted");
inline const QString kQuestionAdded = QStringLiteral("questionAdded");
inline const QString kQuestionUpdated = QStringLiteral("questionUpdated");
inline const QString kQuestionDeleted = QStringLiteral("questionDeleted");

inline const QString kAnswerAdded = QStringLiteral("answerAdded");
inline const QString kAnswerUpdated = QStringLiteral("answerUpdated");
inline const QString kAnswerDeleted = QStringLiteral("answerDeleted");

inline const QString kRecordingAttached = QStringLiteral("recordingAttached");
inline const QString kRecordingRemoved = QStringLiteral("recordingRemoved");
}  // namespace Reply

// ---- Field names (the keys inside the objects) ----
namespace Field {
inline const QString kType = QStringLiteral("type");
inline const QString kRequest = QStringLiteral("request");  // in "error": the type that failed
inline const QString kCode = QStringLiteral("code");        // in "error": getErrorCode()
inline const QString kReason = QStringLiteral("reason");    // in "loggedOut"
inline const QString kProtocol = QStringLiteral("protocol");
inline const QString kServerTime = QStringLiteral("serverTime");

inline const QString kId = QStringLiteral("id");
inline const QString kName = QStringLiteral("name");
inline const QString kPassword = QStringLiteral("password");
inline const QString kOldPassword = QStringLiteral("oldPassword");
inline const QString kNewPassword = QStringLiteral("newPassword");
inline const QString kEmail = QStringLiteral("email");
inline const QString kUser = QStringLiteral("user");
inline const QString kUserId = QStringLiteral("userId");

inline const QString kCourse = QStringLiteral("course");
inline const QString kCourses = QStringLiteral("courses");
inline const QString kCourseId = QStringLiteral("courseId");
inline const QString kJoinCode = QStringLiteral("joinCode");
inline const QString kRole = QStringLiteral("role");  // in a course: "educator" | "student"
inline const QString kAnonymous = QStringLiteral("anonymous");
inline const QString kQuestionsShared = QStringLiteral("questionsShared");
inline const QString kEducatorId = QStringLiteral("educatorId");
inline const QString kStudentIds = QStringLiteral("studentIds");
inline const QString kSessionIds = QStringLiteral("sessionIds");
inline const QString kLiveSessionId = QStringLiteral("liveSessionId");

inline const QString kSession = QStringLiteral("session");
inline const QString kSessions = QStringLiteral("sessions");
inline const QString kSessionId = QStringLiteral("sessionId");
inline const QString kHostId = QStringLiteral("hostId");
inline const QString kStartTime = QStringLiteral("startTime");
inline const QString kEndTime = QStringLiteral("endTime");
inline const QString kStatus = QStringLiteral("status");  // "live" | "inReview" | "published"
inline const QString kPublishedTime = QStringLiteral("publishedTime");
inline const QString kLostCount = QStringLiteral("lostCount");
inline const QString kQuestionCount = QStringLiteral("questionCount");
inline const QString kHasRecording = QStringLiteral("hasRecording");
inline const QString kNewlyEnrolled = QStringLiteral("newlyEnrolled");
inline const QString kCount = QStringLiteral("count");
inline const QString kOk = QStringLiteral("ok");
inline const QString kData = QStringLiteral("data");
inline const QString kParticipants = QStringLiteral("participants");

inline const QString kLostPing = QStringLiteral("lostPing");
inline const QString kLostPings = QStringLiteral("lostPings");
inline const QString kPingId = QStringLiteral("pingId");
inline const QString kQuestion = QStringLiteral("question");
inline const QString kQuestions = QStringLiteral("questions");
inline const QString kQuestionId = QStringLiteral("questionId");
inline const QString kTime = QStringLiteral("time");
inline const QString kText = QStringLiteral("text");
inline const QString kEditedTime = QStringLiteral("editedTime");
inline const QString kAuthorId = QStringLiteral("authorId");
inline const QString kAuthorName = QStringLiteral("authorName");
inline const QString kAuthorState = QStringLiteral("authorState");  // "known" | "hidden" | "deleted"
inline const QString kByModerator = QStringLiteral("byModerator");

inline const QString kAnswer = QStringLiteral("answer");
inline const QString kAnswers = QStringLiteral("answers");
inline const QString kAnswerId = QStringLiteral("answerId");
inline const QString kByEducator = QStringLiteral("byEducator");

inline const QString kRecording = QStringLiteral("recording");
inline const QString kUrl = QStringLiteral("url");
inline const QString kOffsetSeconds = QStringLiteral("offsetSeconds");
inline const QString kSource = QStringLiteral("source");  // "manual" | "panopto"
inline const QString kCandidates = QStringLiteral("candidates");  // in "checkRecording"
inline const QString kDurationSeconds = QStringLiteral("durationSeconds");
}  // namespace Field

// "loggedOut" reasons.
inline const QString kReasonLogout = QStringLiteral("logout");
inline const QString kReasonAccountDeleted = QStringLiteral("accountDeleted");

// ---- Model object <-> JSON (shapes: CONTRACT.md) ----
// toJson writes exactly what the object holds. It does NOT hide anything:
// the server must pass only objects that already went through Visibility.
QJsonObject toJson(const User& user);
QJsonObject toJson(const Participant& participant);
QJsonObject toJson(const Course& course);
QJsonObject toJson(const SessionSummary& summary);
QJsonObject toJson(const Session& session);  // details + recording + pings + questions + answers
QJsonObject toJson(const LostPing& ping);
QJsonObject toJson(const SessionQuestion& question);
QJsonObject toJson(const Answer& answer);
QJsonObject toJson(const Recording& recording);
QJsonObject toJson(const RecordingCandidate& candidate);
QJsonArray toJson(const std::vector<RecordingCandidate>& candidates);
QJsonArray toJson(const std::vector<Course>& courses);
QJsonArray toJson(const std::vector<SessionSummary>& summaries);
QJsonArray toJson(const std::vector<Participant>& participants);

// A missing or wrongly typed field becomes that type's zero ("" / 0 /
// false); these never throw. Check required fields with hasFields() first.
User userFromJson(const QJsonObject& json);
Participant participantFromJson(const QJsonObject& json);
Course courseFromJson(const QJsonObject& json);
SessionSummary summaryFromJson(const QJsonObject& json);
Session sessionFromJson(const QJsonObject& json);
LostPing lostPingFromJson(const QJsonObject& json);
SessionQuestion questionFromJson(const QJsonObject& json);
Answer answerFromJson(const QJsonObject& json);
Recording recordingFromJson(const QJsonObject& json);
std::vector<RecordingCandidate> candidatesFromJson(const QJsonArray& json);
std::vector<Course> coursesFromJson(const QJsonArray& json);
std::vector<SessionSummary> summariesFromJson(const QJsonArray& json);
std::vector<Participant> participantsFromJson(const QJsonArray& json);

// Times <-> milliseconds since 1970 UTC.
qint64 toMs(std::chrono::system_clock::time_point time);
std::chrono::system_clock::time_point fromMs(qint64 ms);

// "educator" / "student" / "" (None) and back; "manual" / "panopto";
// "known" / "hidden" / "deleted". Unknown text gives None / Manual / Hidden.
QString toText(CourseRole role);
CourseRole roleFromText(const QString& text);
QString toText(RecordingSource source);
RecordingSource sourceFromText(const QString& text);
QString toText(SessionStatus status);  // "live" / "inReview" / "published"; unknown -> Live
SessionStatus statusFromText(const QString& text);
QString toText(AuthorState state);
AuthorState authorStateFromText(const QString& text);

// ---- Whole messages ----
// {"type": type, ...fields}
QJsonObject makeMessage(const QString& type, QJsonObject fields = {});
// {"type":"error","request": requestType,"code": getErrorCode(error)}
QJsonObject makeError(const QString& requestType, Error error);
// The Error inside an "error" message.
Error getError(const QJsonObject& errorMessage);

// True if every listed key is present in `message` with the JSON type
// given: 's' string, 'i' whole number, 'b' bool, 'o' object.
//   hasFields(m, {{Field::kSessionId, 'i'}, {Field::kText, 's'}})
// The server answers BadRequest when this is false (N3).
bool hasFields(const QJsonObject& message,
               std::initializer_list<std::pair<QString, char>> fields);

// Compact text for sending, and back. Text that is not a JSON object
// decodes to an empty object (which has no "type" -> BadRequest).
QString encode(const QJsonObject& message);
QJsonObject decode(const QString& text);

}  // namespace Protocol
