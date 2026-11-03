#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "database.h"
#include "user.h"
#include <iostream>
//#include <optional>

TEST_CASE("Check Database Object Creation") {

	Database db("test.db");
	CHECK(db.open() == true);

}


TEST_CASE("Check Database User Store & Retrevial") {

	Database db("test.db");
	db.open();
	std::optional<User> user = db.registerUser("Crawford", "Crawford2!");
	std::optional<User> test = db.login("Crawford", "Crawford2!");
	bool user_status = user.has_value();
	bool test_status = test.has_value();
	CHECK(user_status);
	CHECK(test_status);
	if(test_status){
		CHECK(user->getName() == test->getName());
	}
}

TEST_CASE("Check Password Mismatch"){
	
	Database db("test.db");
	db.open();
	std::optional<User> test = db.login("Crawford", "Crawford!");
	bool test_status = test.has_value();
	CHECK(test_status == false);

}

TEST_CASE("Check Username not found"){

	Database db("test.db");
	db.open();
	std::optional<User> test = db.login("Herbert", "Crawford2!");
	bool test_status = test.has_value();
	CHECK(test_status == false);
}

TEST_CASE("Check Session Creation"){

	Database db("test.db");
	db.open();
	std::optional<User> user = db.login("Crawford", "Crawford2!");
	Session s = db.createSession(user->getId());
	CHECK(s.getId() > -1);

}


TEST_CASE("Check Save Input"){

	
	Database db("test.db");
	db.open();
	std::optional<User> user = db.login("Crawford", "Crawford2!");
	if(user.has_value()){
		Session s = db.createSession(user->getId());
		if(s.lostButtonClicked()){
			std::vector<SessionInput> inputs = s.getInputs();
			db.saveInput(s.getId(), user->getId(), inputs[0]);
			CHECK(inputs[0].getId() > -1);
		}
	}

}

TEST_CASE("Check loadSession"){

	Database db("test.db");
	db.open();
	std::optional<User> user = db.login("Crawford", "Crawford2!");
	std::optional<Session> session = db.createSession(user->getId());
	session->lostButtonClicked();
	session->lostButtonClicked();
	session->lostButtonClicked();
	session->lostButtonClicked();
	session->endSession();
	db.saveSessionEnd(session->getId(), session->getEndTime());
	std::vector<SessionInput> inputs = session->getInputs();
	for(int i = 0; i < inputs.size(); i++){
		db.saveInput(session->getId(), user->getId(), inputs[i]);
	}

	std::optional<Session> test_session = db.loadSession(session->getId());
	std::vector<SessionInput> test_inputs = test_session->getInputs();
	CHECK(test_inputs.size() == inputs.size());


}


