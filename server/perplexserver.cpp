// SERVER SIDE: PerplexServer implementation.
//
// Uses the contract protocol: every message is one JSON object with a
// "type" field (headers/protocol.h). On connect the server sends
// {"type":"hello","protocol":N} (CONTRACT.md 6).

#include "perplexserver.h"

#include <algorithm>
#include <tuple>

#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QPair>

#include "database.h"
#include "joincodes.h"
#include "protocol.h"

namespace {

QString timeToIsoString(std::chrono::system_clock::time_point time) {
    const auto secs =
        std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch())
            .count();
    return QDateTime::fromSecsSinceEpoch(secs).toUTC().toString(Qt::ISODate);
}

std::chrono::system_clock::time_point isoStringToTime(const QString& text) {
    QDateTime parsed = QDateTime::fromString(text, Qt::ISODate);
    if (!parsed.isValid()) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::time_point(
        std::chrono::seconds(parsed.toSecsSinceEpoch()));
}

QJsonObject sessionToM2Json(const Session& session) {
    QJsonObject obj;
    obj["id"] = session.getId();
    obj["userId"] = session.getHostId();
    obj["startTime"] = timeToIsoString(session.getStartTime());
    if (session.isEnded()) {
        obj["endTime"] = timeToIsoString(session.getEndTime());
    } else {
        obj["endTime"] = QJsonValue::Null;
    }

    struct InputItem {
        std::chrono::system_clock::time_point time;
        int id;
        QString type;
        QString text;
    };

    std::vector<InputItem> items;
    for (const auto& ping : session.getLostPings()) {
        items.push_back({ping.getTime(), ping.getId(), QStringLiteral("Lost"), QString()});
    }
    for (const auto& q : session.getQuestions()) {
        items.push_back({q.getTime(), q.getId(), QStringLiteral("Question"),
                         QString::fromStdString(q.getText())});
    }
    std::sort(items.begin(), items.end(), [](const InputItem& a, const InputItem& b) {
        if (a.time != b.time) {
            return a.time < b.time;
        }
        return a.id < b.id;
    });

    QJsonArray inputs;
    for (const auto& item : items) {
        QJsonObject inObj;
        inObj["type"] = item.type;
        inObj["time"] = timeToIsoString(item.time);
        inObj["text"] = item.text;
        inputs.append(inObj);
    }
    obj["inputs"] = inputs;
    return obj;
}

// Marks the reply as failed with this error, the same shape the other
// handlers use: {"ok": false, "error": "<code>", "code": "<code>"}.
void setFailure(QJsonObject& reply, Error error) {
    const QString code = QString::fromStdString(getErrorCode(error));
    reply[Protocol::Field::kOk] = false;
    reply[Protocol::Reply::kError] = code;
    reply[Protocol::Field::kCode] = code;
}

// What one viewer may see of a changed ping: Visibility runs on a copy of
// the session holding only this ping, so the rules are the same ones
// openSession uses (no second copy of them here). Nothing if they may not
// open the session (a student while it is in review) or may not see it.
std::optional<LostPing> getPingFor(const LostPingChange& change, const Viewer& viewer) {
    if (!Permissions::canOpenSession(viewer, change.session)) {
        return std::nullopt;
    }
    Session only = change.session;
    only.setLostPings({change.ping});
    only.setQuestions({});
    Visibility::redactSession(only, change.course, viewer);
    if (only.getLostPings().empty()) {
        return std::nullopt;
    }
    return only.getLostPings().front();
}

// Same for a changed question (with its answers).
std::optional<SessionQuestion> getQuestionFor(const QuestionChange& change, const Viewer& viewer) {
    if (!Permissions::canOpenSession(viewer, change.session)) {
        return std::nullopt;
    }
    Session only = change.session;
    only.setLostPings({});
    only.setQuestions({change.question});
    Visibility::redactSession(only, change.course, viewer);
    if (only.getQuestions().empty()) {
        return std::nullopt;
    }
    return only.getQuestions().front();
}

// {"type": type, "sessionId": .., "lostPing": {..}} for lostAdded.
QJsonObject makePingMessage(const QString& type, const LostPingChange& change,
                            const LostPing& ping) {
    QJsonObject message = Protocol::makeMessage(type);
    message[Protocol::Field::kSessionId] = change.session.getId();
    message[Protocol::Field::kLostPing] = Protocol::toJson(ping);
    return message;
}

// {"type": type, "sessionId": .., "question": {.. with answers}, "answerId": ..}
// for question and answer changes. An answer change sends its whole
// question, so the app just replaces that question (session.h upsertQuestion).
QJsonObject makeQuestionMessage(const QString& type, const QuestionChange& change,
                                const SessionQuestion& question) {
    QJsonObject message = Protocol::makeMessage(type);
    message[Protocol::Field::kSessionId] = change.session.getId();
    message[Protocol::Field::kQuestionId] = change.question.getId();
    message[Protocol::Field::kQuestion] = Protocol::toJson(question);
    if (change.answerId != 0) {
        message[Protocol::Field::kAnswerId] = change.answerId;
    }
    return message;
}

