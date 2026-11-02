// MODEL: one thing the user sent during a session: an "I'm lost" press or a
// typed question, and when it happened. Plain C++ (no Qt).

#pragma once

#include <chrono>
#include <string>

enum class InputType { Lost, Question };

class SessionInput {
public:
    using Clock = std::chrono::system_clock;

    // questionText is only used when type is Question.
    SessionInput(InputType type, Clock::time_point time, std::string questionText = "");

    void setId(int id);
    InputType getType() const;
    bool isQuestion() const;
    Clock::time_point getTime() const;
    const std::string& getQuestionText() const;  // empty for Lost

private:
    int id;
    InputType type;
    Clock::time_point time;
    std::string questionText;
};
