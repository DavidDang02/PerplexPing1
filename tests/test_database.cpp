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
	std::optional<User> user = db.registerUser("Crawford", "Crawford2!");
	std::optional<User> test = db.login("Crawford", "Crawford!");
	bool user_status = user.has_value();
	bool test_status = test.has_value();
	CHECK(user_status);
	CHECK(test_status == false);

}

TEST_CASE("Check Username not found"){

	Database db("test.db");
	db.open();
	std::optional<User> user = db.registerUser("Crawford", "Crawford2!");
	std::optional<User> test = db.login("Herbert", "Crawford2!");
	bool user_status = user.has_value();
	bool test_status = test.has_value();
	CHECK(user_status);
	CHECK(test_status == false);
}

TEST_CASE("Check Session Creation"){

	Database db("test.db");
	db.open();
	std::optional<User> user = db.registerUser("Crawford", "Crawford2!");
	Session s = db.createSession(user->getId());
	CHECK(s.getId() > -1);

}
