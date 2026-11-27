#include "panoptooauthclient.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTextStream>
#include <QUrlQuery>

namespace {

QByteArray base64Url(const QByteArray& bytes) {
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

Recording::Clock::time_point toTimePoint(const QDateTime& time) {
    return Recording::Clock::time_point(std::chrono::milliseconds(time.toMSecsSinceEpoch()));
}

void reply(QTcpSocket* socket, const QByteArray& html) {
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n" + html);
    socket->disconnectFromHost();
}

}  // namespace

PanoptoOAuthClient::Settings PanoptoOAuthClient::loadSettings(const QString& envFile) {
    QHash<QString, QString> values;
    for (const QString& key : {QStringLiteral("PANOPTO_HOST"), QStringLiteral("PANOPTO_CLIENT_ID"),
                               QStringLiteral("PANOPTO_REDIRECT_URL")}) {
        const QByteArray fromEnv = qgetenv(key.toLatin1().constData());
        if (!fromEnv.isEmpty()) {
            values.insert(key, QString::fromUtf8(fromEnv).trimmed());
        }
    }
    QFile file(envFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            const int eq = line.indexOf('=');
            if (line.startsWith('#') || eq < 0) {
                continue;
            }
            const QString key = line.left(eq).trimmed();
            // Only these three; never a secret.
            if ((key == "PANOPTO_HOST" || key == "PANOPTO_CLIENT_ID" || key == "PANOPTO_REDIRECT_URL") &&
                !values.contains(key)) {
                values.insert(key, line.mid(eq + 1).trimmed());
            }
        }
    }
    Settings settings;
    settings.host = values.value("PANOPTO_HOST");
    settings.host.remove(QRegularExpression("^https?://"));
    settings.host.remove(QRegularExpression("/+$"));
    settings.clientId = values.value("PANOPTO_CLIENT_ID");
    if (values.contains("PANOPTO_REDIRECT_URL")) {
        settings.redirect = QUrl(values.value("PANOPTO_REDIRECT_URL"));
    }
    return settings;
}

