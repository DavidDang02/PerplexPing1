#include "doctest.h"

#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include "database.h"
#include "error.h"
#include "passwordhasher.h"
#include "perplexserver.h"
#include "protocol.h"

TEST_CASE("Server: register then login round-trips over WebSocket with password verification") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());

    PerplexServer server(0, &db, nullptr, QHostAddress::LocalHost,
                         PasswordHasher::kTestIterations);
    REQUIRE(server.listen());
    const int port = server.port();
    REQUIRE(port > 0);

    QWebSocket socket;
    QList<QJsonObject> inbox;
    QEventLoop connectLoop;
    QEventLoop waitLoop;

    QObject::connect(&socket, &QWebSocket::connected, &connectLoop, &QEventLoop::quit);
    QObject::connect(&socket, &QWebSocket::errorOccurred, &connectLoop, &QEventLoop::quit);
    QObject::connect(&socket, &QWebSocket::textMessageReceived, &waitLoop,
                     [&](const QString& message) {
                         const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
                         if (doc.isObject()) {
                             inbox.append(doc.object());
                         }
                         waitLoop.quit();
                     });

    QTimer::singleShot(5000, &connectLoop, &QEventLoop::quit);
    socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    connectLoop.exec();
    INFO("socket error: ", socket.errorString().toStdString());
    REQUIRE(socket.state() == QAbstractSocket::ConnectedState);

    auto nextMessage = [&]() -> QJsonObject {
        if (inbox.isEmpty()) {
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(&timeout, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
            timeout.start(5000);
            waitLoop.exec();
        }
        if (inbox.isEmpty()) {
            return QJsonObject{};
        }
        return inbox.takeFirst();
    };

    // Consume initial hello message from server
    const QJsonObject hello = nextMessage();
    INFO("hello: ", QJsonDocument(hello).toJson(QJsonDocument::Compact).toStdString());
    REQUIRE(hello[Protocol::Field::kType].toString() == Protocol::Reply::kHello);

    // 1. Create an account with a valid school email
    QJsonObject regReq = Protocol::makeMessage(Protocol::Request::kRegister);
    regReq[Protocol::Field::kName] = QStringLiteral("ana");
    regReq[Protocol::Field::kPassword] = QStringLiteral("valid_password123");
    regReq[Protocol::Field::kEmail] = QStringLiteral("ana@scu.edu");
    socket.sendTextMessage(Protocol::encode(regReq));

    const QJsonObject regResp = nextMessage();
    INFO("regResp: ", QJsonDocument(regResp).toJson(QJsonDocument::Compact).toStdString());
    REQUIRE(regResp[Protocol::Field::kOk].toBool());
    const QJsonObject regData = regResp[Protocol::Field::kData].toObject();
    CHECK(regData[Protocol::Field::kId].toInt() > 0);
    CHECK(regData[Protocol::Field::kName].toString() == QStringLiteral("ana"));

    // 2. Log back in with the correct password
    QJsonObject loginReq = Protocol::makeMessage(Protocol::Request::kLogin);
    loginReq[Protocol::Field::kName] = QStringLiteral("ana");
    loginReq[Protocol::Field::kPassword] = QStringLiteral("valid_password123");
    socket.sendTextMessage(Protocol::encode(loginReq));

    const QJsonObject loginResp = nextMessage();
    INFO("loginResp: ", QJsonDocument(loginResp).toJson(QJsonDocument::Compact).toStdString());
    REQUIRE(loginResp[Protocol::Field::kOk].toBool());
    CHECK(loginResp[Protocol::Field::kData].toObject()[Protocol::Field::kName].toString() ==
          QStringLiteral("ana"));

    // 3. Wrong password returns Error::WrongLogin
    QJsonObject badLoginReq = Protocol::makeMessage(Protocol::Request::kLogin);
    badLoginReq[Protocol::Field::kName] = QStringLiteral("ana");
    badLoginReq[Protocol::Field::kPassword] = QStringLiteral("wrong_password");
    socket.sendTextMessage(Protocol::encode(badLoginReq));

    const QJsonObject badLoginResp = nextMessage();
    INFO("badLoginResp: ", QJsonDocument(badLoginResp).toJson(QJsonDocument::Compact).toStdString());
    CHECK_FALSE(badLoginResp[Protocol::Field::kOk].toBool());
    const QString errorCode = badLoginResp[Protocol::Reply::kError].toString();
    CHECK(getErrorFromCode(errorCode.toStdString()) == Error::WrongLogin);
}