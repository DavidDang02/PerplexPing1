#include "doctest.h"
#include "session.h"

#include <algorithm>
#include <chrono>
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
    CHECK(std::is_sorted(t.begin(), t.end()));
    CHECK(t.back() <= s.getEndTime());
}

// Presses that arrive out of time order (e.g. live pushes) still end up in
// time order, ties broken by id, so the timeline never jumps backwards.
TEST_CASE("Session keeps presses in time order whatever order they arrive in") {
    using std::chrono::seconds;
    const Session::Clock::time_point t0(seconds(1700000000));
    Session s(1, 1, t0);
    LostPing late(t0 + seconds(30));
    late.setId(3);
    LostPing early(t0 + seconds(10));
    early.setId(2);
    LostPing tie(t0 + seconds(10));
    tie.setId(1);
    REQUIRE(s.addLostPing(late));
    REQUIRE(s.addLostPing(early));
    REQUIRE(s.addLostPing(tie));
    const auto& pings = s.getLostPings();
    REQUIRE(pings.size() == 3);
    CHECK(pings[0].getId() == 1);
    CHECK(pings[1].getId() == 2);
    CHECK(pings[2].getId() == 3);
    // A replaced ping whose time changed moves to its new place.
    LostPing moved(t0 + seconds(40));
    moved.setId(1);
    s.upsertLostPing(moved);
    CHECK(s.getLostPings().back().getId() == 1);
}
