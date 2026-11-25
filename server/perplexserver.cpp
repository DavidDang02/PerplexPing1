// SERVER SIDE: PerplexServer implementation.
//
// Uses the contract protocol: every message is one JSON object with a
// "type" field (headers/protocol.h). On connect the server sends
// {"type":"hello","protocol":N} (CONTRACT.md 6).

#include "perplexserver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include "database.h"
#include "protocol.h"

PerplexServer::PerplexServer(int port, Database* db, QObject* parent,
                             const QHostAddress& host) :
    QObject(parent), db_(db), port_(port), host_(host) {
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
                                  : QStringLiteral("user@scu.edu");

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
}

void PerplexServer::onNewConnection() {
    QWebSocket* client = server_->nextPendingConnection();
    connect(client, &QWebSocket::textMessageReceived, this,
            &PerplexServer::onTextMessageReceived);
    connect(client, &QWebSocket::disconnected, this,
            &PerplexServer::onClientDisconnected);

    ClientState state;
    state.model = std::make_shared<AppModel>(db_);
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