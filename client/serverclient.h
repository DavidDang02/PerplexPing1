// CONTROLLER (network): the app's one WebSocket connection to PerplexServer.
// Sends requests and hands each reply to the callback given with it; hands
// pushes (messages that are not replies) to whoever listens to
// pushReceived. Never blocks: the window keeps responding while it waits.
//
// It knows the message envelope (CONTRACT.md 6), nothing about what the
// messages mean. AppController does that.
//
//   client.send(Protocol::Request::kAddLost, {{"sessionId", 7}},
//               [](const QJsonObject& reply) { ...reply["ok"]... });
//
// If the connection drops, every waiting request gets a failed reply with
// code ServerUnreachable, and the client tries to reconnect every couple of
// seconds until it is back (connectionChanged tells the app both times).

#pragma once

#include <functional>

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include "error.h"

class ServerClient : public QObject {
    Q_OBJECT

public:
    using ReplyHandler = std::function<void(const QJsonObject& reply)>;

    // How long a request may wait for its reply before it fails with
    // ServerUnreachable.
    static constexpr int kReplyTimeoutMs = 10000;
    // How often to try again after the connection is lost.
    static constexpr int kReconnectMs = 2000;

    explicit ServerClient(const QUrl& url, QObject* parent = nullptr);
    // Closes the connection without calling any waiting callback (their
    // owners may already be gone).
    ~ServerClient() override;

    // Opens the connection (and keeps reopening it if it drops).
    void start();
    bool isConnected() const;
    QUrl getUrl() const;

    // Sends {"id": n, "type": type, ...fields}. `onReply` runs exactly once:
    // with the server's reply, or with {"ok": false, "code":
    // "ServerUnreachable"} if there is no connection, it drops, or no reply
    // comes within kReplyTimeoutMs. Returns the request id.
    int send(const QString& type, const QJsonObject& fields, ReplyHandler onReply);

    // The error in a failed reply (None if it worked).
    static Error getError(const QJsonObject& reply);

signals:
    void connectionChanged(bool connected);
    void pushReceived(const QJsonObject& push);  // has "type", never "ok"

private:
    struct Pending {
        ReplyHandler onReply;
        qint64 deadlineMs = 0;
    };

    void onText(const QString& text);
    void onDisconnected();
    void failAll();
    void checkTimeouts();
    static QJsonObject makeUnreachable(const QString& type);

    QWebSocket socket;
    QUrl url;
    QHash<int, Pending> pending;
    QHash<int, QString> pendingTypes;
    int nextId = 1;
    bool started = false;
    QTimer reconnectTimer;
    QTimer timeoutTimer;
};
