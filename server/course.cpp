#include "course.h"

#include <vector>

Course::Course() : id(0) {}

Course::Course(int id, std::string name) : id(id), name(std::move(name)) {}

int Course::getId() const {
    return this->id;
}

void Course::setId(int id) {
    this->id = id;
}

const std::string& Course::getName() const {
    return this->name;
}

void Course::setName(const std::string& name) {
    this->name = name;
}

const std::string& Course::getJoinCode() const {
    return this->joinCode;
}

void Course::setJoinCode(const std::string& code) {
    this->joinCode = code;
}

bool Course::isAnonymous() const {
    return this->anonymous;
}

void Course::setAnonymous(bool anonymous) {
    this->anonymous = anonymous;
}

bool Course::areQuestionsShared() const {
    return this->questionsShared;
}

void Course::setQuestionsShared(bool shared) {
    this->questionsShared = shared;
}

int Course::getEducatorId() const {
    return this->educatorId;
}

void Course::setEducatorId(int userId) {
    this->educatorId = userId;
}

bool Course::isEducator(int userId) const {
    // 0 means "nobody" (a deleted account), never the educator.
    return userId != 0 && this->educatorId == userId;
}

const std::vector<int>& Course::getStudentIds() const {
    return this->studentIds;
}

void Course::setStudentIds(std::vector<int> ids) {
    this->studentIds = std::move(ids);
}

const std::vector<int>& Course::getSessionIds() const {
    return this->sessionIds;
}

void Course::setSessionIds(std::vector<int> ids) {
    this->sessionIds = std::move(ids);
}

CourseRole Course::getRole() const {
    return this->role;
}

void Course::setRole(CourseRole role) {
    this->role = role;
}

int Course::getLiveSessionId() const {
    return this->liveSessionId;
}

void Course::setLiveSessionId(int sessionId) {
    this->liveSessionId = sessionId;
}