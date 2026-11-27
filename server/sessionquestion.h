// MODEL: one question a student typed during a session: its text, when it
// was asked (and last edited), who asked it (if the viewer may know), and
// the answers it got after the session. Plain C++ (no Qt).
//
// Split from the M2 SessionInput (Nov 11 team decision): only questions have
// text, edits and answers, so an Answer can only ever point at a question.
//
// Owner: Model layer.

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "answer.h"
#include "user.h"

class SessionQuestion {
public:
    using Clock = std::chrono::system_clock;

    SessionQuestion();  // id 0, time 0, empty text
    SessionQuestion(Clock::time_point time, std::string text);

    // Database id, 0 until it is saved.
    int getId() const;
    void setId(int id);
    int getSessionId() const;
    void setSessionId(int sessionId);

    Clock::time_point getTime() const;  // never changes, even when edited (I4)
    const std::string& getText() const;

    // Replaces the text and remembers when (I4). No rule check here:
    // AppModel checks Validation and "live + yours" first.
    void editText(const std::string& text, Clock::time_point when);
    bool isEdited() const;
    std::optional<Clock::time_point> getEditedTime() const;
    void setEditedTime(std::optional<Clock::time_point> when);  // for loaders

    // Who asked it. Same meaning as in LostPing.
    int getAuthorId() const;
    const std::string& getAuthorName() const;
    AuthorState getAuthorState() const;
    void setAuthor(int userId, const std::string& userName);  // -> Known
    void hideAuthor();        // -> Hidden
    void setAuthorDeleted();  // -> Deleted
    bool hasAuthor() const;
    bool isBy(int userId) const;

    // ---- Answers ----
    // Always in display order (A5): educator answers first, then oldest first.
    const std::vector<Answer>& getAnswers() const;
    void setAnswers(std::vector<Answer> answers);  // sorts them
    // Adds the answer, or replaces the one with the same id; keeps the order.
    void upsertAnswer(const Answer& answer);
    bool removeAnswer(int answerId);  // false if there is no such answer
    const Answer* findAnswer(int answerId) const;  // nullptr if none
    bool hasEducatorAnswer() const;

private:
    int id = 0;
    int sessionId = 0;
    Clock::time_point time;
    std::string text;
    std::optional<Clock::time_point> editedTime;
    int authorId = 0;
    std::string authorName;
    AuthorState authorState = AuthorState::Hidden;
    std::vector<Answer> answers;
};
