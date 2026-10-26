#include "session.h"

Session::Session() : startTime(Clock::now()) {}

void Session::endSession() {
    if (ended) {
        return;
    }
    endTime = Clock::now();
    ended = true;
}

void Session::buttonClicked() {
    if (ended) {
        return;
    }
    buttonTimestamps.push_back(Clock::now());
}

bool Session::isEnded() const {
    return ended;
}

int Session::getPressCount() const {
    return static_cast<int>(buttonTimestamps.size());
}

Session::Clock::time_point Session::getStartTime() const {
    return startTime;
}

Session::Clock::time_point Session::getEndTime() const {
    return endTime;
}

const std::vector<Session::Clock::time_point>& Session::getButtonTimestamps() const {
    return buttonTimestamps;
}

std::vector<long long> Session::getPressOffsetsSeconds() const {
    std::vector<long long> offsets;
    for (const Clock::time_point &press : buttonTimestamps) {
        offsets.push_back(
            std::chrono::duration_cast<std::chrono::seconds>(press - startTime).count());
    }
    return offsets;
}
