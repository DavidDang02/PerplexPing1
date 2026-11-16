// Rahul Datta
// Tests for AppModel, using a throwaway in-memory database.

#include "doctest.h"
#include "appmodel.h"
#include "database.h"

#include <string>

TEST_CASE("AppModel: nothing works until a user is logged in") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    CHECK_FALSE(model.isLoggedIn());
    CHECK(model.getCurrentUser() == nullptr);

    const auto startRes = model.startSession();
    CHECK_FALSE(startRes.isOk());
    CHECK(startRes.error == Error::NotLoggedIn);

    const auto lostRes = model.clickLostButton();
    CHECK_FALSE(lostRes.isOk());
    CHECK(lostRes.error == Error::NotLoggedIn);

    const auto qRes = model.askQuestion("what is this?");
    CHECK_FALSE(qRes.isOk());
    CHECK(qRes.error == Error::NotLoggedIn);

    const auto endRes = model.endSession();
    CHECK_FALSE(endRes.isOk());
    CHECK(endRes.error == Error::NotLoggedIn);

    const auto pastRes = model.getPastSessions();
    CHECK_FALSE(pastRes.isOk());
    CHECK(pastRes.error == Error::NotLoggedIn);
}

TEST_CASE("AppModel: register logs in, and login only works with the right password") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    const auto regRes = model.registerUser("ana", "password1");
    REQUIRE(regRes.isOk());
    CHECK(regRes.error == Error::None);
    CHECK(regRes.value.getId() > 0);
    CHECK(regRes.value.getName() == "ana");
    CHECK(model.isLoggedIn());
    CHECK(model.getCurrentUser()->getName() == "ana");

    model.logOut();
    CHECK_FALSE(model.isLoggedIn());

    const auto badLogin = model.logIn("ana", "wrongpass1");
    CHECK_FALSE(badLogin.isOk());
    CHECK(badLogin.error == Error::WrongLogin);
    CHECK_FALSE(model.isLoggedIn());

    const auto goodLogin = model.logIn("ana", "password1");
    REQUIRE(goodLogin.isOk());
    CHECK(goodLogin.error == Error::None);
    CHECK(goodLogin.value.getName() == "ana");
}

TEST_CASE("AppModel: login and register say why they failed") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    CHECK(model.registerUser("", "password1").error == Error::EmptyName);
    CHECK(model.registerUser("   ", "password1").error == Error::EmptyName);  // only spaces
    CHECK(model.registerUser("ana", "").error == Error::EmptyPassword);
    CHECK(model.logIn("", "password1").error == Error::EmptyName);
    CHECK(model.logIn("ana", "").error == Error::EmptyPassword);
    CHECK(model.logIn("nobody", "password1").error == Error::WrongLogin);  // same as a wrong password
    CHECK_FALSE(model.isLoggedIn());

    const auto trimmedReg = model.registerUser("  ana ", "password1");  // name is trimmed
    REQUIRE(trimmedReg.isOk());
    CHECK(trimmedReg.value.getName() == "ana");
    CHECK(model.getCurrentUser()->getName() == "ana");

    model.logOut();
    const auto dupReg = model.registerUser("ana", "otherpass1");
    CHECK_FALSE(dupReg.isOk());
    CHECK(dupReg.error == Error::NameTaken);

    const auto trimmedLogin = model.logIn(" ana", "password1");
    CHECK(trimmedLogin.isOk());
    CHECK(trimmedLogin.value.getName() == "ana");
}

TEST_CASE("AppModel: new accounts follow the contract's username and password limits") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    // Edges that are allowed: 3 and 20 character names, 8 and 64 character passwords
    REQUIRE(model.registerUser("abc", std::string(8, 'p')).isOk());
    model.logOut();
    const auto longest = model.registerUser(std::string(20, 'u'), std::string(64, 'p'));
    REQUIRE(longest.isOk());
    CHECK(longest.value.getName().size() == 20);
    model.logOut();

    // Spaces around the name are trimmed before checking its length
    const auto padded = model.registerUser("  " + std::string(20, 'v') + "\t\n", "password1");
    REQUIRE(padded.isOk());
    CHECK(padded.value.getName() == std::string(20, 'v'));
    model.logOut();

    // One past each edge fails, and nothing is logged in
    CHECK(model.registerUser("ab", "password1").error == Error::NameTooShort);
    CHECK(model.registerUser(std::string(21, 'a'), "password1").error == Error::NameTooLong);
    CHECK(model.registerUser("ana smith", "password1").error == Error::NameBadCharacters);
    CHECK(model.registerUser("bob", "1234567").error == Error::PasswordTooShort);
    CHECK(model.registerUser("bob", std::string(65, 'b')).error == Error::PasswordTooLong);
    // Passwords are not trimmed: these spaces count
    CHECK(model.registerUser("bob", " " + std::string(63, 'b') + " ").error == Error::PasswordTooLong);
    CHECK_FALSE(model.isLoggedIn());

    // Order of checks: username before password
    CHECK(model.registerUser(std::string(21, 'a'), "x").error == Error::NameTooLong);
}

