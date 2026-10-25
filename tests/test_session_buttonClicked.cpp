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

// stress case: the "I'm lost" button is pressed 10,000 times before the
// session ends, and every press is still recorded in order
TEST_CASE("Session records a large number of button presses") {
    const int presses = 10000;

    Session s;
    for (int i = 0; i < presses; ++i) {
        s.buttonClicked();
    }
    s.endSession();

    CHECK(s.isEnded());
    CHECK(s.pressCount() == presses);
    const auto &t = s.getButtonTimestamps();
    CHECK(s.getStartTime() <= t.front());
    for (int i = 1; i < presses; ++i) {
        REQUIRE(t[i - 1] <= t[i]);
    }
    CHECK(t.back() <= s.getEndTime());
}
