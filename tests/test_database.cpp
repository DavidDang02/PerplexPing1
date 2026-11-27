#include "doctest.h"

#include "database.h"
#include "error.h"

#include <QCoreApplication>
#include <QSqlError>

using namespace std::chrono;

TEST_SUITE("Database Users [F1]") {
TEST_CASE("Create user succeeds") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("alice", "[EMAIL]",
                                         "hash123", "salt456", 10000,
                                         system_clock::now());
    CHECK(created.isOk());
    CHECK(created.value().getId() > 0);
    CHECK(created.value().getName() == "alice");
    CHECK(created.value().getEmail() == "[EMAIL]");
}

TEST_CASE("Create duplicate username returns NameTaken") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> first = db.createUser("bob", "[EMAIL]", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("bob", "[EMAIL]", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(!second.isOk());
    CHECK(second.error == Error::NameTaken);
}

TEST_CASE("Create duplicate email returns EmailTaken") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> first = db.createUser("carol", "[EMAIL]", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("carol2", "[EMAIL]", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(!second.isOk());
    CHECK(second.error == Error::EmailTaken);
}

TEST_CASE("Create user with empty email stores NULL") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("dave", "", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());
    CHECK(created.value().getEmail() == "");
}

TEST_CASE("Get user by id") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("eve", "[EMAIL]", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> fetched = db.getUser(created.value().getId());
    CHECK(fetched.isOk());
    CHECK(fetched.value().getName() == "eve");
    CHECK(fetched.value().getEmail() == "[EMAIL]");
}

TEST_CASE("Get non-existent user returns error") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> fetched = db.getUser(99999);
    CHECK(!fetched.isOk());
}

TEST_CASE("Find user by name (case-insensitive)") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("Frank", "[EMAIL]", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> found = db.findUserByName("FRANK");
    CHECK(found.isOk());
    CHECK(found.value().getName() == "Frank");
}

TEST_CASE("Find user by email (case-insensitive)") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("Grace", "[EMAIL]", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> found = db.findUserByEmail("[EMAIL]");
    CHECK(found.isOk());
    CHECK(found.value().getName() == "Grace");
}

TEST_CASE("Get credentials for user") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("heidi", "[EMAIL]", "hash99",
                                         "salt99", 15000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<Credentials> creds = db.getCredentials(created.value().getId());
    CHECK(creds.isOk());
    CHECK(creds.value().userId == created.value().getId());
    CHECK(creds.value().hashHex == "hash99");
    CHECK(creds.value().saltHex == "salt99");
    CHECK(creds.value().iterations == 15000);
}

TEST_CASE("Get credentials for non-existent user returns error") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<Credentials> fetched = db.getCredentials(99999);
    CHECK(!fetched.isOk());
}

TEST_CASE("Set password succeeds") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("ivan", "[EMAIL]", "oldhash",
                                         "oldsalt", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<bool> ok = db.setPassword(created.value().getId(), "newhash", "newsalts", 20000);
    CHECK(ok.isOk());
}

TEST_CASE("Set email succeeds") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("judy", "[EMAIL]", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<bool> ok = db.setEmail(created.value().getId(), "[EMAIL]");
    CHECK(ok.isOk());

    Result<User> fetched = db.getUser(created.value().getId());
    CHECK(fetched.isOk());
    CHECK(fetched.value().getEmail() == "[EMAIL]");
}

TEST_CASE("Set email fails if another user has that email") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> first = db.createUser("kelly", "[EMAIL]", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("larry", "[EMAIL]", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(second.isOk());

    Result<bool> ok = db.setEmail(first.value().getId(), "[EMAIL]");
    CHECK(!ok.isOk());
}

TEST_CASE("Count live sessions hosted by new user") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    // No users or sessions yet
    int count = db.countLiveSessionsHostedBy(0);
    CHECK(count == 0);

    Result<User> created = db.createUser("mallory", "[EMAIL]", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    // No sessions hosted yet
    count = db.countLiveSessionsHostedBy(created.value().getId());
    CHECK(count == 0);
}
}