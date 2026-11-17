// SERVER SIDE: SocketsServer owns a Qt WebSocketServer and a SQLite Database.
// Each connected client is authenticated once per connection and tracked
// per socket. Incoming JSON envelopes are dispatched to domain-specific
// handlers that mirror the DataStore API.

#pragma once

#include <functional>
#include <memory>

#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QUrl>
#include <QWebSocket>
#include <QWebSocketServer>

class Database;

// Authentication state for one connected client.
struct ClientState {
    bool authenticated = false;
    int userId = -1;
};

class SocketsServer : public QObject {
    Q_OBJECT
public:
    explicit SocketsServer(int port, Database* db,
                           QObject* parent = nullptr,
                           const QHostAddress& host = QHostAddress::LocalHost);
    ~SocketsServer() override;

    bool listen();
    int port() const;
    QHostAddress host() const { return host_; }
    QString lastError() const { return lastError_; }

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onClientDisconnected();

private:
    using Handler = std::function<void(QWebSocket*, ClientState&,
                                       const QJsonObject& params,
                                       QJsonObject& response)>;

    void registerHandlers();

    Database* db_;          // not owned
    QWebSocketServer* server_;
    int port_;
    QHostAddress host_;
    QString lastError_;

    // Authentication state keyed by socket pointer.
    QHash<QWebSocket*, ClientState> clients_;

    // Operation string -> handler.
    QHash<QString, Handler> handlers_;
};
