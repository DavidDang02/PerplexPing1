#include "doctest.h"

#include <QCoreApplication>
#include <QThread>

#include "database.h"
#include "error.h"
#include "perplexserver.h"
#include "serverclient.h"

static const int kTestPort = 19876;

static void pump(int ms = 250) { QThread::msleep(ms); }

TEST_CASE("Server: register then login round-trips over WebSocket with password verification") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());

    PerplexServer server(kTestPort, &db);
    REQUIRE(server.listen());
    pump();

    ServerClient client("ws://localhost:" + QString::number(kTestPort));
    REQUIRE(client.isConnected());

    // 1. Create an account with a valid school email
    const auto regRes = client.registerUser("ana", "valid_password123");
    REQUIRE(regRes.isOk());
    CHECK(regRes.value.getId() > 0);
    CHECK(regRes.value.getName() == "ana");
    pump();

    // 2. Log back in with the correct password
    const auto loginRes = client.logIn("ana", "valid_password123");
    REQUIRE(loginRes.isOk());
    CHECK(loginRes.value.getName() == "ana");

    // 3. Wrong password returns Error::WrongLogin
    const auto badLogin = client.logIn("ana", "wrong_password");
    CHECK_FALSE(badLogin.isOk());
    CHECK(badLogin.error == Error::WrongLogin);
}