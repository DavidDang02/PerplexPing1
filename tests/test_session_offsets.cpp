// Rahul Datta
// Tests that pressOffsetsSeconds() gives one offset per press, measured from the session start.
#include "doctest.h"

#include "session.h"

// normal case: presses right after the start give one offset each, all within the first second
TEST_CASE("pressOffsetsSeconds gives one offset per press from the start") {
    Session session;
    session.buttonClicked();
    session.buttonClicked();
    session.buttonClicked();

    const std::vector<long long> offsets = session.pressOffsetsSeconds();
    REQUIRE(offsets.size() == 3);
    for (long long offset : offsets) {
        CHECK(offset <= 1);
    }
}

// wrong case: offsets never go backwards, each one is at least the one before
TEST_CASE("pressOffsetsSeconds never goes backwards") {
    Session session;
    session.buttonClicked();
    session.buttonClicked();
    session.buttonClicked();

    const std::vector<long long> offsets = session.pressOffsetsSeconds();
    for (size_t i = 1; i < offsets.size(); ++i) {
        CHECK(offsets[i] >= offsets[i - 1]);
    }
}

// edge case: a session with no presses has no offsets
TEST_CASE("pressOffsetsSeconds is empty when there are no presses") {
    Session session;
    session.endSession();

    CHECK(session.pressOffsetsSeconds().empty());
}