PanoptoOAuthClient::PanoptoOAuthClient(Settings settings, QObject* parent)
    : PanoptoClient(parent), settings(std::move(settings)) {
    connect(&catcher, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket* socket = catcher.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onCallback(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}

bool PanoptoOAuthClient::isSignedIn() const {
    return !token.isEmpty();
}

void PanoptoOAuthClient::signIn(DoneHandler done) {
    waitingForSignIn.push_back(std::move(done));
    if (waitingForSignIn.size() > 1) {
        return;  // a sign-in is already open in the browser
    }
    const quint16 port = static_cast<quint16>(settings.redirect.port(8080));
    if (!catcher.isListening() && !catcher.listen(QHostAddress::LocalHost, port)) {
        finishSignIn(Error::PanoptoSignInFailed);  // port busy
        return;
    }
    // PKCE: send a hash of a random secret now, the secret itself with the code.
    QByteArray random(32, 0);
    for (char& c : random) {
        c = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    verifier = base64Url(random);
    const QByteArray challenge = base64Url(QCryptographicHash::hash(verifier, QCryptographicHash::Sha256));
    const QString state = QString::number(QRandomGenerator::global()->generate64(), 16);
    validStates.insert(state);

    QUrl authorize("https://" + settings.host + "/Panopto/oauth2/connect/authorize");
    QUrlQuery query;
    query.addQueryItem("client_id", settings.clientId);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("redirect_uri", settings.redirect.toString());
    query.addQueryItem("scope", "openid api");
    query.addQueryItem("state", state);
    query.addQueryItem("code_challenge", QString::fromLatin1(challenge));
    query.addQueryItem("code_challenge_method", "S256");
    authorize.setQuery(query);
    if (!QDesktopServices::openUrl(authorize)) {
        finishSignIn(Error::PanoptoSignInFailed);
    }
}

void PanoptoOAuthClient::onCallback(QTcpSocket* socket) {
    // Wait for the whole request (headers, plus a body if Panopto POSTs).
    buffers[socket] += socket->readAll();
    const QByteArray request = buffers[socket];
    const int headerEnd = request.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return;
    }
    const QByteArray body = request.mid(headerEnd + 4);
    const QByteArray lengthTag = "content-length:";
    const int at = request.left(headerEnd).toLower().indexOf(lengthTag);
    if (at >= 0) {
        const int end = request.indexOf("\r\n", at);
        if (body.size() < request.mid(at + lengthTag.size(), end - at - lengthTag.size()).trimmed().toInt()) {
            return;
        }
    }
    buffers.remove(socket);

    // "GET /callback?code=...&state=... HTTP/1.1"
    const QString first = QString::fromUtf8(request.left(request.indexOf("\r\n")));
    const QString method = first.section(' ', 0, 0);
    const QUrl got("http://localhost" + first.section(' ', 1, 1));
    QUrlQuery query(got);
    if (method == "POST") {
        query = QUrlQuery(QString::fromUtf8(body));
    }
    const QString path = settings.redirect.path();
    // Panopto may put the answer after a '#', which browsers never send to a
    // server: a tiny page copies it into a real request.
    if (got.path() == path && query.isEmpty()) {
        reply(socket, "<html><body><script>location.replace('/relay?' + location.hash.substring(1));"
                      "</script>Finishing sign-in...</body></html>");
        return;
    }
    if (got.path() != path && got.path() != "/relay") {
        reply(socket, "");  // e.g. favicon.ico
        return;
    }
    reply(socket, "<html><body style='font-family:sans-serif;text-align:center;margin-top:4em'>"
                  "<h2>PerplexPing</h2><p>You're signed in to Panopto. You can close this tab.</p>"
                  "</body></html>");
    catcher.close();
    if (query.hasQueryItem("error") ||
        !validStates.contains(query.queryItemValue("state", QUrl::FullyDecoded))) {
        finishSignIn(Error::PanoptoSignInFailed);
    } else if (query.hasQueryItem("access_token")) {
        token = query.queryItemValue("access_token", QUrl::FullyDecoded);
        finishSignIn(Error::None);
    } else if (query.hasQueryItem("code")) {
        exchangeCode(query.queryItemValue("code", QUrl::FullyDecoded));
    } else {
        finishSignIn(Error::PanoptoSignInFailed);
    }
}

void PanoptoOAuthClient::exchangeCode(const QString& code) {
    QNetworkRequest request(QUrl("https://" + settings.host + "/Panopto/oauth2/connect/token"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QUrlQuery body;
    body.addQueryItem("grant_type", "authorization_code");
    body.addQueryItem("code", code);
    body.addQueryItem("redirect_uri", settings.redirect.toString());
    body.addQueryItem("code_verifier", QString::fromLatin1(verifier));
    body.addQueryItem("client_id", settings.clientId);  // public client: no secret
    QNetworkReply* answer = net.post(request, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(answer, &QNetworkReply::finished, this, [this, answer] {
        answer->deleteLater();
        const QJsonObject json = QJsonDocument::fromJson(answer->readAll()).object();
        if (json.contains("access_token")) {
            token = json["access_token"].toString();
            finishSignIn(Error::None);
        } else {
            finishSignIn(answer->error() == QNetworkReply::NoError ||
                                 answer->error() >= QNetworkReply::ContentAccessDenied
                             ? Error::PanoptoSignInFailed
                             : Error::PanoptoUnreachable);
        }
    });
}

void PanoptoOAuthClient::finishSignIn(Error error) {
    std::vector<DoneHandler> waiting = std::exchange(waitingForSignIn, {});
    for (DoneHandler& done : waiting) {
        if (done) {
            done(error);
        }
    }
}

void PanoptoOAuthClient::get(const QString& path, JsonHandler done) {
    QNetworkRequest request(QUrl("https://" + settings.host + path));
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    QNetworkReply* answer = net.get(request);
    connect(answer, &QNetworkReply::finished, this, [this, answer, done] {
        answer->deleteLater();
        const int status = answer->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401) {
            token.clear();  // expired: the next press signs in again
            done(Error::PanoptoSignInFailed, {});
        } else if (answer->error() != QNetworkReply::NoError) {
            done(status == 404 || status == 400 ? Error::PanoptoLinkInvalid : Error::PanoptoUnreachable, {});
        } else {
            done(Error::None, QJsonDocument::fromJson(answer->readAll()).object());
        }
    });
}

QString PanoptoOAuthClient::getLectureId(const QString& lectureLink) {
    const QUrl url(lectureLink.trimmed());
    if (!url.isValid() || !url.host().contains("panopto", Qt::CaseInsensitive)) {
        return {};
    }
    const QString id = QUrlQuery(url).queryItemValue("id");
    static const QRegularExpression guid("^[0-9a-fA-F-]{8,64}$");
    return guid.match(id).hasMatch() ? id : QString();
}

void PanoptoOAuthClient::findFolderOfLink(const QString& lectureLink, FolderHandler done) {
    const QString id = getLectureId(lectureLink);
    if (id.isEmpty()) {
        done(Error::PanoptoLinkInvalid, {}, {});
        return;
    }
    get("/Panopto/api/v1/sessions/" + id, [done](Error error, const QJsonObject& lecture) {
        if (error != Error::None) {
            done(error, {}, {});
            return;
        }
        const QJsonObject folder = lecture["FolderDetails"].toObject();
        QString folderId = folder["Id"].toString();
        if (folderId.isEmpty()) {
            folderId = lecture["Folder"].toString();
        }
        if (folderId.isEmpty()) {
            done(Error::PanoptoLinkInvalid, {}, {});
            return;
        }
        done(Error::None, folderId, folder["Name"].toString());
    });
}

void PanoptoOAuthClient::listRecordings(const QString& folderId, RecordingsHandler done) {
    get("/Panopto/api/v1/folders/" + folderId + "/sessions?sortField=CreatedDate&sortOrder=Desc&pageNumber=0",
        [done](Error error, const QJsonObject& page) {
            if (error != Error::None) {
                done(error, {});
                return;
            }
            done(Error::None, parseRecordings(page));
        });
}

std::vector<RecordingCandidate> PanoptoOAuthClient::parseRecordings(const QJsonObject& page) {
    std::vector<RecordingCandidate> candidates;
    for (const QJsonValue& value : page["Results"].toArray()) {
        const QJsonObject recording = value.toObject();
        QDateTime start = QDateTime::fromString(recording["StartTime"].toString(), Qt::ISODate);
        if (!start.isValid()) {
            continue;  // still being set up in Panopto
        }
        start = QDateTime(start.date(), start.time());  // local wall-clock time, see header
        long long duration = static_cast<long long>(recording["Duration"].toDouble());
        if (duration <= 0) {
            duration = Recording::kDefaultDurationSeconds;
        }
        RecordingCandidate candidate;
        candidate.viewerUrl = recording["Urls"].toObject()["ViewerUrl"].toString().toStdString();
        candidate.name = recording["Name"].toString().toStdString();
        candidate.start = toTimePoint(start);
        candidate.durationSeconds = duration;
        if (!candidate.viewerUrl.empty()) {
            candidates.push_back(std::move(candidate));
        }
    }
    return candidates;
}
