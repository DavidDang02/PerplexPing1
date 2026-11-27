#include "serverclient.h"

#include <QDateTime>

#include "protocol.h"

ServerClient::ServerClient(const QUrl& url, QObject* parent) : QObject(parent), url(url) {
    connect(&socket, &QWebSocket::connected, this, [this] {
        reconnectTimer.stop();
        emit connectionChanged(true);
    });
    connect(&socket, &QWebSocket::disconnected, this, &ServerClient::onDisconnected);
    connect(&socket, &QWebSocket::textMessageReceived, this, &ServerClient::onText);

    reconnectTimer.setInterval(kReconnectMs);
    connect(&reconnectTimer, &QTimer::timeout, this, [this] {
        if (socket.state() == QAbstractSocket::UnconnectedState) {
            socket.open(this->url);
        }
    });

    timeoutTimer.setInterval(500);
    connect(&timeoutTimer, &QTimer::timeout, this, &ServerClient::checkTimeouts);
}

ServerClient::~ServerClient() {
    started = false;
    QObject::disconnect(&socket, nullptr, this, nullptr);
    pending.clear();
    pendingTypes.clear();
    socket.close();
}

void ServerClient::start() {
    started = true;
    timeoutTimer.start();
    socket.open(url);
    // If the first try fails, `disconnected` fires and the timer retries.
    reconnectTimer.start();
}

bool ServerClient::isConnected() const {
    return socket.state() == QAbstractSocket::ConnectedState;
}

QUrl ServerClient::getUrl() const {
    return url;
}

int ServerClient::send(const QString& type, const QJsonObject& fields, ReplyHandler onReply) {
    const int id = nextId++;
    if (!isConnected()) {
        // Answer on the next turn of the event loop, like a real reply, so
        // callers never see their callback run inside send().
        QMetaObject::invokeMethod(this, [onReply, type] {
            if (onReply) {
                onReply(makeUnreachable(type));
            }
        }, Qt::QueuedConnection);
        return id;
    }
    QJsonObject message = fields;
    message[Protocol::Field::kId] = id;
    message[Protocol::Field::kType] = type;
    pending.insert(id, Pending{std::move(onReply), QDateTime::currentMSecsSinceEpoch() + kReplyTimeoutMs});
    pendingTypes.insert(id, type);
    socket.sendTextMessage(Protocol::encode(message));
    return id;
}

Error ServerClient::getError(const QJsonObject& reply) {
    if (reply[Protocol::Field::kOk].toBool(false)) {
        return Error::None;
    }
    QString code = reply[Protocol::Field::kCode].toString();
    if (code.isEmpty()) {
        code = reply[Protocol::Reply::kError].toString();
    }
    return code.isEmpty() ? Error::BadRequest : getErrorFromCode(code.toStdString());
}

void ServerClient::onText(const QString& text) {
    const QJsonObject message = Protocol::decode(text);
    if (message.isEmpty()) {
        return;  // not JSON: ignore it, the server never sends that
    }
    if (!message.contains(Protocol::Field::kOk)) {
        emit pushReceived(message);
        return;
    }
    // A reply. Match it by id; a reply without one (e.g. to unreadable
    // text) has no waiting request.
    const int id = message[Protocol::Field::kId].toInt(0);
    auto it = pending.find(id);
    if (it == pending.end()) {
        return;
    }
    ReplyHandler handler = std::move(it->onReply);
    pending.erase(it);
    pendingTypes.remove(id);
    if (handler) {
        handler(message);
    }
}

void ServerClient::onDisconnected() {
    failAll();
    emit connectionChanged(false);
    if (started && !reconnectTimer.isActive()) {
        reconnectTimer.start();
    }
}

void ServerClient::failAll() {
    // Take them all first: a handler may send a new request.
    const QHash<int, Pending> waiting = std::exchange(pending, {});
    const QHash<int, QString> types = std::exchange(pendingTypes, {});
    for (auto it = waiting.cbegin(); it != waiting.cend(); ++it) {
        if (it->onReply) {
            it->onReply(makeUnreachable(types.value(it.key())));
        }
    }
}

void ServerClient::checkTimeouts() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<int> late;
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        if (it->deadlineMs <= now) {
            late.append(it.key());
        }
    }
    for (int id : late) {
        Pending waiting = pending.take(id);
        const QString type = pendingTypes.take(id);
        if (waiting.onReply) {
            waiting.onReply(makeUnreachable(type));
        }
    }
}

QJsonObject ServerClient::makeUnreachable(const QString& type) {
    QJsonObject reply;
    reply[Protocol::Field::kType] = type;
    reply[Protocol::Field::kOk] = false;
    reply[Protocol::Field::kCode] = QString::fromLatin1(getErrorCode(Error::ServerUnreachable));
    return reply;
}
