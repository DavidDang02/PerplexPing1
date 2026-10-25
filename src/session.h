// MODEL: one lecture session. Knows when it started and ended, and the time
// of every "I'm lost" button press. Plain C++ (no Qt), so it is easy to test.

#pragma once

#include <chrono>
#include <vector>

class Session {
public:
    using Clock = std::chrono::system_clock;

    // Starts the session and saves the start time.
    Session();

    // Saves the end time. After this, button presses are no longer recorded.
    void endSession();

    // Records the time the button was pressed (ignored once the session ended).
    void buttonClicked();

    bool isEnded() const;
    int getPressCount() const;
    Clock::time_point getStartTime() const;
    Clock::time_point getEndTime() const;  // only meaningful once isEnded()
    const std::vector<Clock::time_point>& getButtonTimestamps() const;

    // Seconds from the start of the session to each button press, in order.
    std::vector<long long> getPressOffsetsSeconds() const;

private:
    Clock::time_point startTime;
    Clock::time_point endTime;
    std::vector<Clock::time_point> buttonTimestamps;
    bool ended = false;
};
