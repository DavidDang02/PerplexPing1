// SERVER SIDE: PerplexServer owns a Qt WebSocketServer and a SQLite Database.
// Each connected client is tracked per socket. Incoming JSON messages use
// the contract protocol ({"type":"...", ...fields...}).

#pragma once

#include <functional>
#include <memory>

#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QWebSocket>
#include <QWebSocketServer>

#include "appmodel.h"
#include "passwordhasher.h"

class Database;

struct ClientState {
    bool authenticated = false;
    int userId = -1;
    std::shared_ptr<AppModel> model;
};

class PerplexServer : public QObject {
    Q_OBJECT
public:
    explicit PerplexServer(int port, Database* db,
                           QObject* parent = nullptr,
                           const QHostAddress& host = QHostAddress::LocalHost,
                           int passwordIterations = PasswordHasher::kDefaultIterations);
    ~PerplexServer() override;

    bool listen();
    int port() const;
    QHostAddress host() const;
    QString lastError() const;

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onClientDisconnected();

private:
    // Handler signature: receives the sender socket, client state, and the
    // full request message. Must fill `reply` with the response object.
    using Handler = std::function<void(QWebSocket*, ClientState&,
                                       const QJsonObject& request,
                                       QJsonObject& reply)>;

    void registerHandlers();

    Database* db_;
    QWebSocketServer* server_;
    int port_;
    QHostAddress host_;
    int passwordIterations_;
    QString lastError_;

    // Authentication state keyed by socket pointer.
    QHash<QWebSocket*, ClientState> clients_;

    // Request type string -> handler.
    QHash<QString, Handler> handlers_;
};

// Compatibility typedef
using SocketsServer = PerplexServer;