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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
        const QString email = request.contains(Protocol::Field::kEmail)
                                  ? request[Protocol::Field::kEmail].toString()
                                  : QStringLiteral("%1@scu.edu").arg(name.trimmed());

        Result<User> res = state.model->registerUser(name.toStdString(), password.toStdString(), email.toStdString());
        if (!res.isOk()) {
            reply[Protocol::Field::kOk] = false;
            reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
            reply[Protocol::Field::kCode] = QString::fromStdString(getErrorCode(res.error));
            return;
        }

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
            reply[Protocol::Field::kOk] = false;
            reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
            reply[Protocol::Field::kCode] = QString::fromStdString(getErrorCode(res.error));
            return;
        }

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
        [](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            state.model->logOut();
            state.authenticated = false;
            state.userId = -1;
            reply[Protocol::Field::kOk] = true;
        });

    // ---- M2 client (ServerClient) compatibility handlers ----

    handlers_.insert(QStringLiteral("createSession"),
        [this](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(Error::NotLoggedIn));
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
                    reply[Protocol::Field::kOk] = false;
                    reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(createdCourse.error));
                    return;
                }
                courseId = createdCourse.value.getId();
            }

            Result<Session> created = db_->createSession(
                courseId, state.userId, "Session",
                JoinCodes::generateSessionCode(), std::chrono::system_clock::now());
            if (!created.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(created.error));
                return;
            }

            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = sessionToM2Json(created.value);
        });

    handlers_.insert(QStringLiteral("saveInput"),
        [this](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(Error::NotLoggedIn));
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
                    reply[Protocol::Field::kOk] = false;
                    reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                    return;
                }
                data["inputId"] = res.value.getId();
            } else {
                const std::string text = inputObj["text"].toString().toStdString();
                Result<SessionQuestion> res = db_->addQuestion(sessionId, state.userId, time, text);
                if (!res.isOk()) {
                    reply[Protocol::Field::kOk] = false;
                    reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
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
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(Error::NotLoggedIn));
                return;
            }

            const int sessionId = request["sessionId"].toInt(0);
            const auto endTime = isoStringToTime(request["endTime"].toString());
            Result<bool> res = db_->setSessionEnd(sessionId, endTime);
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
        });

    handlers_.insert(QStringLiteral("loadSession"),
        [this](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(Error::NotLoggedIn));
                return;
            }

            const int sessionId = request["sessionId"].toInt(0);
            Result<Session> res = db_->loadSession(sessionId);
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = sessionToM2Json(res.value);
        });

    handlers_.insert(QStringLiteral("listSessionsForUser"),
        [this](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            if (!state.authenticated || state.userId <= 0) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(Error::NotLoggedIn));
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
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kJoinCourse,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const QString code = request[Protocol::Field::kJoinCode].toString();
            Result<Course> res = state.model->joinCourse(code.toStdString());
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kListCourses,
        [](QWebSocket*, ClientState& state, const QJsonObject&, QJsonObject& reply) {
            Result<std::vector<Course>> res = state.model->getCourses();
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
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
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.course);
        });

    handlers_.insert(Protocol::Request::kStartSession,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int courseId = request[Protocol::Field::kCourseId].toInt();
            const QString name = request[Protocol::Field::kName].toString();

            Result<OpenedSession> res = state.model->startSession(courseId, name.toStdString());
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.session);
        });

    handlers_.insert(Protocol::Request::kJoinSession,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const QString code = request[Protocol::Field::kJoinCode].toString();
            Result<OpenedSession> res = state.model->joinSession(code.toStdString());
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.session);
        });

    handlers_.insert(Protocol::Request::kOpenSession,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int sessionId = request[Protocol::Field::kSessionId].toInt();
            Result<OpenedSession> res = state.model->openSession(sessionId);
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.session);
        });

    handlers_.insert(Protocol::Request::kEndSession,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int sessionId = request[Protocol::Field::kSessionId].toInt();
            Result<Session> res = state.model->endSession(sessionId);
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kAddLost,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int sessionId = request[Protocol::Field::kSessionId].toInt();
            Result<LostPingChange> res = state.model->addLost(sessionId);
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.ping);
        });

    handlers_.insert(Protocol::Request::kDeleteLost,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int pingId = request[Protocol::Field::kPingId].toInt();
            Result<LostPingChange> res = state.model->deleteLost(pingId);
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
        });

    handlers_.insert(Protocol::Request::kAddQuestion,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int sessionId = request[Protocol::Field::kSessionId].toInt();
            const QString text = request[Protocol::Field::kText].toString();
            Result<QuestionChange> res = state.model->addQuestion(sessionId, text.toStdString());
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.question);
        });

    handlers_.insert(Protocol::Request::kEditQuestion,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int questionId = request[Protocol::Field::kQuestionId].toInt();
            const QString text = request[Protocol::Field::kText].toString();
            Result<QuestionChange> res = state.model->editQuestion(questionId, text.toStdString());
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.question);
        });

    handlers_.insert(Protocol::Request::kDeleteQuestion,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int questionId = request[Protocol::Field::kQuestionId].toInt();
            Result<QuestionChange> res = state.model->deleteQuestion(questionId);
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
        });

    handlers_.insert(Protocol::Request::kAddAnswer,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            const int questionId = request[Protocol::Field::kQuestionId].toInt();
            const QString text = request[Protocol::Field::kText].toString();
            Result<QuestionChange> res = state.model->addAnswer(questionId, text.toStdString());
            if (!res.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = QString::fromStdString(getErrorCode(res.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.question);
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
            logOutOtherConnections(client, userId, Protocol::kReasonAccountDeleted);
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kReason] = Protocol::kReasonAccountDeleted;
        });

    // ---- Courses: settings and delete (C6, C7, C9) ----

    handlers_.insert(Protocol::Request::kUpdateCourse,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
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
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kDeleteCourse,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kCourseId, 'i'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            const int courseId = request[Protocol::Field::kCourseId].toInt();
            const Error err = state.model->deleteCourse(courseId);
            if (err != Error::None) {
                setFailure(reply, err);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kCourseId] = courseId;
        });

    // ---- Sessions: publish, rename, delete (S8, S9, S11) ----

    handlers_.insert(Protocol::Request::kPublishSession,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
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
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kRenameSession,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
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
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });

    handlers_.insert(Protocol::Request::kDeleteSession,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
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
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kSessionId] = res.value.getId();
            reply[Protocol::Field::kCourseId] = res.value.getCourseId();
        });

    // ---- Answers: edit and delete (A3, A7, A8) ----

    handlers_.insert(Protocol::Request::kEditAnswer,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kAnswerId, 'i'},
                                               {Protocol::Field::kText, 's'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<QuestionChange> res = state.model->editAnswer(
                request[Protocol::Field::kAnswerId].toInt(),
                request[Protocol::Field::kText].toString().toStdString());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.question);
        });

    handlers_.insert(Protocol::Request::kDeleteAnswer,
        [](QWebSocket*, ClientState& state, const QJsonObject& request, QJsonObject& reply) {
            if (!Protocol::hasFields(request, {{Protocol::Field::kAnswerId, 'i'}})) {
                setFailure(reply, Error::BadRequest);
                return;
            }
            Result<QuestionChange> res = state.model->deleteAnswer(
                request[Protocol::Field::kAnswerId].toInt());
            if (!res.isOk()) {
                setFailure(reply, res.error);
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(res.value.question);
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
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
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
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
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
            reply[Protocol::Field::kData] = Protocol::toJson(res.value);
        });
}

void PerplexServer::onNewConnection() {
    QWebSocket* client = server_->nextPendingConnection();
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

    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
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

    handlerIt.value()(client, state, params, response);

    client->sendTextMessage(QJsonDocument(response).toJson(QJsonDocument::Compact));
}

void PerplexServer::onClientDisconnected() {
    QWebSocket* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }
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
        QJsonObject message = Protocol::makeMessage(Protocol::Reply::kLoggedOut);
        message[Protocol::Field::kReason] = reason;
        it.key()->sendTextMessage(Protocol::encode(message));
    }
}
