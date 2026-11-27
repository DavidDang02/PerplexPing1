// NEW CONTRACT PROTOCOL: JSON <-> model object conversion.
// Compiled into BOTH the app and the server. Uses Qt Core only.

#include "protocol.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace Protocol {

QJsonObject makeMessage(const QString& type, QJsonObject fields) {
    QJsonObject obj;
    obj[Field::kType] = type;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        obj[it.key()] = it.value();
    }
    return obj;
}

// ---- Time <-> milliseconds ----

qint64 toMs(std::chrono::system_clock::time_point time) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch());
    return ms.count();
}

std::chrono::system_clock::time_point fromMs(qint64 ms) {
    return std::chrono::system_clock::time_point(
        std::chrono::milliseconds(ms));
}

// ---- User ----

QJsonObject toJson(const User& user) {
    QJsonObject obj;
    obj[Field::kId] = user.getId();
    obj[Field::kName] = QString::fromStdString(user.getName());
    obj[Field::kEmail] = QString::fromStdString(user.getEmail());
    return obj;
}

bool userFromJson(const QJsonObject& obj, User& user) {
    int id = obj[Field::kId].toInt(0);
    QString name = obj[Field::kName].toString();
    QString email = obj[Field::kEmail].toString();
    user = User(id, name.toStdString(), email.toStdString());
    return true;
}

// ---- Course ----

QJsonObject toJson(const Course& course) {
    QJsonObject obj;
    obj[Field::kId] = course.getId();
    obj[Field::kName] = QString::fromStdString(course.getName());
    obj[Field::kJoinCode] = QString::fromStdString(course.getJoinCode());
    obj[Field::kAnonymous] = course.isAnonymous();
    obj[Field::kEducatorId] = course.getEducatorId();
    return obj;
}

QJsonArray toJson(const std::vector<Course>& courses) {
    QJsonArray arr;
    for (const auto& c : courses) {
        arr.append(toJson(c));
    }
    return arr;
}

bool courseFromJson(const QJsonObject& obj, Course& course) {
    course.setId(obj[Field::kId].toInt(0));
    course.setName(obj[Field::kName].toString().toStdString());
    course.setJoinCode(obj[Field::kJoinCode].toString().toStdString());
    course.setAnonymous(obj[Field::kAnonymous].toInt(0));
    course.setQuestionsShared(obj[Field::kQuestionsShared].toInt(0));
    course.setEducatorId(obj[Field::kEducatorId].toInt(0));
    return true;
}

// ---- Session ----

QJsonObject toJson(const Session& session) {
    QJsonObject obj;
    obj[Field::kId] = session.getId();
    obj[Field::kName] = QString::fromStdString(session.getName());
    obj[Field::kSessionId] = session.getId();
    obj[Field::kHostId] = session.getHostId();
    obj[Field::kStartTime] = toMs(session.getStartTime());
    obj[Field::kStatus] = toText(session.getStatus());
    obj[Field::kCourseId] = session.getCourseId();
    obj[Field::kJoinCode] = QString::fromStdString(session.getJoinCode());
    if (session.isEnded()) {
        obj[Field::kEndTime] = toMs(session.getEndTime());
    }
    if (session.isPublished()) {
        obj[Field::kPublishedTime] = toMs(session.getPublishedTime());
    }
    obj[Field::kLostCount] = session.getPressCount();
    obj[Field::kQuestionCount] = session.getQuestionCount();
    obj[Field::kHasRecording] = session.getRecording() != nullptr;
    if (session.getRecording()) {
        obj[Field::kRecording] = toJson(*session.getRecording());
    }
    // The items this viewer may see (the server runs Visibility first).
    QJsonArray pings;
    for (const auto& ping : session.getLostPings()) {
        pings.append(toJson(ping));
    }
    obj[Field::kLostPings] = pings;
    QJsonArray questions;
    for (const auto& question : session.getQuestions()) {
        questions.append(toJson(question));
    }
    obj[Field::kQuestions] = questions;
    return obj;
}

bool sessionFromJson(const QJsonObject& obj, Session& session) {
    session.setId(obj[Field::kId].toInt(0));
    session.setName(obj[Field::kName].toString().toStdString());
    session.setCourseId(obj[Field::kCourseId].toInt(0));
    session.setJoinCode(obj[Field::kJoinCode].toString().toStdString());
    // Note: startTime and endTime need careful handling if loading from DB
    return true;
}

// ---- LostPing ----

