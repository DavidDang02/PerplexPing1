// MODEL: one lecture session. Knows who started it, when it started and
// ended, and every input (lost press or question) in order. Enforces the
// rule that nothing is recorded after the session ends. Plain C++ (no Qt),
// so it is easy to test.

#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "sessioninput.h"

class Session {
public:
    using Clock = std::chrono::system_clock;

    // Starts a session now with id 0 and user 0 (kept so the M1 tests still work).
    Session();

    // Used by Database: a session with a real id, owned by userId.
    Session(int id, int userId, Clock::time_point startTime);

    // Saves the end time (now, or `when` when loading from the database).
    // After this, inputs are no longer recorded. Ending twice does nothing.
    void endSession();
    void endSession(Clock::time_point when);

    // Records an input. Returns false (and records nothing) once ended.
    bool addInput(const SessionInput& input);

    // Shortcuts that call addInput with the current time.
    bool lostButtonClicked();
    bool askQuestion(const std::string& text);  // also false if text is empty

    bool isEnded() const;
    int getId() const;
    int getUserId() const;
    Clock::time_point getStartTime() const;
    Clock::time_point getEndTime() const;  // only meaningful once isEnded()
    const std::vector<SessionInput>& getInputs() const;

    int getPressCount() const;     // Lost inputs only
    int getQuestionCount() const;  // Question inputs only

    // Times of the Lost inputs only, in order.
    std::vector<Clock::time_point> getLostButtonTimestamps() const;

    // Seconds from the start of the session to each Lost input, in order.
    std::vector<long long> getPressOffsetsSeconds() const;

    // Seconds from the start of the session to one input (for "mm:ss in").
    long long getOffsetSeconds(const SessionInput& input) const;

private:
    int id = 0;
    int userId = 0;
    Clock::time_point startTime;
    Clock::time_point endTime = Clock::time_point{};  // nonsense default value
    std::vector<SessionInput> inputs;
    bool ended = false;
};
