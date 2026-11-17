// CLIENT SIDE: ServerClient wraps a QWebSocket connection and implements the
// DataStore interface by making blocking request/response calls over WebSocket.
//
// Each call sends a JSON request envelope with a unique ID, then spins a local
// QEventLoop until the matching response arrives. The caller thread is blocked
// (main thread for GUI app), which is acceptable for this prototype.

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <atomic>

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
    std::optional<User> registerUser(const std::string& name,
                                     const std::string& password) override;
    std::optional<User> login(const std::string& name,
                              const std::string& password) override;

    // ---- Sessions ----
    Session createSession(int userId) override;
    void saveInput(int sessionId, int userId, SessionInput& input) override;
    void saveSessionEnd(int sessionId,
                        Session::Clock::time_point endTime) override;
    std::optional<Session> loadSession(int sessionId) override;
    std::vector<Session> listSessionsForUser(int userId) override;

private:
    // Sends a request, waits for the response with the same id, and parses it.
    // Returns the ProtocolResponse, with ok==false if transport failed.
    ProtocolResponse sendAndWait(const QJsonObject& request);

    // Generic helpers for session requests where the server uses the
    // authenticated user from the connection.
    ProtocolResponse sendAuthOp(const std::string& op, const QJsonObject& params);
    ProtocolResponse sendSessionOp(const std::string& op,
                                   const QJsonObject& params);

    // Generates the next request ID.
    int64_t nextId();

    QWebSocket socket_;
    QUrl url_;
    std::atomic<int64_t> nextRequestId_{1};
    int connectTimeoutMs_;
};