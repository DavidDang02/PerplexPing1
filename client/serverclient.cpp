// CLIENT SIDE: ServerClient wraps a QWebSocket connection and implements the
// DataStore interface by making blocking request/response calls over WebSocket.

#include "serverclient.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QWebSocket>

ServerClient::ServerClient(const QString& url, int connectTimeoutMs)
    : url_(QUrl(url)), connectTimeoutMs_(connectTimeoutMs) {
  if (url.isEmpty()) {
    return;
  }

  // Block the caller until the socket connects, errors out, or times out.
  QEventLoop connectLoop;
  QObject::connect(&socket_, &QWebSocket::connected, &connectLoop,
                   &QEventLoop::quit);
  QObject::connect(&socket_, &QWebSocket::errorOccurred, &connectLoop,
                   &QEventLoop::quit);
  QTimer::singleShot(connectTimeoutMs_, &connectLoop, &QEventLoop::quit);

  socket_.open(url_);
  connectLoop.exec();
}

ServerClient::~ServerClient() {
  if (socket_.state() != QAbstractSocket::UnconnectedState) {
    socket_.disconnect();
    socket_.close();
  }
}

int64_t ServerClient::nextId() { return nextRequestId_++; }

ProtocolResponse ServerClient::sendAndWait(const QJsonObject& request) {
  ProtocolResponse response;

  if (socket_.state() == QAbstractSocket::UnconnectedState) {
    return response;  // ok=false, error empty
  }

  QString requestJson =
      QJsonDocument(request).toJson(QJsonDocument::Compact);
  socket_.sendTextMessage(requestJson);

  QEventLoop responseLoop;
  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);

  QStringList pendingIds;
  int64_t requestId = request["id"].toInteger(0);

  auto textReceived = [&response, &responseLoop, &pendingIds,
                       requestId](const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
      return;
    }
    QJsonObject obj = doc.object();
    ProtocolResponse resp;
    parseResponse(obj, resp);
    if (resp.id == requestId) {
      response.id = resp.id;
      response.ok = resp.ok;
      response.error = resp.error;
      response.data = resp.data;
      responseLoop.quit();
    }
  };

  QTimer::singleShot(connectTimeoutMs_, &responseLoop, &QEventLoop::quit);
  QObject::connect(&socket_, &QWebSocket::textMessageReceived,
                   &responseLoop, &QEventLoop::quit);
  QObject::connect(&socket_, &QWebSocket::textMessageReceived,
                   textReceived);

  responseLoop.exec();

  return response;
}

ProtocolResponse ServerClient::sendAuthOp(const std::string& op,
                                          const QJsonObject& params) {
  QJsonObject request;
  request["id"] = QJsonValue(static_cast<qint64>(nextId()));
  request["op"] = QString::fromStdString(op);
  request["params"] = params;
  return sendAndWait(request);
}

ProtocolResponse ServerClient::sendSessionOp(const std::string& op,
                                             const QJsonObject& params) {
  QJsonObject request;
  request["id"] = QJsonValue(static_cast<qint64>(nextId()));
  request["op"] = QString::fromStdString(op);
  request["params"] = params;
  return sendAndWait(request);
}

Result<User> ServerClient::registerUser(const std::string& name,
                                        const std::string& password) {
  QJsonObject params;
  params["name"] = QString::fromStdString(name);
  params["password"] = QString::fromStdString(password);

  ProtocolResponse resp = sendAuthOp("registerUser", params);
  if (!resp.ok) {
    return getErrorFromCode(resp.error);
  }

  User user;
  userFromJson(resp.data, user);
  return user;
}

Result<User> ServerClient::logIn(const std::string& name,
                                 const std::string& password) {
  QJsonObject params;
  params["name"] = QString::fromStdString(name);
  params["password"] = QString::fromStdString(password);

  ProtocolResponse resp = sendAuthOp("logIn", params);
  if (!resp.ok) {
    return getErrorFromCode(resp.error);
  }

  User user;
  userFromJson(resp.data, user);
  return user;
}

Result<Session> ServerClient::createSession(int userId) {
  QJsonObject params;
  params["userId"] = userId;

  ProtocolResponse resp = sendSessionOp("createSession", params);
  if (!resp.ok) {
    return getErrorFromCode(resp.error);
  }

  Session session;
  sessionFromJson(resp.data, session);
  return session;
}

Result<SessionInput> ServerClient::saveInput(int sessionId, int userId,
                                             SessionInput input) {
  QJsonObject params;
  params["sessionId"] = sessionId;
  params["userId"] = userId;
  params["input"] = sessionInputToJson(input);

  ProtocolResponse resp = sendSessionOp("saveInput", params);
  if (!resp.ok) {
    return getErrorFromCode(resp.error);
  }

  int inputId = resp.data["inputId"].toInt(0);
  input.setId(inputId);
  return input;
}

Result<bool> ServerClient::saveSessionEnd(int sessionId,
                                          Session::Clock::time_point endTime) {
  QJsonObject params;
  params["sessionId"] = sessionId;
  params["endTime"] = QString::fromStdString(timeToJsonString(endTime));

  ProtocolResponse resp = sendSessionOp("saveSessionEnd", params);
  if (!resp.ok) {
    return getErrorFromCode(resp.error);
  }
  return true;
}

Result<Session> ServerClient::loadSession(int sessionId) {
  QJsonObject params;
  params["sessionId"] = sessionId;

  ProtocolResponse resp = sendSessionOp("loadSession", params);
  if (!resp.ok) {
    return getErrorFromCode(resp.error);
  }

  Session session;
  sessionFromJson(resp.data, session);
  return session;
}

Result<std::vector<Session>> ServerClient::listSessionsForUser(int userId) {
  QJsonObject params;
  params["userId"] = userId;

  ProtocolResponse resp = sendSessionOp("listSessionsForUser", params);
  if (!resp.ok) {
    return getErrorFromCode(resp.error);
  }

  std::vector<Session> sessions;
  QJsonArray sessionsArr = resp.data["sessions"].toArray();
  for (const QJsonValue& val : sessionsArr) {
    Session session;
    sessionFromJson(val.toObject(), session);
    sessions.push_back(session);
  }
  return sessions;
}