// The fields each request must carry, with their JSON type ('s' string,
// 'i' whole number, 'b' bool). One table, checked before any handler runs,
// so a missing or wrongly typed field is always BadRequest (N3) instead of
// a misleading model error ("7" as a session id is not session 0).
// Optional fields (createCourse's toggles, attachRecording's source) are
// only checked when present.
bool hasRequiredFields(const QString& type, const QJsonObject& request) {
    using namespace Protocol;
    static const QHash<QString, QList<QPair<QString, char>>> required = {
        {Request::kRegister, {{Field::kName, 's'}, {Field::kPassword, 's'}, {Field::kEmail, 's'}}},
        {Request::kLogin, {{Field::kName, 's'}, {Field::kPassword, 's'}}},
        {Request::kCreateCourse, {{Field::kName, 's'}}},
        {Request::kJoinCourse, {{Field::kJoinCode, 's'}}},
        {Request::kGetCourse, {{Field::kCourseId, 'i'}}},
        {Request::kStartSession, {{Field::kCourseId, 'i'}, {Field::kName, 's'}}},
        {Request::kJoinSession, {{Field::kJoinCode, 's'}}},
        {Request::kOpenSession, {{Field::kSessionId, 'i'}}},
        {Request::kEndSession, {{Field::kSessionId, 'i'}}},
        {Request::kAddLost, {{Field::kSessionId, 'i'}}},
        {Request::kDeleteLost, {{Field::kPingId, 'i'}}},
        {Request::kAddQuestion, {{Field::kSessionId, 'i'}, {Field::kText, 's'}}},
        {Request::kEditQuestion, {{Field::kQuestionId, 'i'}, {Field::kText, 's'}}},
        {Request::kDeleteQuestion, {{Field::kQuestionId, 'i'}}},
        {Request::kAddAnswer, {{Field::kQuestionId, 'i'}, {Field::kText, 's'}}},
        {Request::kEditAnswer, {{Field::kAnswerId, 'i'}, {Field::kText, 's'}}},
        {Request::kDeleteAnswer, {{Field::kAnswerId, 'i'}}},
    };
    static const QList<QPair<QString, char>> optional = {
        {Field::kAnonymous, 'b'}, {Field::kQuestionsShared, 'b'}, {Field::kSource, 's'},
    };
    auto isType = [](const QJsonValue& value, char kind) {
        switch (kind) {
        case 's': return value.isString();
        case 'i': return value.isDouble() && value.toDouble() == double(value.toInteger());
        case 'b': return value.isBool();
        default: return false;
        }
    };
    for (const auto& [key, kind] : required.value(type)) {
        if (!request.contains(key) || !isType(request[key], kind)) {
            return false;
        }
    }
    for (const auto& [key, kind] : optional) {
        if (request.contains(key) && !isType(request[key], kind)) {
            return false;
        }
    }
    return true;
}

}  // namespace

PerplexServer::PerplexServer(int port, Database* db, QObject* parent,
                             const QHostAddress& host,
                             int passwordIterations) :
    QObject(parent),
    db_(db),
    port_(port),
    host_(host),
    passwordIterations_(passwordIterations) {
    server_ = new QWebSocketServer(QStringLiteral("PerplexPingServer"),
                                   QWebSocketServer::NonSecureMode, this);

    registerHandlers();

    connect(server_, &QWebSocketServer::newConnection, this,
            &PerplexServer::onNewConnection);
}

PerplexServer::~PerplexServer() {
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        it.key()->deleteLater();
    }
    clients_.clear();
}

bool PerplexServer::listen() {
    if (!server_->listen(host_, port_)) {
        lastError_ = server_->errorString();
        qDebug() << "Failed to listen:" << lastError_;
        return false;
    }
    qDebug() << "Server listening on port" << port_;
    return true;
}

int PerplexServer::port() const {
    return server_ ? server_->serverPort() : 0;
}

QHostAddress PerplexServer::host() const {
    return host_;
}

QString PerplexServer::lastError() const {
    return lastError_;
}

// ---- Handler registration ----

