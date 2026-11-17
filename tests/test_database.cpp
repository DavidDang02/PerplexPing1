#include "doctest.h"
#include "database.h"
#include <QFile>
#include <QDateTime>
#include <QSqlQuery>
#include <QVariant>
#include "user.h"

TEST_CASE("Check Database Object Creation") {

	Database db("test.db");
	CHECK(db.open() == true);

}


TEST_CASE("Check Database User Store & Retrevial") {

	QFile::remove("test.db");  // start fresh, or "Crawford" is already taken on a second run
	Database db("test.db");
	db.open();
	Result<User> user = db.registerUser("Crawford", "Crawford2!");
	Result<User> test = db.logIn("Crawford", "Crawford2!");
	CHECK(user.isOk());
	CHECK(test.isOk());
	if (test.isOk()) {
		CHECK(user.value.getName() == test.value.getName());
	}
}

TEST_CASE("Check Password Mismatch") {
	
	Database db("test.db");
	db.open();
	Result<User> test = db.logIn("Crawford", "Crawford!");
	CHECK_FALSE(test.isOk());
	CHECK(test.error == Error::WrongLogin);

}

TEST_CASE("Check Username not found") {

	Database db("test.db");
	db.open();
	Result<User> test = db.logIn("Herbert", "Crawford2!");
	CHECK_FALSE(test.isOk());
	CHECK(test.error == Error::WrongLogin);
}

TEST_CASE("Check Session Creation") {

	Database db("test.db");
	db.open();
	Result<User> user = db.logIn("Crawford", "Crawford2!");
	REQUIRE(user.isOk());
	Result<Session> s = db.createSession(user.value.getId());
	CHECK(s.isOk());
	CHECK(s.value.getId() > 0);

}


TEST_CASE("Check Save Input") {

	
	Database db("test.db");
	db.open();
	Result<User> user = db.logIn("Crawford", "Crawford2!");
	REQUIRE(user.isOk());
	Result<Session> s = db.createSession(user.value.getId());
	REQUIRE(s.isOk());
	REQUIRE(s.value.lostButtonClicked());
	std::vector<SessionInput> inputs = s.value.getInputs();
	SessionInput input = inputs[0];
	Result<SessionInput> saved = db.saveInput(s.value.getId(), user.value.getId(), input);
	CHECK(saved.isOk());
	CHECK(saved.value.getId() > 0);  // the database gave it an id

}

TEST_CASE("Check loadSession") {

	Database db("test.db");
	db.open();
	Result<User> user = db.logIn("Crawford", "Crawford2!");
	REQUIRE(user.isOk());
	Result<Session> session = db.createSession(user.value.getId());
	REQUIRE(session.isOk());
	session.value.lostButtonClicked();
	session.value.lostButtonClicked();
	session.value.lostButtonClicked();
	session.value.lostButtonClicked();
	session.value.endSession();
	Result<bool> endResult = db.saveSessionEnd(session.value.getId(), session.value.getEndTime());
	CHECK(endResult.isOk());
	std::vector<SessionInput> inputs = session.value.getInputs();
	for (const SessionInput& input : inputs) {
		db.saveInput(session.value.getId(), user.value.getId(), input);
	}

	Result<Session> test_session = db.loadSession(session.value.getId());
	REQUIRE(test_session.isOk());
	std::vector<SessionInput> test_inputs = test_session.value.getInputs();
	CHECK(test_inputs.size() == inputs.size());


}



TEST_CASE("Times are stored in UTC and load back exactly, in and out of daylight saving") {
	Database db(":memory:");
	REQUIRE(db.open());
	Result<User> user = db.registerUser("ana", "pw");
	REQUIRE(user.isOk());
	Result<Session> sess = db.createSession(user.value.getId());
	REQUIRE(sess.isOk());
	Session& session = sess.value;

	using namespace std::chrono;
	system_clock::time_point summer = system_clock::from_time_t(
	    QDateTime::fromString(QStringLiteral("2026-07-04T18:30:15Z"), Qt::ISODate)
	        .toSecsSinceEpoch()
	);
	system_clock::time_point winter = system_clock::from_time_t(
	    QDateTime::fromString(QStringLiteral("2026-12-02T18:30:00Z"), Qt::ISODate)
	        .toSecsSinceEpoch()
	);
	SessionInput lost(InputType::Lost, summer);
	SessionInput question(InputType::Question, winter, "what is a foreign key?");
	REQUIRE(db.saveInput(session.getId(), user.value.getId(), lost).isOk());
	REQUIRE(db.saveInput(session.getId(), user.value.getId(), question).isOk());
	CHECK(db.saveSessionEnd(session.getId(), winter).isOk());

	QSqlQuery raw("SELECT input_time FROM Sessioninputs ORDER BY input_id;");
	REQUIRE(raw.next());
	CHECK(raw.value(0).toString() == "2026-07-04T18:30:15Z");

	Result<Session> loaded = db.loadSession(session.getId());
	REQUIRE(loaded.isOk());
	REQUIRE(loaded.value.getInputs().size() == 2);
	CHECK(loaded.value.getInputs()[0].getTime() == summer);
	CHECK(loaded.value.getInputs()[1].getTime() == winter);
	CHECK(loaded.value.getEndTime() == winter);
}

TEST_CASE("loadSession fails for a session id that doesn't exist") {
	Database db(":memory:");
	REQUIRE(db.open());
	CHECK_FALSE(db.loadSession(12345).isOk());
}

TEST_CASE("A session with no inputs loads with no inputs") {
	Database db(":memory:");
	REQUIRE(db.open());
	Result<User> user = db.registerUser("ana", "pw");
	REQUIRE(user.isOk());
	Result<Session> running = db.createSession(user.value.getId());
	REQUIRE(running.isOk());

	Result<Session> loaded = db.loadSession(running.value.getId());
	REQUIRE(loaded.isOk());
	CHECK(loaded.value.getInputs().empty());
	CHECK_FALSE(loaded.value.isEnded());  // never ended, so no end time yet

	CHECK(db.saveSessionEnd(running.value.getId(), running.value.getStartTime()).isOk());
	loaded = db.loadSession(running.value.getId());
	REQUIRE(loaded.isOk());
	CHECK(loaded.value.getInputs().empty());
	CHECK(loaded.value.isEnded());
}
