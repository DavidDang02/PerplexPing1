// Rahul Datta
// Tests for AppModel, using a throwaway in-memory database.

#include "doctest.h"
#include "appmodel.h"
#include "database.h"

TEST_CASE("AppModel: nothing works until a user is logged in") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    CHECK_FALSE(model.isLoggedIn());
    CHECK(model.getCurrentUser() == nullptr);
    CHECK_FALSE(model.startSession());
    CHECK_FALSE(model.clickLostButton());
    CHECK(model.getPastSessions().empty());
}

TEST_CASE("AppModel: register logs in, and login only works with the right password") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    REQUIRE(model.registerUser("ana", "pw") == AuthError::None);
    CHECK(model.isLoggedIn());
    CHECK(model.getCurrentUser()->getName() == "ana");

    model.logOut();
    CHECK_FALSE(model.isLoggedIn());
    CHECK(model.logIn("ana", "wrong") == AuthError::WrongLogin);
    CHECK_FALSE(model.isLoggedIn());
    CHECK(model.logIn("ana", "pw") == AuthError::None);
}

TEST_CASE("AppModel: login and register say why they failed") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    CHECK(model.registerUser("", "pw") == AuthError::EmptyName);
    CHECK(model.registerUser("   ", "pw") == AuthError::EmptyName);  // only spaces
    CHECK(model.registerUser("ana", "") == AuthError::EmptyPassword);
    CHECK(model.logIn("", "pw") == AuthError::EmptyName);
    CHECK(model.logIn("ana", "") == AuthError::EmptyPassword);
    CHECK(model.logIn("nobody", "pw") == AuthError::WrongLogin);  // same as a wrong password
    CHECK_FALSE(model.isLoggedIn());

    REQUIRE(model.registerUser("  ana ", "pw") == AuthError::None);  // name is trimmed
    CHECK(model.getCurrentUser()->getName() == "ana");
    model.logOut();
    CHECK(model.registerUser("ana", "other") == AuthError::NameTaken);
    CHECK(model.logIn(" ana", "pw") == AuthError::None);
}

TEST_CASE("AppModel: a session's inputs are saved and come back after logging in again") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);
    REQUIRE(model.registerUser("ana", "pw") == AuthError::None);

    REQUIRE(model.startSession());
    CHECK(model.getCurrentSession()->getId() > 0);  // the database gave it an id
    CHECK(model.clickLostButton());
    CHECK(model.clickLostButton());
    CHECK(model.askQuestion("what is a foreign key?"));
    CHECK_FALSE(model.askQuestion(""));  // empty questions are rejected
    model.endSession();
    CHECK_FALSE(model.hasRunningSession());
    CHECK_FALSE(model.clickLostButton());  // nothing to press into after End

    model.logOut();
    REQUIRE(model.logIn("ana", "pw") == AuthError::None);
    std::vector<Session> past = model.getPastSessions();
    REQUIRE(past.size() == 1);
    CHECK(past[0].isEnded());
    CHECK(past[0].getPressCount() == 2);
    CHECK(past[0].getQuestionCount() == 1);
    CHECK(past[0].getInputs().back().getQuestionText() == "what is a foreign key?");
}

TEST_CASE("AppModel: users only see their own sessions, newest first") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    REQUIRE(model.registerUser("ana", "pw") == AuthError::None);
    model.startSession();
    model.endSession();
    model.startSession();
    int newestId = model.getCurrentSession()->getId();
    model.endSession();
    model.logOut();

    REQUIRE(model.registerUser("ben", "pw") == AuthError::None);
    CHECK(model.getPastSessions().empty());
    model.logOut();

    REQUIRE(model.logIn("ana", "pw") == AuthError::None);
    std::vector<Session> past = model.getPastSessions();
    REQUIRE(past.size() == 2);
    CHECK(past[0].getId() == newestId);
}

TEST_CASE("AppModel: a second session can't start while one is running") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    REQUIRE(model.registerUser("ana", "pw") == AuthError::None);
    REQUIRE(model.startSession());
    int firstId = model.getCurrentSession()->getId();
    CHECK_FALSE(model.startSession());
    CHECK(model.getCurrentSession()->getId() == firstId);  // still the first one

    model.endSession();
    CHECK(model.startSession());  // fine once the first one has ended
}
