// Matthew Erdman
// Unit tests for starting and ending sessions

#include "doctest.h"

#include <chrono>
#include <thread>

#include "session.h"

// Normal case: a new Session has isEnded() == false, getPressCount() == 0, and an empty timestamp list
TEST_CASE("New session initializes in non-ended state with zero presses") {
    Session session;

    CHECK(session.isEnded() == false);
    CHECK(session.getPressCount() == 0);
    CHECK(session.getButtonTimestamps().empty());
}

// Wrong case: calling endSession() twice preserves the initial end time
TEST_CASE("Calling endSession twice keeps the first end time") {
    Session session;
    session.endSession();

    const Session::Clock::time_point firstEndTime = session.getEndTime();

    // Wait ~10 ms so that any unintended second recording would produce a different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    session.endSession();

    CHECK(session.isEnded() == true);
    CHECK(session.getEndTime() == firstEndTime);
}

// Edge case: ending a session right away marks it ended with start time <= end time
TEST_CASE("Starting and ending a session right away preserves time order") {
    Session session;
    session.endSession();

    CHECK(session.isEnded() == true);
    CHECK(session.getStartTime() <= session.getEndTime());
}