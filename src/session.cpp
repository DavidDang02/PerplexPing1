#include "session.h"

Session::Session()
    : id(0), userId(0), startTime(Clock::now()), ended(false) {}

Session::Session(int id, int userId, Clock::time_point startTime)
    : id(id), userId(userId), startTime(startTime), ended(false) {}

void Session::endSession() {
    if (ended) {
        return;
    }
    endTime = Clock::now();
    ended = true;
}

void Session::endSession(Clock::time_point when) {
    if (ended) {
        return;
    }
    endTime = when;
    ended = true;
}

bool Session::addInput(const SessionInput& input) {
    if (ended) {
        return false;
    }
    inputs.push_back(input);
    return true;
}

bool Session::lostButtonClicked() {
    return addInput(SessionInput(InputType::Lost, Clock::now(), ""));
}

bool Session::askQuestion(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    return addInput(SessionInput(InputType::Question, Clock::now(), text));
}

bool Session::isEnded() const {
    return ended;
}

int Session::getId() const {
    return id;
}

int Session::getUserId() const {
    return userId;
}

Session::Clock::time_point Session::getStartTime() const {
    return startTime;
}

Session::Clock::time_point Session::getEndTime() const {
    return endTime;
}

const std::vector<SessionInput>& Session::getInputs() const {
    return inputs;
}

int Session::getPressCount() const {
    int count = 0;
    for (const auto& input : inputs) {
        if (input.getType() == InputType::Lost) {
            ++count;
        }
    }
    return count;
}

int Session::getQuestionCount() const {
    int count = 0;
    for (const auto& input : inputs) {
        if (input.isQuestion()) {
            ++count;
        }
    }
    return count;
}

std::vector<Session::Clock::time_point> Session::getLostButtonTimestamps() const {
    std::vector<Clock::time_point> timestamps;
    for (const auto& input : inputs) {
        if (input.getType() == InputType::Lost) {
            timestamps.push_back(input.getTime());
        }
    }
    return timestamps;
}

std::vector<long long> Session::getPressOffsetsSeconds() const {
    std::vector<long long> offsets;
    for (const auto& input : inputs) {
        if (input.getType() == InputType::Lost) {
            offsets.push_back(getOffsetSeconds(input));
        }
    }
    return offsets;
}

long long Session::getOffsetSeconds(const SessionInput& input) const {
    return std::chrono::duration_cast<std::chrono::seconds>(input.getTime() - startTime).count();
}