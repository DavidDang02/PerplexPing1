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

      std::optional<User> newUser = db_->registerUser(name, password);
      if (!newUser) {
        resp["ok"] = false;
        resp["error"] = "NameTaken";
        return;
      }
      state.authenticated = true;
      state.userId = newUser->getId();
      resp["ok"] = true;
      resp["data"] = userToJson(*newUser);
    });

  handlers_.insert("login",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      std::string name = params["name"].toString().toStdString();
      std::string password = params["password"].toString().toStdString();

      std::optional<User> found = db_->login(name, password);
      if (!found) {
        resp["ok"] = false;
        resp["error"] = "WrongLogin";
        return;
      }
      state.authenticated = true;
      state.userId = found->getId();
      resp["ok"] = true;
      resp["data"] = userToJson(*found);
    });

  handlers_.insert("createSession",
                   [this](QWebSocket* sender, ClientState& state,
                          const QJsonObject& params, QJsonObject& resp) {
      if (!state.authenticated) {
        resp["ok"] = false;
        resp["error"] = "NotLoggedIn";
        return;
      }
      Session session = db_->createSession(state.userId);
      resp["ok"] = true;
      resp["data"] = sessionToJson(session);
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

      db_->saveInput(sessionId, userId, input);

      resp["ok"] = true;
      QJsonObject data;
      data["inputId"] = input.getId();
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

      db_->saveSessionEnd(sessionId, jsonStringToTime(endStr));

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

      std::optional<Session> session = db_->loadSession(sessionId);
      if (!session) {
        resp["ok"] = false;
        resp["error"] = "NotFound";
        return;
      }
      resp["ok"] = true;
      resp["data"] = sessionToJson(*session);
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

      std::vector<Session> sessions = db_->listSessionsForUser(userId);
      QJsonArray sessionsArr;
      for (const Session& s : sessions) {
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