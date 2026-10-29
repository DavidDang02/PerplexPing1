//Crawford Herbert
#include "doctest.h"
#include "session.h"

TEST_CASE("Button presses don't record after session ends") {

	Session s;
	s.lostButtonClicked();
	s.lostButtonClicked();
	s.endSession();
	s.lostButtonClicked();

	CHECK(s.isEnded());
	CHECK(s.getPressCount() == 2);
	CHECK(s.getLostButtonTimestamps().size() == 2);

}


TEST_CASE("Button presses do not record after session ends even if no presses were recorded during the session") {

	Session s;
	s.endSession();
	s.lostButtonClicked();

	CHECK(s.isEnded());
	CHECK(s.getPressCount() == 0);
	CHECK(s.getLostButtonTimestamps().size() == 0);

}
