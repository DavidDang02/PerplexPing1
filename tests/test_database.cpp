#include "doctest.h"
#include "database.h"
#include <QSqlQuery>
#include <QVariant>
#include "user.h"

// Every test opens its own empty in-memory database (":memory:"), so no test
// depends on another one having run first, and each passes when run alone
// (e.g. ./test_build -tc="Check Save Input"). M2 grading note: the old tests
// shared test.db and failed when run on their own.

// Opens a fresh empty database for one test.
static void openFresh(Database& db) {
	REQUIRE(db.open());
}

TEST_CASE("Check Database Object Creation") {
	Database db(":memory:");
	CHECK(db.open() == true);
}

TEST_CASE("Check Database User Store & Retrevial") {
	Database db(":memory:");
	openFresh(db);
	std::optional<User> user = db.registerUser("Crawford", "Crawford2!");
	std::optional<User> test = db.login("Crawford", "Crawford2!");
	REQUIRE(user.has_value());
	REQUIRE(test.has_value());
	CHECK(user->getName() == test->getName());
	CHECK(user->getId() == test->getId());
}

TEST_CASE("Check Password Mismatch") {
	Database db(":memory:");
	openFresh(db);
	// The user must exist, so this fails because of the password and not
	// because the name is unknown (M2 grading note).
	REQUIRE(db.registerUser("Crawford", "Crawford2!").has_value());
	CHECK_FALSE(db.login("Crawford", "Crawford!").has_value());
	CHECK(db.login("Crawford", "Crawford2!").has_value());  // the right one still works
}

TEST_CASE("Check Username not found") {
	Database db(":memory:");
	openFresh(db);
	REQUIRE(db.registerUser("Crawford", "Crawford2!").has_value());
	CHECK_FALSE(db.login("Herbert", "Crawford2!").has_value());
}

TEST_CASE("Check Session Creation") {
	Database db(":memory:");
	openFresh(db);
	std::optional<User> user = db.registerUser("Crawford", "Crawford2!");
	REQUIRE(user.has_value());
	Session s = db.createSession(user->getId());
	CHECK(s.getId() > 0);
	CHECK(s.getUserId() == user->getId());
}

TEST_CASE("Check Save Input") {
	Database db(":memory:");
	openFresh(db);
	std::optional<User> user = db.registerUser("Crawford", "Crawford2!");
	REQUIRE(user.has_value());
	Session s = db.createSession(user->getId());
	REQUIRE(s.lostButtonClicked());
	SessionInput input = s.getInputs()[0];
	db.saveInput(s.getId(), user->getId(), input);
	CHECK(input.getId() > 0);  // the database gave it an id

	std::optional<Session> loaded = db.loadSession(s.getId());
	REQUIRE(loaded.has_value());
	CHECK(loaded->getInputs().size() == 1);
}

TEST_CASE("Check loadSession") {
	Database db(":memory:");
	openFresh(db);
	std::optional<User> user = db.registerUser("Crawford", "Crawford2!");
	REQUIRE(user.has_value());
	Session session = db.createSession(user->getId());
	for (int i = 0; i < 4; i++) {
		REQUIRE(session.lostButtonClicked());
	}
	session.endSession();
	db.saveSessionEnd(session.getId(), session.getEndTime());
	std::vector<SessionInput> inputs = session.getInputs();
	for (SessionInput& input : inputs) {
		db.saveInput(session.getId(), user->getId(), input);
	}

	std::optional<Session> loaded = db.loadSession(session.getId());
	REQUIRE(loaded.has_value());
	CHECK(loaded->getInputs().size() == inputs.size());
	CHECK(loaded->isEnded());
}



TEST_CASE("Times are stored in UTC and load back exactly, in and out of daylight saving") {
	Database db(":memory:");
	REQUIRE(db.open());
	std::optional<User> user = db.registerUser("ana", "pw");
	REQUIRE(user.has_value());
	Session session = db.createSession(user->getId());

	using namespace std::chrono;
	system_clock::time_point summer = system_clock::from_time_t(1783189815);  // 2026-07-04 18:30:15 UTC
	system_clock::time_point winter = system_clock::from_time_t(1796236200);  // 2026-12-02 18:30:00 UTC
	SessionInput lost(InputType::Lost, summer);
	SessionInput question(InputType::Question, winter, "what is a foreign key?");
	db.saveInput(session.getId(), user->getId(), lost);
	db.saveInput(session.getId(), user->getId(), question);
	db.saveSessionEnd(session.getId(), winter);

	QSqlQuery raw("SELECT input_time FROM Sessioninputs ORDER BY input_id;");
	REQUIRE(raw.next());
	CHECK(raw.value(0).toString() == "2026-07-04T18:30:15Z");

	std::optional<Session> loaded = db.loadSession(session.getId());
	REQUIRE(loaded.has_value());
	REQUIRE(loaded->getInputs().size() == 2);
	CHECK(loaded->getInputs()[0].getTime() == summer);
	CHECK(loaded->getInputs()[1].getTime() == winter);
	CHECK(loaded->getEndTime() == winter);
}

TEST_CASE("loadSession returns nothing for a session id that doesn't exist") {
	Database db(":memory:");
	REQUIRE(db.open());
	CHECK_FALSE(db.loadSession(12345).has_value());
}

TEST_CASE("A session with no inputs loads with no inputs") {
	Database db(":memory:");
	REQUIRE(db.open());
	std::optional<User> user = db.registerUser("ana", "pw");
	REQUIRE(user.has_value());
	Session running = db.createSession(user->getId());

	std::optional<Session> loaded = db.loadSession(running.getId());
	REQUIRE(loaded.has_value());
	CHECK(loaded->getInputs().empty());
	CHECK_FALSE(loaded->isEnded());  // never ended, so no end time yet

	db.saveSessionEnd(running.getId(), running.getStartTime());
	loaded = db.loadSession(running.getId());
	REQUIRE(loaded.has_value());
	CHECK(loaded->getInputs().empty());
	CHECK(loaded->isEnded());
}
