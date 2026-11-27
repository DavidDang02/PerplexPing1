#include "answer.h"

Answer::Answer() : id(0), questionId(0), time(std::chrono::system_clock::time_point{}) {}

Answer::Answer(int id, int questionId, std::chrono::system_clock::time_point time,
               std::string text)
    : id(id), questionId(questionId), time(time), text(std::move(text)) {}

int Answer::getId() const { return this->id; }
void Answer::setId(int id) { this->id = id; }

int Answer::getQuestionId() const { return this->questionId; }
std::chrono::system_clock::time_point Answer::getTime() const { return this->time; }
const std::string& Answer::getText() const { return this->text; }

void Answer::editText(const std::string& text,
                      std::chrono::system_clock::time_point when) {
    this->text = text;
    this->editedTime = when;
}

bool Answer::isEdited() const {
    return this->editedTime.has_value();
}

std::optional<std::chrono::system_clock::time_point> Answer::getEditedTime() const {
    return this->editedTime;
}

void Answer::setEditedTime(
    std::optional<std::chrono::system_clock::time_point> when) {
    this->editedTime = std::move(when);
}

int Answer::getAuthorId() const { return this->authorId; }
const std::string& Answer::getAuthorName() const { return this->authorName; }
AuthorState Answer::getAuthorState() const { return this->authorState; }

void Answer::setAuthor(int userId, const std::string& userName) {
    this->authorId = userId;
    this->authorName = userName;
    this->authorState = AuthorState::Known;
}

void Answer::hideAuthor() {
    this->authorState = AuthorState::Hidden;
}

void Answer::setAuthorDeleted() {
    this->authorState = AuthorState::Deleted;
    this->authorId = 0;
    this->authorName = "";
}

bool Answer::hasAuthor() const {
    return this->authorState == AuthorState::Known;
}

bool Answer::isBy(int userId) const {
    return this->authorState == AuthorState::Known && this->authorId == userId;
}

bool Answer::isByEducator() const {
    return this->byEducator;
}

void Answer::setByEducator(bool byEducator) {
    this->byEducator = byEducator;
}

bool Answer::isShownBefore(const Answer& a, const Answer& b) {
    if (a.isByEducator() != b.isByEducator()) {
        return a.isByEducator();
    }
    if (a.getTime() != b.getTime()) {
        return a.getTime() < b.getTime();
    }
    return a.getId() < b.getId();
}