void PerplexServer::registerHandlers() {
    auto handleRegister = [this](QWebSocket*, ClientState& state,
                                 const QJsonObject& request, QJsonObject& reply) {
        const QString name = request[Protocol::Field::kName].toString();
        const QString password = request[Protocol::Field::kPassword].toString();
        // Email is required (U6): no made-up name@scu.edu when it's missing.
        const QString email = request[Protocol::Field::kEmail].toString();

        Result<User> res = state.model->registerUser(name.toStdString(), password.toStdString(), email.toStdString());
        if (!res.isOk()) {
            setFailure(reply, res.error);
            return;
        }

        // Logging in on a logged-in app switches user: the old user's open
        // session (and its pushes and participant line) must not carry over.
        setOpenSession(state, 0, 0);
        state.authenticated = true;
        state.userId = res.value.getId();
        reply[Protocol::Field::kOk] = true;
        reply[Protocol::Field::kData] = Protocol::toJson(res.value);
    };

    auto handleLogin = [this](QWebSocket*, ClientState& state,
                              const QJsonObject& request, QJsonObject& reply) {
        const QString name = request[Protocol::Field::kName].toString();
        const QString password = request[Protocol::Field::kPassword].toString();

        Result<User> res = state.model->logIn(name.toStdString(), password.toStdString());
        if (!res.isOk()) {
            setFailure(reply, res.error);
            return;
        }

        // Logging in on a logged-in app switches user: the old user's open
        // session (and its pushes and participant line) must not carry over.
        setOpenSession(state, 0, 0);
        state.authenticated = true;
        state.userId = res.value.getId();
        reply[Protocol::Field::kOk] = true;
        reply[Protocol::Field::kData] = Protocol::toJson(res.value);
    };

    handlers_.insert(Protocol::Request::kRegister, handleRegister);
    handlers_.insert(QStringLiteral("registerUser"), handleRegister);

    handlers_.insert(Protocol::Request::kLogin, handleLogin);
    handlers_.insert(QStringLiteral("logIn"), handleLogin);

    handlers_.insert(Protocol::Request::kLogout,
        [this](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            state.model->logOut();
            state.authenticated = false;
            state.userId = -1;
            setOpenSession(state, 0, 0);
            reply[Protocol::Field::kOk] = true;
        });

    // ---- M2 client (ServerClient) compatibility handlers ----

    handlers_.insert(QStringLiteral("createSession"),
        [this](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                setFailure(reply, Error::NotLoggedIn);
                return;
            }

            int courseId = 0;
            Result<std::vector<Course>> courses = db_->listCoursesForUser(state.userId);
            if (courses.isOk()) {
                for (const auto& c : courses.value) {
                    if (c.getEducatorId() == state.userId) {
                        courseId = c.getId();
                        break;
                    }
                }
            }
            if (courseId == 0) {
                Result<Course> createdCourse = db_->createCourse(
                    "Default Course", JoinCodes::generateCourseCode(),
                    false, true, state.userId, std::chrono::system_clock::now());
                if (!createdCourse.isOk()) {
                    setFailure(reply, createdCourse.error);
                    return;
                }
                courseId = createdCourse.value.getId();
            }

            Result<Session> created = db_->createSession(
                courseId, state.userId, "Session",
                JoinCodes::generateSessionCode(), std::chrono::system_clock::now());
            if (!created.isOk()) {
                setFailure(reply, created.error);
                return;
            }

            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = sessionToM2Json(created.value);
        });

    handlers_.insert(QStringLiteral("saveInput"),
        [this](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                setFailure(reply, Error::NotLoggedIn);
                return;
            }

            const int sessionId = request["sessionId"].toInt(0);
            const QJsonObject inputObj = request["input"].toObject();
            const QString type = inputObj["type"].toString();
            const auto time = isoStringToTime(inputObj["time"].toString());

            QJsonObject data;
            if (type == QStringLiteral("Lost")) {
                Result<LostPing> res = db_->addLostPing(sessionId, state.userId, time);
                if (!res.isOk()) {
                    setFailure(reply, res.error);
                    return;
                }
                data["inputId"] = res.value.getId();
            } else {
                const std::string text = inputObj["text"].toString().toStdString();
                Result<SessionQuestion> res = db_->addQuestion(sessionId, state.userId, time, text);
                if (!res.isOk()) {
                    setFailure(reply, res.error);
                    return;
                }
                data["inputId"] = res.value.getId();
            }

            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = data;
        });

    handlers_.insert(QStringLiteral("saveSessionEnd"),
        [this](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                setFailure(reply, Error::NotLoggedIn);
                return;
            }

            const int sessionId = request["sessionId"].toInt(0);
            const auto endTime = isoStringToTime(request["endTime"].toString());
            Result<bool> res = db_->setSessionEnd(sessionId, endTime);
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
        });

    handlers_.insert(QStringLiteral("loadSession"),
        [this](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                setFailure(reply, Error::NotLoggedIn);
                return;
            }

            const int sessionId = request["sessionId"].toInt(0);
            Result<Session> res = db_->loadSession(sessionId);
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = sessionToM2Json(res.value);
        });

    handlers_.insert(QStringLiteral("listSessionsForUser"),
        [this](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                setFailure(reply, Error::NotLoggedIn);
                return;
            }

            std::vector<Session> sessions;
            Result<std::vector<Course>> courses = db_->listCoursesForUser(state.userId);
            if (courses.isOk()) {
                for (const auto& c : courses.value) {
                    if (c.getEducatorId() != state.userId) {
                        continue;
                    }
                    Result<std::vector<SessionSummary>> summaries = db_->listSessionsForCourse(c.getId());
                    if (!summaries.isOk()) {
                        continue;
                    }
                    for (const auto& summary : summaries.value) {
                        Result<Session> loaded = db_->loadSession(summary.id);
                        if (loaded.isOk()) {
                            sessions.push_back(std::move(loaded.value));
                        }
                    }
                }
            }

            std::sort(sessions.begin(), sessions.end(), [](const Session& a, const Session& b) {
                return a.getId() > b.getId();
            });

            QJsonArray arr;
            for (const auto& s : sessions) {
                arr.append(sessionToM2Json(s));
            }
            QJsonObject data;
            data["sessions"] = arr;
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = data;
        });

    // ---- Contract v2 handlers ----

    handlers_.insert(Protocol::Request::kCreateCourse,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const QString name = request[Protocol::Field::kName].toString();
            const bool anonymous = request[Protocol::Field::kAnonymous].toBool(false);
            const bool shared = request[Protocol::Field::kQuestionsShared].toBool(true);

            Result<Course> res = state.model->createCourse(name.toStdString(), anonymous, shared);
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kJoinCourse,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const QString code = request[Protocol::Field::kJoinCode].toString();
            Result<Course> res = state.model->joinCourse(code.toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            pushCourseChanged(client, res.value.getId());  // educator sees the new student
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kListCourses,
        [](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            Result<std::vector<Course>> res = state.model->getCourses();
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            QJsonObject data;
            data[Protocol::Field::kCourses] = Protocol::toJson(res.value);
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = data;
        });

    handlers_.insert(Protocol::Request::kGetCourse,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int courseId = request[Protocol::Field::kCourseId].toInt();
            Result<CourseDetail> res = state.model->getCourse(courseId);
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            // The course page needs its session list too (newest first,
            // already redacted per user by AppModel: S12).
            QJsonObject data = Protocol::toJson(res.value.course);
            data[Protocol::Field::kSessions] = Protocol::toJson(res.value.sessions);
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = data;
        });

    handlers_.insert(Protocol::Request::kStartSession,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int courseId = request[Protocol::Field::kCourseId].toInt();
            const QString name = request[Protocol::Field::kName].toString();

            Result<OpenedSession> res = state.model->startSession(courseId, name.toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            setOpenSession(state, res.value.session.getId(), res.value.session.getCourseId());
            pushCourseChanged(client, courseId);  // the course page shows it live
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.session);
        });

    handlers_.insert(Protocol::Request::kJoinSession,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const QString code = request[Protocol::Field::kJoinCode].toString();
            Result<OpenedSession> res = state.model->joinSession(code.toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            setOpenSession(state, res.value.session.getId(), res.value.session.getCourseId());
            if (res.value.newlyEnrolled) {
                pushCourseChanged(client, res.value.course.getId());
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.session);
        });

    handlers_.insert(Protocol::Request::kOpenSession,
        [this](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int sessionId = request[Protocol::Field::kSessionId].toInt();
            Result<OpenedSession> res = state.model->openSession(sessionId);
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            setOpenSession(state, res.value.session.getId(), res.value.session.getCourseId());
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.session);
        });

    handlers_.insert(Protocol::Request::kEndSession,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int sessionId = request[Protocol::Field::kSessionId].toInt();
            Result<Session> res = state.model->endSession(sessionId);
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            // Everyone in the room learns it locked, students too (they can
            // no longer open it once it is in review, S12).
            pushToSession(client, sessionId, res.value.getCourseId(),
                [ended = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    Session details = ended;  // details only, no items
                    if (viewer.role != CourseRole::Educator) {
                        details.setJoinCode("");  // S4
                    }
                    QJsonObject message = Protocol::makeMessage(Protocol::Reply::kSessionEnded);
                    message[Protocol::Field::kSessionId] = ended.getId();
                    message[Protocol::Field::kSession] = Protocol::toDetailsJson(details);
                    return message;
                });
            pushCourseChanged(client, res.value.getCourseId());
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toDetailsJson(res.value);
        });

    handlers_.insert(Protocol::Request::kAddLost,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            Result<LostPingChange> res = state.model->addLost(request[Protocol::Field::kSessionId].toInt());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            const int courseId = res.value.session.getCourseId();
            reply[Protocol::Field::kOk] = true;
            if (const auto mine = getPingFor(res.value, getViewer(state, courseId))) {
                reply[Protocol::Field::kData] = Protocol::toJson(*mine);
            }
            pushToSession(client, res.value.session.getId(), courseId,
                [change = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    const auto ping = getPingFor(change, viewer);
                    if (!ping) {
                        return std::nullopt;
                    }
                    return makePingMessage(Protocol::Reply::kLostAdded, change, *ping);
                });
        });

    handlers_.insert(Protocol::Request::kDeleteLost,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            Result<LostPingChange> res = state.model->deleteLost(request[Protocol::Field::kPingId].toInt());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            const int courseId = res.value.session.getCourseId();
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kPingId] = res.value.ping.getId();
            pushToSession(client, res.value.session.getId(), courseId,
                [change = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    // Only to those who could see the ping before it went.
                    if (!getPingFor(change, viewer)) {
                        return std::nullopt;
                    }
                    QJsonObject message = Protocol::makeMessage(Protocol::Reply::kLostDeleted);
                    message[Protocol::Field::kSessionId] = change.session.getId();
                    message[Protocol::Field::kPingId] = change.ping.getId();
                    return message;
                });
        });

    handlers_.insert(Protocol::Request::kAddQuestion,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            Result<QuestionChange> res = state.model->addQuestion(
                request[Protocol::Field::kSessionId].toInt(),
                request[Protocol::Field::kText].toString().toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            const int courseId = res.value.session.getCourseId();
            reply[Protocol::Field::kOk] = true;
            if (const auto mine = getQuestionFor(res.value, getViewer(state, courseId))) {
                reply[Protocol::Field::kData] = Protocol::toJson(*mine);
            }
            pushToSession(client, res.value.session.getId(), courseId,
                [change = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    const auto question = getQuestionFor(change, viewer);
                    if (!question) {
                        return std::nullopt;
                    }
                    return makeQuestionMessage(Protocol::Reply::kQuestionAdded, change, *question);
                });
        });

    handlers_.insert(Protocol::Request::kEditQuestion,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            Result<QuestionChange> res = state.model->editQuestion(
                request[Protocol::Field::kQuestionId].toInt(),
                request[Protocol::Field::kText].toString().toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            const int courseId = res.value.session.getCourseId();
            reply[Protocol::Field::kOk] = true;
            if (const auto mine = getQuestionFor(res.value, getViewer(state, courseId))) {
                reply[Protocol::Field::kData] = Protocol::toJson(*mine);
            }
            pushToSession(client, res.value.session.getId(), courseId,
                [change = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    const auto question = getQuestionFor(change, viewer);
                    if (!question) {
                        return std::nullopt;
                    }
                    return makeQuestionMessage(Protocol::Reply::kQuestionUpdated, change, *question);
                });
        });

    handlers_.insert(Protocol::Request::kDeleteQuestion,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            Result<QuestionChange> res = state.model->deleteQuestion(
                request[Protocol::Field::kQuestionId].toInt());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            const int courseId = res.value.session.getCourseId();
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kQuestionId] = res.value.question.getId();
            pushToSession(client, res.value.session.getId(), courseId,
                [change = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    // Only to those who could see the question before it went.
                    if (!getQuestionFor(change, viewer)) {
                        return std::nullopt;
                    }
                    QJsonObject message = Protocol::makeMessage(Protocol::Reply::kQuestionDeleted);
                    message[Protocol::Field::kSessionId] = change.session.getId();
                    message[Protocol::Field::kQuestionId] = change.question.getId();
                    message[Protocol::Field::kByModerator] = change.byModerator;
                    return message;
                });
        });

    handlers_.insert(Protocol::Request::kAddAnswer,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            Result<QuestionChange> res = state.model->addAnswer(
                request[Protocol::Field::kQuestionId].toInt(),
                request[Protocol::Field::kText].toString().toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            const int courseId = res.value.session.getCourseId();
            reply[Protocol::Field::kOk] = true;
            if (const auto mine = getQuestionFor(res.value, getViewer(state, courseId))) {
                reply[Protocol::Field::kData] = Protocol::toJson(*mine);
            }
            pushToSession(client, res.value.session.getId(), courseId,
                [change = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    const auto question = getQuestionFor(change, viewer);
                    if (!question) {
                        return std::nullopt;
                    }
                    return makeQuestionMessage(Protocol::Reply::kAnswerAdded, change, *question);
                });
        });

    // ---- Accounts: change password, change email, delete (U6-U8) ----

    handlers_.insert(Protocol::Request::kChangePassword,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kOldPassword, 's'},
                                               {Protocol::Field::kNewPassword, 's'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            const Error err = state.model->changePassword(
                request[Protocol::Field::kOldPassword].toString().toStdString(),
                request[Protocol::Field::kNewPassword].toString().toStdString());
            if (err != Error::None) {
                setFailure(reply, err);
                return;
            }
            reply[Protocol::Field::kOk] = true;
        });

    handlers_.insert(Protocol::Request::kUpdateEmail,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kEmail, 's'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<User> res = state.model->updateEmail(
                request[Protocol::Field::kEmail].toString().toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kDeleteAccount,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kPassword, 's'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            const int userId = state.userId;
            const Error err = state.model->deleteAccount(
                request[Protocol::Field::kPassword].toString().toStdString());
            if (err != Error::None) {
                setFailure(reply, err);
                return;
            }
            // AppModel already logged this connection out. The same user may
            // be logged in on other devices: log those out too (U8).
            state.authenticated = false;
            state.userId = -1;
            setOpenSession(state, 0, 0);
            logOutOtherConnections(client, userId, Protocol::kReasonAccountDeleted);
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kReason] = Protocol::kReasonAccountDeleted;
        });

    // ---- Courses: settings and delete (C6, C7, C9) ----

    handlers_.insert(Protocol::Request::kUpdateCourse,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            // All three are sent every time (appmodel.h), so none can be left out.
            if (!Protocol::hasFields(request, {{Protocol::Field::kCourseId, 'i'},
                                               {Protocol::Field::kName, 's'},
                                               {Protocol::Field::kAnonymous, 'b'},
                                               {Protocol::Field::kQuestionsShared, 'b'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<Course> res = state.model->updateCourse(
                request[Protocol::Field::kCourseId].toInt(),
                request[Protocol::Field::kName].toString().toStdString(),
                request[Protocol::Field::kAnonymous].toBool(),
                request[Protocol::Field::kQuestionsShared].toBool());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            pushCourseChanged(client, res.value.getId());
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kDeleteCourse,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kCourseId, 'i'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            const int courseId = request[Protocol::Field::kCourseId].toInt();
            // Who to tell, worked out before the course (and their role in it) is gone.
            const QList<QWebSocket*> members = getCourseConnections(client, courseId);
            const Error err = state.model->deleteCourse(courseId);
            if (err != Error::None) {
                setFailure(reply, err);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kCourseId] = courseId;
            QJsonObject message = Protocol::makeMessage(Protocol::Reply::kCourseDeleted);
            message[Protocol::Field::kCourseId] = courseId;
            const QString text = Protocol::encode(message);
            for (QWebSocket* socket : members) {
                socket->sendTextMessage(text);
            }
        });

    // ---- Sessions: publish, rename, delete (S8, S9, S11) ----

    handlers_.insert(Protocol::Request::kPublishSession,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kSessionId, 'i'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<Session> res = state.model->publishSession(
                request[Protocol::Field::kSessionId].toInt());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            pushCourseChanged(client, res.value.getCourseId());  // students can open it now
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toDetailsJson(res.value);
        });

    handlers_.insert(Protocol::Request::kRenameSession,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kSessionId, 'i'},
                                               {Protocol::Field::kName, 's'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<Session> res = state.model->renameSession(
                request[Protocol::Field::kSessionId].toInt(),
                request[Protocol::Field::kName].toString().toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            {
                QJsonObject message = Protocol::makeMessage(Protocol::Reply::kSessionRenamed);
                message[Protocol::Field::kSessionId] = res.value.getId();
                message[Protocol::Field::kCourseId] = res.value.getCourseId();
                message[Protocol::Field::kName] = QString::fromStdString(res.value.getName());
                const QString text = Protocol::encode(message);
                for (QWebSocket* socket : getCourseConnections(client, res.value.getCourseId())) {
                    socket->sendTextMessage(text);
                }
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toDetailsJson(res.value);
        });

    handlers_.insert(Protocol::Request::kDeleteSession,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kSessionId, 'i'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<Session> res = state.model->deleteSession(
                request[Protocol::Field::kSessionId].toInt());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            {
                QJsonObject message = Protocol::makeMessage(Protocol::Reply::kSessionDeleted);
                message[Protocol::Field::kSessionId] = res.value.getId();
                message[Protocol::Field::kCourseId] = res.value.getCourseId();
                const QString text = Protocol::encode(message);
                for (QWebSocket* socket : getCourseConnections(client, res.value.getCourseId())) {
                    socket->sendTextMessage(text);
                }
                for (ClientState& other : clients_) {
                    if (other.openSessionId == res.value.getId()) {
                        other.openSessionId = 0;  // it is gone
                    }
                }
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kSessionId] = res.value.getId();
            reply[Protocol::Field::kCourseId] = res.value.getCourseId();
        });

    // ---- Answers: edit and delete (A3, A7, A8) ----

    handlers_.insert(Protocol::Request::kEditAnswer,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            Result<QuestionChange> res = state.model->editAnswer(
                request[Protocol::Field::kAnswerId].toInt(),
                request[Protocol::Field::kText].toString().toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            const int courseId = res.value.session.getCourseId();
            reply[Protocol::Field::kOk] = true;
            if (const auto mine = getQuestionFor(res.value, getViewer(state, courseId))) {
                reply[Protocol::Field::kData] = Protocol::toJson(*mine);
            }
            pushToSession(client, res.value.session.getId(), courseId,
                [change = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    const auto question = getQuestionFor(change, viewer);
                    if (!question) {
                        return std::nullopt;
                    }
                    return makeQuestionMessage(Protocol::Reply::kAnswerUpdated, change, *question);
                });
        });

    handlers_.insert(Protocol::Request::kDeleteAnswer,
        [this](QWebSocket* client, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            Result<QuestionChange> res = state.model->deleteAnswer(
                request[Protocol::Field::kAnswerId].toInt());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            const int courseId = res.value.session.getCourseId();
            reply[Protocol::Field::kOk] = true;
            if (const auto mine = getQuestionFor(res.value, getViewer(state, courseId))) {
                reply[Protocol::Field::kData] = Protocol::toJson(*mine);
            }
            pushToSession(client, res.value.session.getId(), courseId,
                [change = res.value](const Viewer& viewer) -> std::optional<QJsonObject> {
                    const auto question = getQuestionFor(change, viewer);
                    if (!question) {
                        return std::nullopt;
                    }
                    return makeQuestionMessage(Protocol::Reply::kAnswerDeleted, change, *question);
                });
        });

    // ---- Recording: attach, remove, auto-match (R1-R3, R6) ----

    handlers_.insert(Protocol::Request::kAttachRecording,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kSessionId, 'i'},
                                               {Protocol::Field::kUrl, 's'},
                                               {Protocol::Field::kOffsetSeconds, 'i'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            // "source" is optional: a pasted link is "manual".
            const RecordingSource source = Protocol::sourceFromText(
                request[Protocol::Field::kSource].toString());
            Result<Session> res = state.model->attachRecording(
                request[Protocol::Field::kSessionId].toInt(),
                request[Protocol::Field::kUrl].toString().toStdString(),
                request[Protocol::Field::kOffsetSeconds].toInteger(),
                source);
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toDetailsJson(res.value);
        });

    handlers_.insert(Protocol::Request::kRemoveRecording,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kSessionId, 'i'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<Session> res = state.model->removeRecording(
                request[Protocol::Field::kSessionId].toInt());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toDetailsJson(res.value);
        });

    // The app asks Panopto for the course folder's recordings and sends them
    // here; AppModel picks the one that overlaps the session.
    handlers_.insert(Protocol::Request::kCheckRecording,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kSessionId, 'i'}}) ||
                !request[Protocol::Field::kCandidates].isArray()) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            const QJsonArray candidates = request[Protocol::Field::kCandidates].toArray();
            if (candidates.size() > Protocol::kMaxCandidates) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<Session> res = state.model->matchRecording(
                request[Protocol::Field::kSessionId].toInt(),
                Protocol::candidatesFromJson(candidates));
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toDetailsJson(res.value);
        });

    // The app left the session page. Nothing changes in the database.
    handlers_.insert(Protocol::Request::kCloseSession,
        [this](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            setOpenSession(state, 0, 0);
            reply[Protocol::Field::kOk] = true;
        });
}

