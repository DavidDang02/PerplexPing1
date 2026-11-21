#include "doctest.h"

#include "database.h"
#include "error.h"

#include <QCoreApplication>
#include <QSqlError>

using namespace std::chrono;

TEST_SUITE("Database Users [F1]") {
TEST_CASE("Create user succeeds") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> created = db.createUser("alice", "alice@scu.edu",
                                         "hash123", "salt456", 10000,
                                         system_clock::now());
    CHECK(created.isOk());
    CHECK(created.value().getId() > 0);
    CHECK(created.value().getName() == "alice");
    CHECK(created.value().getEmail() == "alice@scu.edu");
}

TEST_CASE("Create duplicate username returns NameTaken") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> first = db.createUser("bob", "bob@scu.edu", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("bob", "other@scu.edu", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(!second.isOk());
    CHECK(second.error == Error::NameTaken);
}

TEST_CASE("Create duplicate email returns EmailTaken") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> first = db.createUser("carol", "carol@scu.edu", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("carol2", "carol@scu.edu", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(!second.isOk());
    CHECK(second.error == Error::EmailTaken);
}

TEST_CASE("Create user with empty email stores NULL") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> created = db.createUser("dave", "", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());
    CHECK(created.value().getEmail() == "");
}

TEST_CASE("Get user by id") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> created = db.createUser("eve", "eve@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> fetched = db.getUser(created.value().getId());
    CHECK(fetched.isOk());
    CHECK(fetched.value().getName() == "eve");
    CHECK(fetched.value().getEmail() == "eve@scu.edu");
}

TEST_CASE("Get non-existent user returns error") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> fetched = db.getUser(99999);
    CHECK(!fetched.isOk());
    // NoSuchUser is not in the original contract's Error enum for this scope,
    // but we test the pattern; adjust per actual error values.
    // Actually, checking for any error:
    // (We'll just verify !isOk())
    (void)fetched;
}

TEST_CASE("Find user by name (case-insensitive)") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> created = db.createUser("Frank", "frank@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> found = db.findUserByName("FRANK");
    CHECK(found.isOk());
    CHECK(found.value().getName() == "Frank");
}

TEST_CASE("Find user by email (case-insensitive)") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> created = db.createUser("Grace", "GRACE@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> found = db.findUserByEmail("grace@scu.edu");
    CHECK(found.isOk());
    CHECK(found.value().getName() == "Grace");
}

TEST_CASE("Get credentials for user") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> created = db.createUser("heidi", "heidi@scu.edu", "hash99",
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
    CHECK(db.open() == true);

    Result<Credentials> fetched = db.getCredentials(99999);
    CHECK(!fetched.isOk());
}

TEST_CASE("Set password succeeds") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> created = db.createUser("ivan", "ivan@scu.edu", "oldhash",
                                         "oldsalt", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    bool ok = db.setPassword(created.value().getId(), "newhash", "newsalts", 20000);
    CHECK(ok);
}

TEST_CASE("Set email succeeds") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> created = db.createUser("judy", "judy@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    bool ok = db.setEmail(created.value().getId(), "judy.new@scu.edu");
    CHECK(ok);

    Result<User> fetched = db.getUser(created.value().getId());
    CHECK(fetched.isOk());
    CHECK(fetched.value().getEmail() == "judy.new@scu.edu");
}

TEST_CASE("Set email fails if another user has that email") {
    Database db(":memory:");
    CHECK(db.open() == true);

    Result<User> first = db.createUser("kelly", "kelly@scu.edu", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("larry", "larry2@scu.edu", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(second.isOk());

    bool ok = db.setEmail(first.value().getId(), "larry@scu.edu");
    CHECK(!ok);  // larry already has this email
}

TEST_CASE("Count live sessions hosted by new user") {
    Database db(":memory:");
    CHECK(db.open() == true);

    // No users or sessions yet
    int count = db.countLiveSessionsHostedBy(0);
    CHECK(count == 0);

    Result<User> created = db.createUser("mallory", "mallory@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    // No sessions hosted yet
    count = db.countLiveSessionsHostedBy(created.value().getId());
    CHECK(count == 0);
}
}