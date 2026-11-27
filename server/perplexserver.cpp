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
#include "session.h"
#include "user.h"

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
    // Register (app -> server {"type":"register","name":...,"password":...,"email":...})
    handlers_.insert(Protocol::Request::kRegister,
        [this](QWebSocket* sender, ClientState& state,
               const QJsonObject& request, QJsonObject& reply) {
            QString name = request[Protocol::Field::kName].toString();
            QString password = request[Protocol::Field::kPassword].toString();
            QString email = request[Protocol::Field::kEmail].toString();

            // Hash/password setup would go here (AppModel owns rules).
            // For now create user with a placeholder hash.
            Result<User> newUser = db_->createUser(
                name.toStdString(), email.toStdString(),
                "placeholder-hash", "placeholder-salt", 1,
                std::chrono::system_clock::now());
            if (!newUser.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] =
                    QString::fromStdString(getErrorCode(newUser.error));
                return;
            }
            state.authenticated = true;
            state.userId = newUser.value.getId();
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(newUser.value);
        });

    // Login
    handlers_.insert(Protocol::Request::kLogin,
        [this](QWebSocket* sender, ClientState& state,
               const QJsonObject& request, QJsonObject& reply) {
            QString name = request[Protocol::Field::kName].toString();
            QString password = request[Protocol::Field::kPassword].toString();

            Result<User> found = db_->findUserByName(name.toStdString());
            if (!found.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = "WrongLogin";
                return;
            }
            state.authenticated = true;
            state.userId = found.value.getId();
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(found.value);
        });

    // Create session
    handlers_.insert(Protocol::Request::kStartSession,
        [this](QWebSocket* sender, ClientState& state,
               const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = "NotLoggedIn";
                return;
            }
            QString name = request[Protocol::Field::kName].toString();
            QString code = request[Protocol::Field::kJoinCode].toString();

            Result<Session> session = db_->createSession(
                0, state.userId, name.toStdString(), code.toStdString(),
                std::chrono::system_clock::now());
            if (!session.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] =
                    QString::fromStdString(getErrorCode(session.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(session.value);
        });

    // Add lost ping
    handlers_.insert(Protocol::Request::kAddLost,
        [this](QWebSocket* sender, ClientState& state,
               const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = "NotLoggedIn";
                return;
            }
            int sessionId = request[Protocol::Field::kSessionId].toInt();
            Result<LostPing> ping = db_->addLostPing(
                sessionId, state.userId, std::chrono::system_clock::now());
            if (!ping.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] =
                    QString::fromStdString(getErrorCode(ping.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(ping.value);
        });

    // Add question
    handlers_.insert(Protocol::Request::kAddQuestion,
        [this](QWebSocket* sender, ClientState& state,
               const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = "NotLoggedIn";
                return;
            }
            int sessionId = request[Protocol::Field::kSessionId].toInt();
            QString text = request[Protocol::Field::kText].toString();
            Result<SessionQuestion> q = db_->addQuestion(
                sessionId, state.userId, std::chrono::system_clock::now(),
                text.toStdString());
            if (!q.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] =
                    QString::fromStdString(getErrorCode(q.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(q.value);
        });

    // List sessions for user
    handlers_.insert(Protocol::Request::kListCourses,
        [this](QWebSocket* sender, ClientState& state,
               const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = "NotLoggedIn";
                return;
            }
            Result<std::vector<Course>> courses = db_->listCoursesForUser(state.userId);
            if (!courses.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] =
                    QString::fromStdString(getErrorCode(courses.error));
                return;
            }
            QJsonObject data;
            data[Protocol::Field::kCourses] = Protocol::toJson(courses.value);
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = data;
        });

    // Get course detail
    handlers_.insert(Protocol::Request::kGetCourse,
        [this](QWebSocket* sender, ClientState& state,
               const QJsonObject& request, QJsonObject& reply) {
            if (!state.authenticated) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] = "NotLoggedIn";
                return;
            }
            int courseId = request[Protocol::Field::kCourseId].toInt();
            Result<Course> course = db_->getCourse(courseId, state.userId);
            if (!course.isOk()) {
                reply[Protocol::Field::kOk] = false;
                reply[Protocol::Reply::kError] =
                    QString::fromStdString(getErrorCode(course.error));
                return;
            }
            reply[Protocol::Field::kOk] = true;
            reply[Protocol::Field::kData] = Protocol::toJson(course.value);
        });

    // Logout (no-op at DB level; just clears state)
    handlers_.insert(Protocol::Request::kLogout,
        [this](QWebSocket* sender, ClientState& state,
               const QJsonObject& request, QJsonObject& reply) {
            state.authenticated = false;
            state.userId = -1;
            reply[Protocol::Field::kOk] = true;
        });
}

// ---- Connection handling ----

void PerplexServer::onNewConnection() {
    QWebSocket* client = server_->nextPendingConnection();
    connect(client, &QWebSocket::textMessageReceived, this,
            &PerplexServer::onTextMessageReceived);
    connect(client, &QWebSocket::disconnected, this,
            &PerplexServer::onClientDisconnected);
    clients_[client] = ClientState{};

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

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj[Protocol::Field::kType].toString();
    QJsonObject params = obj[Protocol::Field::kRequest].toObject();

    auto handlerIt = handlers_.find(type);
    if (handlerIt == handlers_.end()) {
        QJsonObject resp;
        resp[Protocol::Field::kType] = Protocol::Reply::kError;
        resp[Protocol::Field::kRequest] = type;
        resp[Protocol::Field::kCode] = "UnknownRequest";
        client->sendTextMessage(QJsonDocument(resp).toJson());
        return;
    }

    ClientState& state = clients_[client];
    QJsonObject response;
    response[Protocol::Field::kType] = type;
    response[Protocol::Field::kOk] = false;

    handlerIt.value()(client, state, params, response);

    client->sendTextMessage(QJsonDocument(response).toJson());
}

void PerplexServer::onClientDisconnected() {
    QWebSocket* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }
    clients_.remove(client);
    client->deleteLater();
}