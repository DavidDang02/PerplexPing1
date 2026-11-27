#include "session.h"

#include "validation.h"

#include <algorithm>

namespace {
// Time order, then id (session.h: "by time, then id"), so a list stays in
// order however its items arrive (loaders, live pushes out of order).
template <typename Item>
bool isBefore(const Item& a, const Item& b) {
    if (a.getTime() != b.getTime()) {
        return a.getTime() < b.getTime();
    }
    return a.getId() < b.getId();
}

template <typename Item>
void sortByTimeThenId(std::vector<Item>& items) {
    std::stable_sort(items.begin(), items.end(), isBefore<Item>);
}

// Puts one item in its place in an already sorted list (no full re-sort,
// so a lecture with thousands of presses stays fast).
template <typename Item>
void insertInOrder(std::vector<Item>& items, const Item& item) {
    items.insert(std::upper_bound(items.begin(), items.end(), item, isBefore<Item>), item);
}

// Replaces the item with the same id (its time may have changed) or adds it,
// keeping the order.
template <typename Item>
void upsertInOrder(std::vector<Item>& items, const Item& item) {
    const auto same = std::find_if(items.begin(), items.end(),
                                   [&](const Item& existing) { return existing.getId() == item.getId(); });
    if (same != items.end()) {
        items.erase(same);
    }
    insertInOrder(items, item);
}
}  // namespace

Session::Session() : startTime(Clock::now()), status(SessionStatus::Live) {}

Session::Session(int hostId) : id(0), hostId(hostId), startTime(Clock::now()), status(SessionStatus::Live) {}

Session::Session(int id, int hostId) : id(id), hostId(hostId), startTime(Clock::now()), status(SessionStatus::Live) {}

Session::Session(int id, int hostId, Clock::time_point startTime)
    : id(id), hostId(hostId), startTime(startTime), status(SessionStatus::Live) {}

Session::Session(int id, int hostId, Clock::time_point startTime, Clock::time_point endTime)
    : id(id), hostId(hostId), startTime(startTime), endTime(endTime), status(SessionStatus::InReview) {}

int Session::getId() const { return this->id; }
void Session::setId(int id) { this->id = id; }

int Session::getCourseId() const { return this->courseId; }
void Session::setCourseId(int courseId) { this->courseId = courseId; }

int Session::getHostId() const { return this->hostId; }

const std::string& Session::getName() const { return this->name; }
void Session::setName(const std::string& name) { this->name = name; }

const std::string& Session::getJoinCode() const { return this->joinCode; }
void Session::setJoinCode(const std::string& code) { this->joinCode = code; }

Session::Clock::time_point Session::getStartTime() const { return this->startTime; }

Session::Clock::time_point Session::getEndTime() const { return this->endTime; }

// The status says where the session is (S11), even with a zero end time.
bool Session::isEnded() const { return this->status != SessionStatus::Live; }
bool Session::isLive() const { return !this->isEnded(); }

SessionStatus Session::getStatus() const { return this->status; }
bool Session::isInReview() const { return this->status == SessionStatus::InReview; }
bool Session::isPublished() const { return this->status == SessionStatus::Published; }

bool Session::publish(Clock::time_point when) {
    if (this->status == SessionStatus::Live || this->status == SessionStatus::Published) {
        return false;
    }
    this->publishedTime = when;
    this->status = SessionStatus::Published;
    return true;
}

Session::Clock::time_point Session::getPublishedTime() const { return this->publishedTime; }

void Session::endSession() {
    if (this->status == SessionStatus::Live) {
        this->endTime = Clock::now();
        this->status = SessionStatus::InReview;
    }
}

void Session::endSession(Clock::time_point when) {
    if (this->status == SessionStatus::Live) {
        this->endTime = when;
        this->status = SessionStatus::InReview;
    }
}

