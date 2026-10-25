#include "doctest.h"

#include "session.h"

// normal case: 3 presses during a running session are recorded in order
// Made by David
TEST_CASE("Session properly records 3 button presses during a session") {
    Session s;
    s.buttonClicked();
    s.buttonClicked();
    s.buttonClicked();
    s.endSession();

    CHECK(s.pressCount() == 3);
    CHECK(s.isEnded());
    const auto &t = s.getButtonTimestamps();
    CHECK(s.getStartTime() <= t.front());
    CHECK(t[0] <= t[1]);
    CHECK(t[1] <= t[2]);
    CHECK(t.back() <= s.getEndTime());
}
