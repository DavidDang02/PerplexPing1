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


// ---- Course ----

QJsonObject toJson(const Course& course) {
    QJsonObject obj;
    obj[Field::kId] = course.getId();
    obj[Field::kName] = QString::fromStdString(course.getName());
    obj[Field::kJoinCode] = QString::fromStdString(course.getJoinCode());
    obj[Field::kAnonymous] = course.isAnonymous();
    obj[Field::kQuestionsShared] = course.areQuestionsShared();
    obj[Field::kEducatorId] = course.getEducatorId();
    obj[Field::kRole] = toText(course.getRole());
    if (course.getLiveSessionId() != 0) {
        obj[Field::kLiveSessionId] = course.getLiveSessionId();
    }
    if (course.hasPanoptoFolder()) {
        obj[Field::kPanoptoFolderId] = QString::fromStdString(course.getPanoptoFolderId());
    }
    QJsonArray students;
    for (int id : course.getStudentIds()) {
        students.append(id);
    }
    obj[Field::kStudentIds] = students;
    QJsonArray sessions;
    for (int id : course.getSessionIds()) {
        sessions.append(id);
    }
    obj[Field::kSessionIds] = sessions;
    return obj;
}

QJsonArray toJson(const std::vector<Course>& courses) {
    QJsonArray arr;
    for (const auto& c : courses) {
        arr.append(toJson(c));
    }
    return arr;
}


// ---- Session ----

QJsonObject toDetailsJson(const Session& session) {
    QJsonObject obj = toJson(session);
    // Items were not loaded, so leave them out instead of sending 0 and []
    // (which would look like an empty session to the app).
    obj.remove(Field::kLostCount);
    obj.remove(Field::kQuestionCount);
    obj.remove(Field::kLostPings);
    obj.remove(Field::kQuestions);
    return obj;
}

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


// ---- SessionSummary ----

