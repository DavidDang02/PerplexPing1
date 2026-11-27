#include "sessionquestion.h"

#include <algorithm>

SessionQuestion::SessionQuestion() : time(std::chrono::system_clock::time_point{}) {}

SessionQuestion::SessionQuestion(std::chrono::system_clock::time_point time,
                                 std::string text)
    : time(time), text(std::move(text)) {}

int SessionQuestion::getId() const { return this->id; }
void SessionQuestion::setId(int id) { this->id = id; }

int SessionQuestion::getSessionId() const { return this->sessionId; }
void SessionQuestion::setSessionId(int sessionId) { this->sessionId = sessionId; }

std::chrono::system_clock::time_point SessionQuestion::getTime() const {
    return this->time;
}

const std::string& SessionQuestion::getText() const { return this->text; }

void SessionQuestion::editText(const std::string& text,
                               std::chrono::system_clock::time_point when) {
    this->text = text;
    this->editedTime = when;
}

bool SessionQuestion::isEdited() const {
    return this->editedTime.has_value();
}

std::optional<std::chrono::system_clock::time_point>
SessionQuestion::getEditedTime() const {
    return this->editedTime;
}

void SessionQuestion::setEditedTime(
    std::optional<std::chrono::system_clock::time_point> when) {
    this->editedTime = std::move(when);
}

int SessionQuestion::getAuthorId() const { return this->authorId; }
const std::string& SessionQuestion::getAuthorName() const { return this->authorName; }
AuthorState SessionQuestion::getAuthorState() const { return this->authorState; }

void SessionQuestion::setAuthor(int userId, const std::string& userName) {
    this->authorId = userId;
    this->authorName = userName;
    this->authorState = AuthorState::Known;
}

void SessionQuestion::hideAuthor() {
    // Forget who it was, not just flag it, so nothing after Visibility can
    // read the name by mistake (header: id 0 and name "" unless Known).
    this->authorState = AuthorState::Hidden;
    this->authorId = 0;
    this->authorName = "";
}

void SessionQuestion::setAuthorDeleted() {
    this->authorState = AuthorState::Deleted;
    this->authorId = 0;
    this->authorName = "";
}

bool SessionQuestion::hasAuthor() const {
    return this->authorState == AuthorState::Known;
}

bool SessionQuestion::isBy(int userId) const {
    return this->authorState == AuthorState::Known && this->authorId == userId;
}

const std::vector<Answer>& SessionQuestion::getAnswers() const {
    return this->answers;
}

void SessionQuestion::setAnswers(std::vector<Answer> answers) {
    std::sort(answers.begin(), answers.end(), Answer::isShownBefore);
    this->answers = std::move(answers);
}

void SessionQuestion::upsertAnswer(const Answer& answer) {
    for (auto& a : this->answers) {
        if (a.getId() == answer.getId()) {
            a = answer;
            return;
        }
    }
    this->answers.push_back(answer);
    std::sort(this->answers.begin(), this->answers.end(), Answer::isShownBefore);
}

bool SessionQuestion::removeAnswer(int answerId) {
    auto it = std::find_if(this->answers.begin(), this->answers.end(),
                           [answerId](const Answer& a) { return a.getId() == answerId; });
    if (it != this->answers.end()) {
        this->answers.erase(it);
        return true;
    }
    return false;
}

const Answer* SessionQuestion::findAnswer(int answerId) const {
    for (const auto& a : this->answers) {
        if (a.getId() == answerId) {
            return &a;
        }
    }
    return nullptr;
}

bool SessionQuestion::hasEducatorAnswer() const {
    for (const auto& a : this->answers) {
        if (a.isByEducator()) {
            return true;
        }
    }
    return false;
}