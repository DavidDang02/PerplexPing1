// SERVER SIDE: PerplexServer owns a Qt WebSocketServer and a SQLite Database.
// Each connected client is tracked per socket. Incoming JSON messages use
// the contract protocol ({"type":"...", ...fields...}).

#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <QHash>
#include <QHostAddress>
#include <QList>
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
    // The session this app is showing right now (0 = none). Set by
    // startSession / joinSession / openSession, cleared by closeSession and
    // logout. Live changes are pushed to every connection showing it.
    int openSessionId = 0;
    int openCourseId = 0;  // that session's course
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
    // Sends {"type":"loggedOut","reason":reason} to every connection of this
    // user except `except`, and logs each of them out.
    void logOutOtherConnections(QWebSocket* except, int userId, const QString& reason);

    // ---- Live push (CONTRACT.md 6, "Broadcast to others") ----
    // Builds the message for one recipient, or nothing if they may not see it.
    using PushBuilder = std::function<std::optional<QJsonObject>(const Viewer& viewer)>;
    // This connection's user and their role in the course, worked out now.
    Viewer getViewer(const ClientState& state, int courseId) const;
    // Sends to every logged-in connection except `except` that has this
    // session open. `build` runs once per recipient, so each one gets only
    // what Visibility lets them see.
    void pushToSession(QWebSocket* except, int sessionId, int courseId, const PushBuilder& build);
    // Every logged-in connection except `except` whose user is in this
    // course (its educator or a student).
    QList<QWebSocket*> getCourseConnections(QWebSocket* except, int courseId) const;
    // Sends {"type":"courseChanged","courseId":..}: the app reloads the course
    // page, and the session it shows (C7: toggles apply at once).
    void pushCourseChanged(QWebSocket* except, int courseId);
    // Sets (or clears, with 0) the session this connection shows, and
    // updates the participant list of the session it left and joined.
    void setOpenSession(ClientState& state, int sessionId, int courseId);
    // Sends {"type":"participants","sessionId":..,"participants":[..]} to
    // the educator's connections showing this session: the students showing
    // it right now (V5). Names are left out in an anonymous course. Queued,
    // so it goes out after the reply that caused it.
    void pushParticipantsLater(int sessionId, int courseId);

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