QJsonObject toJson(const SessionSummary& s) {
    QJsonObject obj;
    obj[Field::kSessionId] = s.id;
    obj[Field::kName] = QString::fromStdString(s.name);
    obj[Field::kStartTime] = toMs(s.startTime);
    obj[Field::kStatus] = toText(s.status);
    obj[Field::kCourseId] = s.courseId;
    if (!s.isLive()) {
        obj[Field::kEndTime] = toMs(s.endTime);
    }
    obj[Field::kLostCount] = s.lostCount;
    obj[Field::kQuestionCount] = s.questionCount;
    obj[Field::kHasRecording] = s.hasRecording;
    return obj;
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

// ---- JSON -> objects (the app's side). Missing or wrong fields become
// zero values; nothing throws. ----

namespace {

std::chrono::system_clock::time_point timeField(const QJsonObject& obj, const QString& key) {
    return fromMs(obj[key].toInteger(0));
}

// The three author fields (V4) onto any item that has setAuthor/hideAuthor.
template <typename Item>
void readAuthor(const QJsonObject& obj, Item& item) {
    switch (authorStateFromText(obj[Field::kAuthorState].toString())) {
    case AuthorState::Known:
        item.setAuthor(obj[Field::kAuthorId].toInt(0),
                       obj[Field::kAuthorName].toString().toStdString());
        break;
    case AuthorState::Deleted:
        item.setAuthorDeleted();
        break;
    case AuthorState::Hidden:
        item.hideAuthor();
        break;
    }
}

std::vector<int> intsFromJson(const QJsonArray& json) {
    std::vector<int> ids;
    for (const QJsonValue& value : json) {
        ids.push_back(value.toInt(0));
    }
    return ids;
}

}  // namespace

User userFromJson(const QJsonObject& json) {
    return User(json[Field::kId].toInt(0), json[Field::kName].toString().toStdString(),
                json[Field::kEmail].toString().toStdString());
}

Participant participantFromJson(const QJsonObject& json) {
    Participant p;
    p.userId = json[Field::kUserId].toInt(0);
    p.name = json[Field::kName].toString().toStdString();
    return p;
}

Course courseFromJson(const QJsonObject& json) {
    Course course(json[Field::kId].toInt(0), json[Field::kName].toString().toStdString());
    course.setJoinCode(json[Field::kJoinCode].toString().toStdString());
    course.setAnonymous(json[Field::kAnonymous].toBool(false));
    course.setQuestionsShared(json[Field::kQuestionsShared].toBool(false));
    course.setEducatorId(json[Field::kEducatorId].toInt(0));
    course.setRole(roleFromText(json[Field::kRole].toString()));
    course.setLiveSessionId(json[Field::kLiveSessionId].toInt(0));
    course.setPanoptoFolderId(json[Field::kPanoptoFolderId].toString().toStdString());
    course.setStudentIds(intsFromJson(json[Field::kStudentIds].toArray()));
    course.setSessionIds(intsFromJson(json[Field::kSessionIds].toArray()));
    return course;
}

SessionSummary summaryFromJson(const QJsonObject& json) {
    SessionSummary s;
    s.id = json[Field::kSessionId].toInt(0);
    s.courseId = json[Field::kCourseId].toInt(0);
    s.name = json[Field::kName].toString().toStdString();
    s.startTime = timeField(json, Field::kStartTime);
    s.endTime = timeField(json, Field::kEndTime);
    s.status = statusFromText(json[Field::kStatus].toString());
    s.lostCount = json[Field::kLostCount].toInt(0);
    s.questionCount = json[Field::kQuestionCount].toInt(0);
    s.hasRecording = json[Field::kHasRecording].toBool(false);
    return s;
}

LostPing lostPingFromJson(const QJsonObject& json) {
    LostPing ping(timeField(json, Field::kTime));
    ping.setId(json[Field::kPingId].toInt(0));
    ping.setSessionId(json[Field::kSessionId].toInt(0));
    readAuthor(json, ping);
    return ping;
}

Answer answerFromJson(const QJsonObject& json) {
    Answer answer(json[Field::kAnswerId].toInt(0), json[Field::kQuestionId].toInt(0),
                  timeField(json, Field::kTime), json[Field::kText].toString().toStdString());
    if (json.contains(Field::kEditedTime)) {
        answer.setEditedTime(timeField(json, Field::kEditedTime));
    }
    answer.setByEducator(json[Field::kByEducator].toBool(false));
    readAuthor(json, answer);
    return answer;
}

SessionQuestion questionFromJson(const QJsonObject& json) {
    SessionQuestion question(timeField(json, Field::kTime),
                             json[Field::kText].toString().toStdString());
    question.setId(json[Field::kQuestionId].toInt(0));
    question.setSessionId(json[Field::kSessionId].toInt(0));
    if (json.contains(Field::kEditedTime)) {
        question.setEditedTime(timeField(json, Field::kEditedTime));
    }
    readAuthor(json, question);
    std::vector<Answer> answers;
    for (const QJsonValue& value : json[Field::kAnswers].toArray()) {
        answers.push_back(answerFromJson(value.toObject()));
    }
    question.setAnswers(std::move(answers));
    return question;
}

Recording recordingFromJson(const QJsonObject& json) {
    return Recording(json[Field::kUrl].toString().toStdString(),
                     json[Field::kOffsetSeconds].toInteger(0),
                     sourceFromText(json[Field::kSource].toString()));
}

Session sessionFromJson(const QJsonObject& json) {
    const int id = json[Field::kSessionId].toInt(json[Field::kId].toInt(0));
    const int hostId = json[Field::kHostId].toInt(0);
    const auto start = timeField(json, Field::kStartTime);
    const SessionStatus status = statusFromText(json[Field::kStatus].toString());
    Session session = status == SessionStatus::Live
                          ? Session(id, hostId, start)
                          : Session(id, hostId, start, timeField(json, Field::kEndTime));
    if (status == SessionStatus::Published) {
        session.publish(timeField(json, Field::kPublishedTime));
    }
    session.setCourseId(json[Field::kCourseId].toInt(0));
    session.setName(json[Field::kName].toString().toStdString());
    session.setJoinCode(json[Field::kJoinCode].toString().toStdString());
    if (json[Field::kRecording].isObject()) {
        session.setRecording(recordingFromJson(json[Field::kRecording].toObject()));
    }
    std::vector<LostPing> pings;
    for (const QJsonValue& value : json[Field::kLostPings].toArray()) {
        pings.push_back(lostPingFromJson(value.toObject()));
    }
    session.setLostPings(std::move(pings));
    std::vector<SessionQuestion> questions;
    for (const QJsonValue& value : json[Field::kQuestions].toArray()) {
        questions.push_back(questionFromJson(value.toObject()));
    }
    session.setQuestions(std::move(questions));
    return session;
}

std::vector<Course> coursesFromJson(const QJsonArray& json) {
    std::vector<Course> courses;
    for (const QJsonValue& value : json) {
        courses.push_back(courseFromJson(value.toObject()));
    }
    return courses;
}

std::vector<SessionSummary> summariesFromJson(const QJsonArray& json) {
    std::vector<SessionSummary> summaries;
    for (const QJsonValue& value : json) {
        summaries.push_back(summaryFromJson(value.toObject()));
    }
    return summaries;
}

std::vector<Participant> participantsFromJson(const QJsonArray& json) {
    std::vector<Participant> participants;
    for (const QJsonValue& value : json) {
        participants.push_back(participantFromJson(value.toObject()));
    }
    return participants;
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