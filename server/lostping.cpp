#include "lostping.h"

#include <chrono>

LostPing::LostPing() : time(std::chrono::system_clock::time_point{}) {}

LostPing::LostPing(std::chrono::system_clock::time_point time) : time(time) {}

int LostPing::getId() const { return this->id; }
void LostPing::setId(int id) { this->id = id; }

int LostPing::getSessionId() const { return this->sessionId; }
void LostPing::setSessionId(int sessionId) { this->sessionId = sessionId; }

std::chrono::system_clock::time_point LostPing::getTime() const {
    return this->time;
}

int LostPing::getAuthorId() const { return this->authorId; }
const std::string& LostPing::getAuthorName() const { return this->authorName; }
AuthorState LostPing::getAuthorState() const { return this->authorState; }

void LostPing::setAuthor(int userId, const std::string& userName) {
    this->authorId = userId;
    this->authorName = userName;
    this->authorState = AuthorState::Known;
}

void LostPing::hideAuthor() {
    this->authorState = AuthorState::Hidden;
}

void LostPing::setAuthorDeleted() {
    this->authorState = AuthorState::Deleted;
    this->authorId = 0;
    this->authorName = "";
}

bool LostPing::hasAuthor() const {
    return this->authorState == AuthorState::Known;
}

bool LostPing::isBy(int userId) const {
    return this->authorState == AuthorState::Known && this->authorId == userId;
}