void PerplexServer::onNewConnection() {
    QWebSocket* client = server_->nextPendingConnection();
    // Qt drops (and closes on) anything bigger than this before we parse it.
    client->setMaxAllowedIncomingMessageSize(Protocol::kMaxMessageBytes);
    connect(client, &QWebSocket::textMessageReceived, this,
            &PerplexServer::onTextMessageReceived);
    connect(client, &QWebSocket::disconnected, this,
            &PerplexServer::onClientDisconnected);

    ClientState state;
    state.model = std::make_shared<AppModel>(
        db_, [] { return AppModel::Clock::now(); }, passwordIterations_);
    clients_[client] = std::move(state);

    // Send hello on connect (CONTRACT.md 6)
    QJsonObject hello = Protocol::makeMessage(Protocol::Reply::kHello);
    hello[Protocol::Field::kProtocol] = Protocol::kVersion;
    client->sendTextMessage(QJsonDocument(hello).toJson());
}

void PerplexServer::onTextMessageReceived(const QString& message) {
    QWebSocket* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }

    const QByteArray bytes = message.toUtf8();
    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (bytes.size() > Protocol::kMaxMessageBytes || !doc.isObject()) {
        // Not one JSON object (or too big): answer, so the app isn't left
        // waiting, but run nothing (N3).
        QJsonObject resp = Protocol::makeMessage(Protocol::Reply::kError);
        setFailure(resp, Error::BadRequest);
        client->sendTextMessage(Protocol::encode(resp));
        return;
    }

    const QJsonObject obj = doc.object();
    QString type = obj[Protocol::Field::kType].toString();
    if (type.isEmpty() && obj.contains(QStringLiteral("op"))) {
        type = obj[QStringLiteral("op")].toString();
    }

    QJsonObject params;
    if (obj.contains(Protocol::Field::kRequest) && obj[Protocol::Field::kRequest].isObject()) {
        params = obj[Protocol::Field::kRequest].toObject();
    } else if (obj.contains(QStringLiteral("params")) && obj[QStringLiteral("params")].isObject()) {
        params = obj[QStringLiteral("params")].toObject();
    } else {
        params = obj;
    }

    auto handlerIt = handlers_.find(type);
    if (handlerIt == handlers_.end()) {
        QJsonObject resp;
        if (obj.contains(QStringLiteral("id"))) {
            resp[QStringLiteral("id")] = obj[QStringLiteral("id")];
        }
        resp[Protocol::Field::kType] = Protocol::Reply::kError;
        resp[Protocol::Field::kRequest] = type;
        resp[Protocol::Field::kCode] = QStringLiteral("UnknownRequest");
        resp[QStringLiteral("error")] = QStringLiteral("UnknownRequest");
        resp[QStringLiteral("ok")] = false;
        client->sendTextMessage(QJsonDocument(resp).toJson(QJsonDocument::Compact));
        return;
    }

    ClientState& state = clients_[client];
    QJsonObject response;
    if (obj.contains(QStringLiteral("id"))) {
        response[QStringLiteral("id")] = obj[QStringLiteral("id")];
    }
    response[Protocol::Field::kType] = type;
    response[Protocol::Field::kOk] = false;

    if (!hasRequiredFields(type, params)) {
        setFailure(response, Error::BadRequest);
    } else {
        handlerIt.value()(client, state, params, response);
    }

    client->sendTextMessage(QJsonDocument(response).toJson(QJsonDocument::Compact));
}

