//Crawford Herbert
#include "doctest.h"
#include "session.h"

TEST_CASE("Button presses don't record after session ends") {

	Session s;
	s.buttonClicked();
	s.buttonClicked();
	s.endSession();
	s.buttonClicked();

	CHECK(s.isEnded());
	CHECK(s.getPressCount() == 2);
	CHECK(s.getButtonTimestamps().size() == 2);

}


TEST_CASE("Button presses do not record after session ends even if no presses were recorded during the session") {

	Session s;
	s.endSession();
	s.buttonClicked();

	CHECK(s.isEnded());
	CHECK(s.getPressCount() == 0);
	CHECK(s.getButtonTimestamps().size() == 0);

}
