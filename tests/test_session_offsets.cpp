// Rahul Datta
// Tests that pressOffsetsSeconds() gives one offset per press, measured from the session start.
#include "doctest.h"

#include <chrono>
#include <thread>

#include "session.h"

// normal case: presses about one second apart get offsets of 0, 1 and 2 seconds
TEST_CASE("pressOffsetsSeconds gives one offset per press from the start") {
    Session session;
    session.buttonClicked();
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    session.buttonClicked();
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    session.buttonClicked();

    const std::vector<long long> offsets = session.pressOffsetsSeconds();
    REQUIRE(offsets.size() == 3);
    CHECK(offsets[0] == 0);
    CHECK(offsets[1] == 1);
    CHECK(offsets[2] == 2);
}

// wrong case: a later press never gets a smaller offset than an earlier one.
// Without the sleep, both presses land in the same second and both offsets are
// 0, so the check below could never fail.
TEST_CASE("pressOffsetsSeconds never goes backwards") {
    Session session;
    session.buttonClicked();
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    session.buttonClicked();

    const std::vector<long long> offsets = session.pressOffsetsSeconds();
    REQUIRE(offsets.size() == 2);
    CHECK(offsets[0] == 0);
    CHECK(offsets[1] > offsets[0]);
}

// edge case: a session with no presses has no offsets
TEST_CASE("pressOffsetsSeconds is empty when there are no presses") {
    Session session;
    session.endSession();

    CHECK(session.pressOffsetsSeconds().empty());
}
