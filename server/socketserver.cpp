// SERVER SIDE: SocketsServer implementation.

#include "socketserver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include "database.h"
#include "protocol.h"
#include "session.h"
#include "sessioninput.h"
#include "user.h"

SocketsServer::SocketsServer(int port, Database* db, QObject* parent)
    : QObject(parent), db_(db), port_(port) {
  server_ = new QWebSocketServer(QStringLiteral("PerplexPingServer"),
                                  QWebSocketServer::NonSecureMode, this);

  registerHandlers();

  connect(server_, &QWebSocketServer::newConnection, this,
          &SocketsServer::onNewConnection);
}

SocketsServer::~SocketsServer() {
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    it.key()->deleteLater();
  }
  clients_.clear();
}

bool SocketsServer::listen() {
  if (!server_->listen(QHostAddress::LocalHost, port_)) {
    lastError_ = server_->errorString();
    qDebug() << "Failed to listen:" << lastError_;
    return false;
  }
  qDebug() << "Server listening on port" << port_;
  return true;
}

int SocketsServer::port() const {
  return server_ ? server_->serverPort() : 0;
}

void SocketsServer::registerHandlers() {
  handlers_.insert("registerUser",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      std::string name = params["name"].toString().toStdString();
      std::string password = params["password"].toString().toStdString();

      Result<User> newUser = db_->registerUser(name, password);
      if (!newUser.isOk()) {
        resp["ok"] = false;
        resp["error"] = QString::fromStdString(getErrorCode(newUser.error));
        return;
      }
      state.authenticated = true;
      state.userId = newUser.value.getId();
      resp["ok"] = true;
      resp["data"] = userToJson(newUser.value);
    });

  handlers_.insert("logIn",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      std::string name = params["name"].toString().toStdString();
      std::string password = params["password"].toString().toStdString();

      Result<User> found = db_->logIn(name, password);
      if (!found.isOk()) {
        resp["ok"] = false;
        resp["error"] = QString::fromStdString(getErrorCode(found.error));
        return;
      }
      state.authenticated = true;
      state.userId = found.value.getId();
      resp["ok"] = true;
      resp["data"] = userToJson(found.value);
    });

  handlers_.insert("createSession",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      if (!state.authenticated) {
        resp["ok"] = false;
        resp["error"] = "NotLoggedIn";
        return;
      }
      Result<Session> session = db_->createSession(state.userId);
      if (!session.isOk()) {
        resp["ok"] = false;
        resp["error"] = QString::fromStdString(getErrorCode(session.error));
        return;
      }
      resp["ok"] = true;
      resp["data"] = sessionToJson(session.value);
    });

  handlers_.insert("saveInput",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      if (!state.authenticated) {
        resp["ok"] = false;
        resp["error"] = "NotLoggedIn";
        return;
      }

      int sessionId = params["sessionId"].toInt();
      int userId = params["userId"].toInt(state.userId);
      QJsonObject inputObj = params["input"].toObject();
      if (inputObj.isEmpty()) {
        resp["ok"] = false;
        resp["error"] = "MissingInput";
        return;
      }

      SessionInput input;
      sessionInputFromJson(inputObj, input);

      Result<SessionInput> saved = db_->saveInput(sessionId, userId, input);
      if (!saved.isOk()) {
        resp["ok"] = false;
        resp["error"] = QString::fromStdString(getErrorCode(saved.error));
        return;
      }

      resp["ok"] = true;
      QJsonObject data;
      data["inputId"] = saved.value.getId();
      resp["data"] = data;
    });

  handlers_.insert("saveSessionEnd",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      if (!state.authenticated) {
        resp["ok"] = false;
        resp["error"] = "NotLoggedIn";
        return;
      }

      int sessionId = params["sessionId"].toInt();
      std::string endStr = params["endTime"].toString().toStdString();

      Result<bool> ok = db_->saveSessionEnd(sessionId, jsonStringToTime(endStr));
      if (!ok.isOk()) {
        resp["ok"] = false;
        resp["error"] = QString::fromStdString(getErrorCode(ok.error));
        return;
      }

      resp["ok"] = true;
    });

  handlers_.insert("loadSession",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      if (!state.authenticated) {
        resp["ok"] = false;
        resp["error"] = "NotLoggedIn";
        return;
      }

      int sessionId = params["sessionId"].toInt();

      Result<Session> session = db_->loadSession(sessionId);
      if (!session.isOk()) {
        resp["ok"] = false;
        resp["error"] = QString::fromStdString(getErrorCode(session.error));
        return;
      }
      resp["ok"] = true;
      resp["data"] = sessionToJson(session.value);
    });

  handlers_.insert("listSessionsForUser",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      if (!state.authenticated) {
        resp["ok"] = false;
        resp["error"] = "NotLoggedIn";
        return;
      }

      int userId = params["userId"].toInt(state.userId);

      Result<std::vector<Session>> sessionsResult =
          db_->listSessionsForUser(userId);
      if (!sessionsResult.isOk()) {
        resp["ok"] = false;
        resp["error"] = QString::fromStdString(
            getErrorCode(sessionsResult.error));
        return;
      }
      QJsonArray sessionsArr;
      for (const Session& s : sessionsResult.value) {
        sessionsArr.append(sessionToJson(s));
      }
      QJsonObject data;
      data["sessions"] = sessionsArr;
      resp["ok"] = true;
      resp["data"] = data;
    });
}

void SocketsServer::onNewConnection() {
  QWebSocket* client = server_->nextPendingConnection();
  connect(client, &QWebSocket::textMessageReceived, this,
          &SocketsServer::onTextMessageReceived);
  connect(client, &QWebSocket::disconnected, this,
          &SocketsServer::onClientDisconnected);
  clients_[client] = ClientState{};
}

void SocketsServer::onTextMessageReceived(const QString& message) {
  QWebSocket* client = qobject_cast<QWebSocket*>(sender());
  if (!client) {
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
  if (!doc.isObject()) {
    return;
  }

  QJsonObject obj = doc.object();
  int id = static_cast<int>(obj["id"].toInteger(0));
  std::string op = obj["op"].toString().toStdString();
  QJsonObject params = obj["params"].toObject();

  auto handlerIt = handlers_.find(QString::fromStdString(op));
  if (handlerIt == handlers_.end()) {
    QJsonObject resp;
    resp["id"] = id;
    resp["ok"] = false;
    resp["error"] = "UnknownOp";
    client->sendTextMessage(QJsonDocument(resp).toJson());
    return;
  }

  ClientState& state = clients_[client];
  QJsonObject response;
  response["id"] = id;
  response["ok"] = false;

  handlerIt.value()(client, state, params, response);

  client->sendTextMessage(QJsonDocument(response).toJson());
}

void SocketsServer::onClientDisconnected() {
  QWebSocket* client = qobject_cast<QWebSocket*>(sender());
  if (!client) {
    return;
  }
  clients_.remove(client);
  client->deleteLater();
}