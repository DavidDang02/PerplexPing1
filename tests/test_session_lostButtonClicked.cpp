#include "doctest.h"
#include "session.h"
// Made by David
// Unit test for session.lostButtonClicked()

// normal case: 3 presses during a running session are recorded in order
TEST_CASE("Session properly records 3 button presses during a session") {
    Session s;
    s.lostButtonClicked();
    s.lostButtonClicked();
    s.lostButtonClicked();
    s.endSession();

    CHECK(s.getPressCount() == 3);
    CHECK(s.isEnded());
    const auto &t = s.getLostButtonTimestamps();
    CHECK(s.getStartTime() <= t.front());
    CHECK(t[0] <= t[1]);
    CHECK(t[1] <= t[2]);
    CHECK(t.back() <= s.getEndTime());
}

// stress case: the "I'm lost" button is pressed 10,000 times before the
// session ends, and every press is still recorded in order
TEST_CASE("Session records a large number of button presses") {
    const int presses = 10000;

    Session s;
    for (int i = 0; i < presses; ++i) {
        s.lostButtonClicked();
    }
    s.endSession();

    CHECK(s.isEnded());
    CHECK(s.getPressCount() == presses);
    const auto &t = s.getLostButtonTimestamps();
    CHECK(s.getStartTime() <= t.front());
    for (int i = 1; i < presses; ++i) {
        REQUIRE(t[i - 1] <= t[i]);
    }
    CHECK(t.back() <= s.getEndTime());
}