void PerplexServer::onClientDisconnected() {
    QWebSocket* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }
    setOpenSession(clients_[client], 0, 0);  // tell the educator they left
    clients_.remove(client);
    client->deleteLater();
}

void PerplexServer::logOutOtherConnections(QWebSocket* except, int userId,
                                           const QString& reason) {
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        if (it.key() == except || it.value().userId != userId) {
            continue;
        }
        it.value().model->logOut();
        it.value().authenticated = false;
        it.value().userId = -1;
        setOpenSession(it.value(), 0, 0);
        QJsonObject message = Protocol::makeMessage(Protocol::Reply::kLoggedOut);
        message[Protocol::Field::kReason] = reason;
        it.key()->sendTextMessage(Protocol::encode(message));
    }
}

Viewer PerplexServer::getViewer(const ClientState& state, int courseId) const {
    const User* user = state.model->getCurrentUser();
    if (!user) {
        return Viewer{};
    }
    return Viewer{user->getId(), state.model->getRole(courseId)};
}

void PerplexServer::pushToSession(QWebSocket* except, int sessionId, int courseId,
                                  const PushBuilder& build) {
    if (sessionId == 0) {
        return;
    }
    for (auto it = clients_.cbegin(); it != clients_.cend(); ++it) {
        if (it.key() == except || it.value().openSessionId != sessionId ||
            !it.value().model->isLoggedIn()) {
            continue;
        }
        const Viewer viewer = getViewer(it.value(), courseId);
        if (viewer.role == CourseRole::None) {
            continue;
        }
        const std::optional<QJsonObject> message = build(viewer);
        if (message) {
            it.key()->sendTextMessage(Protocol::encode(*message));
        }
    }
}

