// CONTROLLER (external system): the real PanoptoClient, for SCU's Panopto.
// Ported from the Nov 9 spike that matched a CSCI 187 lecture end to end.
//
// Sign-in is OAuth 2 "authorization code + PKCE" for a desktop app:
//   1. open https://<host>/Panopto/oauth2/connect/authorize in the browser
//      (the SCU sign-in page; the app never sees the password),
//   2. catch the redirect on http://localhost:<port>/callback with a tiny
//      local web server,
//   3. trade the one-time code (+ the PKCE verifier) for an access token.
// No client secret is used or read: the app is a public client (the course's
// rule: API keys stay off the front end and off GitHub). The token stays in
// memory for this run of the app only.
//
// Settings (not secret, but kept out of the repo): PANOPTO_HOST,
// PANOPTO_CLIENT_ID and optionally PANOPTO_REDIRECT_URL, from the
// environment or from a git-ignored .env file in the folder the app starts
// in. Any other line of that file (e.g. a client secret) is ignored.

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSet>
#include <QString>
#include <QTcpServer>
#include <QUrl>

#include "panoptoclient.h"

class QTcpSocket;

class PanoptoOAuthClient : public PanoptoClient {
    Q_OBJECT

public:
    struct Settings {
        QString host;      // e.g. "santaclarauniversity.hosted.panopto.com"
        QString clientId;
        QUrl redirect{QStringLiteral("http://localhost:8080/callback")};
        bool isComplete() const { return !host.isEmpty() && !clientId.isEmpty(); }
    };

    // Reads the settings from the environment, then from `envFile` for any
    // still missing. Only the three PANOPTO_ keys above are read.
    static Settings loadSettings(const QString& envFile = QStringLiteral(".env"));

    explicit PanoptoOAuthClient(Settings settings, QObject* parent = nullptr);

    bool isSignedIn() const override;
    void signIn(DoneHandler done) override;
    void findFolderOfLink(const QString& lectureLink, FolderHandler done) override;
    void listRecordings(const QString& folderId, RecordingsHandler done) override;

    // The recording id in a Viewer.aspx?id=... (or Embed.aspx?id=...) link,
    // "" if there is none.
    static QString getLectureId(const QString& lectureLink);
    // Panopto's folder-session list -> candidates. SCU's Panopto writes the
    // local wall-clock start time with a "Z" (e.g. "14:15:00Z" for a 2:15 PM
    // lecture), so the date and time are read as local time; an empty
    // Duration becomes Recording::kDefaultDurationSeconds.
    static std::vector<RecordingCandidate> parseRecordings(const QJsonObject& page);

private:
    using JsonHandler = std::function<void(Error error, const QJsonObject& body)>;

    void onCallback(QTcpSocket* socket);
    void exchangeCode(const QString& code);
    void finishSignIn(Error error);
    void get(const QString& path, JsonHandler done);

    Settings settings;
    QString token;
    QByteArray verifier;
    QSet<QString> validStates;
    QTcpServer catcher;
    QHash<QTcpSocket*, QByteArray> buffers;
    QNetworkAccessManager net;
    std::vector<DoneHandler> waitingForSignIn;
};