TEST_CASE("AppModel: usernames ignore case, passwords don't") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    REQUIRE(model.registerUser("Ana", "password1").isOk());
    model.logOut();
    CHECK(model.registerUser("ana", "password2").error == Error::NameTaken);  // same name
    const auto upper = model.logIn("ANA", "password1");
    REQUIRE(upper.isOk());
    CHECK(upper.value.getName() == "Ana");  // shown as registered
    model.logOut();
    CHECK(model.logIn("ana", "PASSWORD1").error == Error::WrongLogin);
}

TEST_CASE("AppModel: questions over 500 characters return QuestionTooLong") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);
    REQUIRE(model.registerUser("ana", "password1").isOk());
    REQUIRE(model.startSession().isOk());

    CHECK(model.askQuestion(std::string(501, 'q')).error == Error::QuestionTooLong);
    CHECK(model.getCurrentSession()->getQuestionCount() == 0);  // nothing saved
    CHECK(model.askQuestion(std::string(500, 'q')).isOk());     // exactly the limit
}

TEST_CASE("AppModel: empty and whitespace questions return EmptyQuestion") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    REQUIRE(model.registerUser("ana", "password1").isOk());
    REQUIRE(model.startSession().isOk());

    const auto emptyRes = model.askQuestion("");
    CHECK_FALSE(emptyRes.isOk());
    CHECK(emptyRes.error == Error::EmptyQuestion);

    const auto spacesRes = model.askQuestion("   \t\r\n  ");
    CHECK_FALSE(spacesRes.isOk());
    CHECK(spacesRes.error == Error::EmptyQuestion);

    // Valid question with surrounding whitespace is trimmed and saved
    const auto okRes = model.askQuestion("  Why does this compile?  ");
    REQUIRE(okRes.isOk());
    CHECK(okRes.value.getType() == InputType::Question);
    CHECK(okRes.value.getQuestionText() == "Why does this compile?");
    CHECK(okRes.value.getId() > 0);
    CHECK(model.getCurrentSession()->getQuestionCount() == 1);
}

TEST_CASE("AppModel: a session's inputs are saved and come back after logging in again") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);
    REQUIRE(model.registerUser("ana", "password1").isOk());

    // Cannot submit inputs or end before a session starts
    CHECK(model.clickLostButton().error == Error::NoSuchSession);
    CHECK(model.askQuestion("hello?").error == Error::NoSuchSession);
    CHECK(model.endSession().error == Error::NoSuchSession);

    const auto startRes = model.startSession();
    REQUIRE(startRes.isOk());
    CHECK(startRes.value.getId() > 0);
    CHECK(model.getCurrentSession()->getId() == startRes.value.getId());

    const auto lost1 = model.clickLostButton();
    REQUIRE(lost1.isOk());
    CHECK(lost1.value.getType() == InputType::Lost);
    CHECK(lost1.value.getId() > 0);

    CHECK(model.clickLostButton().isOk());

    const auto qRes = model.askQuestion("what is a foreign key?");
    REQUIRE(qRes.isOk());
    CHECK(qRes.value.getQuestionText() == "what is a foreign key?");

    const auto emptyQRes = model.askQuestion("");
    CHECK_FALSE(emptyQRes.isOk());
    CHECK(emptyQRes.error == Error::EmptyQuestion);

    const auto endRes = model.endSession();
    REQUIRE(endRes.isOk());
    CHECK(endRes.value.isEnded());
    CHECK_FALSE(model.hasRunningSession());

    const auto postEndLost = model.clickLostButton();
    CHECK_FALSE(postEndLost.isOk());
    CHECK(postEndLost.error == Error::NoSuchSession);

    model.logOut();
    REQUIRE(model.logIn("ana", "password1").isOk());
    const auto pastRes = model.getPastSessions();
    REQUIRE(pastRes.isOk());
    const std::vector<Session>& past = pastRes.value;
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

    REQUIRE(model.registerUser("ana", "password1").isOk());
    REQUIRE(model.startSession().isOk());
    REQUIRE(model.endSession().isOk());

    const auto secondStart = model.startSession();
    REQUIRE(secondStart.isOk());
    const int newestId = secondStart.value.getId();
    REQUIRE(model.endSession().isOk());
    model.logOut();

    REQUIRE(model.registerUser("ben", "password1").isOk());
    const auto benPast = model.getPastSessions();
    REQUIRE(benPast.isOk());
    CHECK(benPast.value.empty());
    model.logOut();

    REQUIRE(model.logIn("ana", "password1").isOk());
    const auto anaPast = model.getPastSessions();
    REQUIRE(anaPast.isOk());
    REQUIRE(anaPast.value.size() == 2);
    CHECK(anaPast.value[0].getId() == newestId);
}

TEST_CASE("AppModel: a second session can't start while one is running") {
    Database db(":memory:");
    REQUIRE(db.open());
    AppModel model(&db);

    REQUIRE(model.registerUser("ana", "password1").isOk());
    const auto first = model.startSession();
    REQUIRE(first.isOk());
    const int firstId = first.value.getId();

    const auto second = model.startSession();
    CHECK_FALSE(second.isOk());
    CHECK(second.error == Error::CourseHasLiveSession);
    CHECK(model.getCurrentSession()->getId() == firstId);  // still the first one

    REQUIRE(model.endSession().isOk());
    CHECK(model.startSession().isOk());  // fine once the first one has ended
}