QList<QWebSocket*> PerplexServer::getCourseConnections(QWebSocket* except, int courseId) const {
    QList<QWebSocket*> sockets;
    for (auto it = clients_.cbegin(); it != clients_.cend(); ++it) {
        if (it.key() != except && it.value().model->isLoggedIn() &&
            it.value().model->getRole(courseId) != CourseRole::None) {
            sockets.append(it.key());
        }
    }
    return sockets;
}

void PerplexServer::pushCourseChanged(QWebSocket* except, int courseId) {
    QJsonObject message = Protocol::makeMessage(Protocol::Reply::kCourseChanged);
    message[Protocol::Field::kCourseId] = courseId;
    const QString text = Protocol::encode(message);
    for (QWebSocket* socket : getCourseConnections(except, courseId)) {
        socket->sendTextMessage(text);
    }
}

void PerplexServer::setOpenSession(ClientState& state, int sessionId, int courseId) {
    if (state.openSessionId == sessionId) {
        return;
    }
    if (state.openSessionId != 0) {
        pushParticipantsLater(state.openSessionId, state.openCourseId);
    }
    state.openSessionId = sessionId;
    state.openCourseId = courseId;
    if (sessionId != 0) {
        pushParticipantsLater(sessionId, courseId);
    }
}

void PerplexServer::pushParticipantsLater(int sessionId, int courseId) {
    QMetaObject::invokeMethod(this, [this, sessionId, courseId] {
        std::vector<Participant> participants;
        QList<QWebSocket*> educators;
        bool anonymous = false;
        for (auto it = clients_.cbegin(); it != clients_.cend(); ++it) {
            const ClientState& state = it.value();
            if (state.openSessionId != sessionId) {
                continue;
            }
            const Viewer viewer = getViewer(state, courseId);
            if (viewer.role == CourseRole::Educator) {
                educators.append(it.key());
                Result<CourseDetail> course = state.model->getCourse(courseId);
                anonymous = course.isOk() && course.value.course.isAnonymous();
            } else if (viewer.role == CourseRole::Student) {
                // One line per student, even if they have the session open twice.
                const bool listed = std::any_of(participants.begin(), participants.end(),
                    [&](const Participant& p) { return p.userId == viewer.userId; });
                if (!listed) {
                    participants.push_back({viewer.userId, state.model->getCurrentUser()->getName()});
                }
            }
        }
        if (educators.isEmpty()) {
            return;
        }
        if (anonymous) {
            for (Participant& p : participants) {
                p = Participant{};  // still counted, never named
            }
        }
        QJsonObject message = Protocol::makeMessage(Protocol::Reply::kParticipants);
        message[Protocol::Field::kSessionId] = sessionId;
        message[Protocol::Field::kParticipants] = Protocol::toJson(participants);
        const QString text = Protocol::encode(message);
        for (QWebSocket* socket : educators) {
            socket->sendTextMessage(text);
        }
    }, Qt::QueuedConnection);
}
