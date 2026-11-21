// Regression test for the server auth op name matching between client
// ("logIn") and server (registered under the same spelling). Without the fix,
// a login request is routed to the unknown-op handler and returns WrongLogin
// even when the credentials are correct.

#include "doctest.h"

#include "database.h"
#include "error.h"
#include "serverclient.h"
#include "perplexserver.h"  // Updated: use new server header

#include <QCoreApplication>
#include <QThread>

static const int kTestPort = 19876;

// Block briefly so the server's event loop processes pending connections /
// messages before the client proceeds.
static void pump(int ms = 300) { QThread::msleep(ms); }

TEST_CASE("Server: register then login round-trips over WebSocket") {
    Database db(":memory:");
    REQUIRE(db.open());

    SocketsServer server(kTestPort, &db);
    REQUIRE(server.listen());
    pump();

    // Fresh client connection (simulates a fresh app start / logged-out state).
    ServerClient client("ws://localhost:" + QString::number(kTestPort));
    REQUIRE(client.isConnected());

    // 1. Create an account.
    const auto regRes = client.registerUser("ana", "pw");
    REQUIRE(regRes.isOk());
    CHECK(regRes.value.getId() > 0);
    pump();

    // 2. Log back in with the same password — this op must be handled.
    const auto loginRes = client.logIn("ana", "pw");
    REQUIRE(loginRes.isOk());
    CHECK(loginRes.error == Error::None);
    CHECK(loginRes.value.getName() == "ana");

    // 3. Wrong password still fails.
    const auto badLogin = client.logIn("ana", "wrong");
    CHECK_FALSE(badLogin.isOk());
    CHECK(badLogin.error == Error::WrongLogin);
}