QJsonObject toJson(const LostPing& ping) {
    QJsonObject obj;
    obj[Field::kPingId] = ping.getId();
    obj[Field::kSessionId] = ping.getSessionId();
    obj[Field::kTime] = toMs(ping.getTime());
    // Always all three (V4): id 0 and name "" unless the author is known.
    obj[Field::kAuthorState] = toText(ping.getAuthorState());
    obj[Field::kAuthorId] = ping.hasAuthor() ? ping.getAuthorId() : 0;
    obj[Field::kAuthorName] = ping.hasAuthor() ? QString::fromStdString(ping.getAuthorName()) : QString();
    return obj;
}

bool lostPingFromJson(const QJsonObject& obj, LostPing& ping) {
    ping.setId(obj[Field::kPingId].toInt(0));
    ping.setSessionId(obj[Field::kSessionId].toInt(0));
    // Time handling would need careful implementation
    return true;
}

// ---- SessionQuestion ----

QJsonObject toJson(const SessionQuestion& question) {
    QJsonObject obj;
    obj[Field::kQuestionId] = question.getId();
    obj[Field::kSessionId] = question.getSessionId();
    obj[Field::kTime] = toMs(question.getTime());
    obj[Field::kText] = QString::fromStdString(question.getText());
    if (question.isEdited()) {
        obj[Field::kEditedTime] = toMs(*question.getEditedTime());
    }
    QJsonArray answers;
    for (const auto& answer : question.getAnswers()) {
        answers.append(toJson(answer));
    }
    obj[Field::kAnswers] = answers;
    // Always all three (V4): id 0 and name "" unless the author is known.
    obj[Field::kAuthorState] = toText(question.getAuthorState());
    obj[Field::kAuthorId] = question.hasAuthor() ? question.getAuthorId() : 0;
    obj[Field::kAuthorName] = question.hasAuthor() ? QString::fromStdString(question.getAuthorName()) : QString();
    return obj;
}

bool questionFromJson(const QJsonObject& obj, SessionQuestion& question) {
    question.setId(obj[Field::kQuestionId].toInt(0));
    question.setSessionId(obj[Field::kSessionId].toInt(0));
    return true;
}

// ---- Answer ----

QJsonObject toJson(const Answer& answer) {
    QJsonObject obj;
    obj[Field::kAnswerId] = answer.getId();
    obj[Field::kQuestionId] = answer.getQuestionId();
    obj[Field::kTime] = toMs(answer.getTime());
    obj[Field::kText] = QString::fromStdString(answer.getText());
    obj[Field::kByEducator] = answer.isByEducator();
    if (answer.isEdited()) {
        obj[Field::kEditedTime] = toMs(*answer.getEditedTime());
    }
    // Always all three (V4): id 0 and name "" unless the author is known.
    obj[Field::kAuthorState] = toText(answer.getAuthorState());
    obj[Field::kAuthorId] = answer.hasAuthor() ? answer.getAuthorId() : 0;
    obj[Field::kAuthorName] = answer.hasAuthor() ? QString::fromStdString(answer.getAuthorName()) : QString();
    return obj;
}

// ---- Recording ----

QJsonObject toJson(const Recording& recording) {
    QJsonObject obj;
    obj[Field::kUrl] = QString::fromStdString(recording.getViewerUrl());
    obj[Field::kOffsetSeconds] = recording.getOffsetSeconds();
    obj[Field::kSource] = toText(recording.getSource());
    return obj;
}

bool recordingFromJson(const QJsonObject& obj, Recording& recording) {
    return true;
}

// ---- Recording candidates (checkRecording) ----

QJsonObject toJson(const RecordingCandidate& candidate) {
    QJsonObject obj;
    obj[Field::kUrl] = QString::fromStdString(candidate.viewerUrl);
    obj[Field::kName] = QString::fromStdString(candidate.name);
    obj[Field::kStartTime] = toMs(candidate.start);
    obj[Field::kDurationSeconds] = candidate.durationSeconds;
    return obj;
}

QJsonArray toJson(const std::vector<RecordingCandidate>& candidates) {
    QJsonArray arr;
    for (const auto& candidate : candidates) {
        arr.append(toJson(candidate));
    }
    return arr;
}

