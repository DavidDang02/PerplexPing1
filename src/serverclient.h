// CLIENT SIDE: ServerClient wraps a QWebSocket connection and implements the
// DataStore interface by making blocking request/response calls over WebSocket.
//
// Each call sends a JSON request envelope with a unique ID, then spins a local
// QEventLoop until the matching response arrives. The caller thread is blocked
// (main thread for GUI app), which is acceptable for this prototype.

#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include <QWebSocket>
#include <QAbstractSocket>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>

#include "datastore.h"
#include "protocol.h"
#include "session.h"
#include "sessioninput.h"
#include "user.h"

class ServerClient : public DataStore {
public:
    // Connects to the given WebSocket endpoint (e.g. "ws://localhost:8080").
    // Blocks until the connection is established or the timeout expires.
    // Returns true on success.
    explicit ServerClient(const QString& url, int connectTimeoutMs = 5000);
    ~ServerClient() override;

    bool isConnected() const {
        return socket_.state() == QAbstractSocket::ConnectedState;
    }

    // Not copyable (holds a socket).
    ServerClient(const ServerClient&) = delete;
    ServerClient& operator=(const ServerClient&) = delete;

    // ---- Accounts ----
    Result<User> registerUser(const std::string& name,
                              const std::string& password) override;
    Result<User> logIn(const std::string& name,
                       const std::string& password) override;

    // ---- Sessions ----
    Result<Session> createSession(int userId) override;
    Result<SessionInput> saveInput(int sessionId, int userId,
                                   SessionInput input) override;
    Result<bool> saveSessionEnd(int sessionId,
                                Session::Clock::time_point endTime) override;
    Result<Session> loadSession(int sessionId) override;
    Result<std::vector<Session>> listSessionsForUser(int userId) override;

private:
    ProtocolResponse sendAndWait(const QJsonObject& request);

    ProtocolResponse sendAuthOp(const std::string& op, const QJsonObject& params);
    ProtocolResponse sendSessionOp(const std::string& op,
                                   const QJsonObject& params);

    int64_t nextId();

    QWebSocket socket_;
    QUrl url_;
    std::atomic<int64_t> nextRequestId_{1};
    int connectTimeoutMs_;
};