long long Session::getDurationSeconds(Clock::time_point now) const {
    if (this->isEnded()) {
        return std::chrono::duration_cast<std::chrono::seconds>(
            this->endTime - this->startTime).count();
    }
    return std::chrono::duration_cast<std::chrono::seconds>(
        now - this->startTime).count();
}

const Recording* Session::getRecording() const {
    return this->recording ? &this->recording.value() : nullptr;
}

void Session::setRecording(const Recording& recording) {
    this->recording = recording;
}

void Session::clearRecording() {
    this->recording.reset();
}

bool Session::addLostPing(const LostPing& ping) {
    if (!this->isLive()) {
        return false;
    }
    insertInOrder(this->lostPings, ping);
    return true;
}

bool Session::addQuestion(const SessionQuestion& question) {
    if (!this->isLive()) {
        return false;
    }
    insertInOrder(this->questions, question);
    return true;
}

bool Session::lostButtonClicked() {
    return this->addLostPing(LostPing(Clock::now()));
}

bool Session::askQuestion(const std::string& text) {
    // I3: same check as AppModel (trimmed, 1-500 characters).
    const std::string trimmed = Validation::trim(text);
    if (Validation::checkQuestion(trimmed) != Error::None) {
        return false;
    }
    return this->addQuestion(SessionQuestion(Clock::now(), trimmed));
}


void Session::setLostPings(std::vector<LostPing> pings) {
    this->lostPings = std::move(pings);
    sortByTimeThenId(this->lostPings);
}

void Session::setQuestions(std::vector<SessionQuestion> questions) {
    this->questions = std::move(questions);
    sortByTimeThenId(this->questions);
}

void Session::upsertLostPing(const LostPing& ping) {
    upsertInOrder(this->lostPings, ping);
}

bool Session::removeLostPing(int pingId) {
    auto it = std::find_if(this->lostPings.begin(), this->lostPings.end(),
                           [pingId](const LostPing& p) { return p.getId() == pingId; });
    if (it != this->lostPings.end()) {
        this->lostPings.erase(it);
        return true;
    }
    return false;
}

void Session::upsertQuestion(const SessionQuestion& question) {
    upsertInOrder(this->questions, question);
}

bool Session::removeQuestion(int questionId) {
    auto it = std::find_if(this->questions.begin(), this->questions.end(),
                           [questionId](const SessionQuestion& q) { return q.getId() == questionId; });
    if (it != this->questions.end()) {
        this->questions.erase(it);
        return true;
    }
    return false;
}

const std::vector<LostPing>& Session::getLostPings() const {
    return this->lostPings;
}

const std::vector<SessionQuestion>& Session::getQuestions() const {
    return this->questions;
}

const LostPing* Session::findLostPing(int pingId) const {
    for (const auto& p : this->lostPings) {
        if (p.getId() == pingId) {
            return &p;
        }
    }
    return nullptr;
}

const SessionQuestion* Session::findQuestion(int questionId) const {
    for (const auto& q : this->questions) {
        if (q.getId() == questionId) {
            return &q;
        }
    }
    return nullptr;
}

SessionQuestion* Session::findQuestionMutable(int questionId) {
    for (auto& q : this->questions) {
        if (q.getId() == questionId) {
            return &q;
        }
    }
    return nullptr;
}

int Session::getPressCount() const {
    return this->lostPings.size();
}

int Session::getQuestionCount() const {
    return this->questions.size();
}

std::vector<Session::Clock::time_point> Session::getLostButtonTimestamps() const {
    std::vector<Clock::time_point> times;
    for (const auto& p : this->lostPings) {
        times.push_back(p.getTime());
    }
    return times;
}

std::vector<long long> Session::getPressOffsetsSeconds() const {
    std::vector<long long> offsets;
    for (const auto& p : this->lostPings) {
        offsets.push_back(this->getOffsetSeconds(p.getTime()));
    }
    return offsets;
}

long long Session::getOffsetSeconds(Clock::time_point time) const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        time - this->startTime).count();
}