std::vector<RecordingCandidate> candidatesFromJson(const QJsonArray& json) {
    std::vector<RecordingCandidate> candidates;
    for (const QJsonValue& value : json) {
        const QJsonObject obj = value.toObject();
        RecordingCandidate candidate;
        candidate.viewerUrl = obj[Field::kUrl].toString().toStdString();
        candidate.name = obj[Field::kName].toString().toStdString();
        candidate.start = fromMs(obj[Field::kStartTime].toInteger());
        candidate.durationSeconds = obj[Field::kDurationSeconds].toInteger();
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

// ---- Participant ----

QJsonObject toJson(const Participant& p) {
    QJsonObject obj;
    obj[Field::kUserId] = p.userId;
    obj[Field::kName] = QString::fromStdString(p.name);
    return obj;
}

bool participantFromJson(const QJsonObject& obj, Participant& p) {
    p.userId = obj[Field::kUserId].toInt(0);
    p.name = obj[Field::kName].toString().toStdString();
    return true;
}

// ---- SessionSummary ----

QJsonObject toJson(const SessionSummary& s) {
    QJsonObject obj;
    obj[Field::kSessionId] = s.id;
    obj[Field::kName] = QString::fromStdString(s.name);
    obj[Field::kStartTime] = toMs(s.startTime);
    obj[Field::kStatus] = toText(s.status);
    obj[Field::kCourseId] = s.courseId;
    if (s.endTime != std::chrono::system_clock::time_point{}) {
        obj[Field::kEndTime] = toMs(s.endTime);
    }
    return obj;
}

bool summaryFromJson(const QJsonObject& obj, SessionSummary& s) {
    s.id = obj[Field::kSessionId].toInt(0);
    s.name = obj[Field::kName].toString().toStdString();
    s.courseId = obj[Field::kCourseId].toInt(0);
    return true;
}

// ---- Vectors ----

QJsonArray toJson(const std::vector<SessionSummary>& summaries) {
    QJsonArray arr;
    for (const auto& s : summaries) {
        arr.append(toJson(s));
    }
    return arr;
}

QJsonArray toJson(const std::vector<Participant>& participants) {
    QJsonArray arr;
    for (const auto& p : participants) {
        arr.append(toJson(p));
    }
    return arr;
}

// ---- Text converters ----

QString toText(CourseRole role) {
    switch (role) {
        case CourseRole::Educator: return QStringLiteral("educator");
        case CourseRole::Student: return QStringLiteral("student");
        default: return QString();
    }
}

CourseRole roleFromText(const QString& text) {
    if (text == "educator") return CourseRole::Educator;
    if (text == "student") return CourseRole::Student;
    return CourseRole::None;
}

QString toText(RecordingSource source) {
    switch (source) {
        case RecordingSource::Manual: return QStringLiteral("manual");
        case RecordingSource::Panopto: return QStringLiteral("panopto");
        default: return QString();
    }
}

RecordingSource sourceFromText(const QString& text) {
    if (text == "manual") return RecordingSource::Manual;
    if (text == "panopto") return RecordingSource::Panopto;
    return RecordingSource::Manual;
}

QString toText(SessionStatus status) {
    switch (status) {
        case SessionStatus::Live: return QStringLiteral("live");
        case SessionStatus::InReview: return QStringLiteral("inReview");
        case SessionStatus::Published: return QStringLiteral("published");
        default: return QStringLiteral("live");
    }
}

SessionStatus statusFromText(const QString& text) {
    if (text == "inReview") return SessionStatus::InReview;
    if (text == "published") return SessionStatus::Published;
    return SessionStatus::Live;
}

QString toText(AuthorState state) {
    switch (state) {
        case AuthorState::Known: return QStringLiteral("known");
        case AuthorState::Hidden: return QStringLiteral("hidden");
        case AuthorState::Deleted: return QStringLiteral("deleted");
        default: return QStringLiteral("hidden");
    }
}

AuthorState authorStateFromText(const QString& text) {
    if (text == "known") return AuthorState::Known;
    if (text == "deleted") return AuthorState::Deleted;
    return AuthorState::Hidden;
}

// ---- Whole message helpers ----

QJsonObject makeError(const QString& requestType, Error error) {
    QJsonObject obj;
    obj[Field::kType] = Reply::kError;
    obj[Field::kRequest] = requestType;
    obj[Field::kCode] = QString::fromStdString(getErrorCode(error));
    return obj;
}

Error getError(const QJsonObject& errorMessage) {
    QString code = errorMessage[Field::kCode].toString();
    return getErrorFromCode(code.toStdString());
}

bool hasFields(const QJsonObject& message,
               std::initializer_list<std::pair<QString, char>> fields) {
    for (const auto& [key, type] : fields) {
        if (!message.contains(key)) return false;
        QJsonValue val = message[key];
        switch (type) {
            case 's': if (!val.isString()) return false; break;
            case 'i': if (!val.isDouble()) return false; break;
            case 'b': if (!val.isBool()) return false; break;
            case 'o': if (!val.isObject()) return false; break;
        }
    }
    return true;
}

QString encode(const QJsonObject& message) {
    QJsonDocument doc(message);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

QJsonObject decode(const QString& text) {
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

}  // namespace Protocol