// MODEL: one answer to a question, written after the session ended
// (rule A1). A question can have many. Plain C++ (no Qt).
//
// Owner: Model layer.

#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "user.h"

class Answer {
public:
    using Clock = std::chrono::system_clock;

    Answer();  // id 0
    Answer(int id, int questionId, Clock::time_point time, std::string text);

    int getId() const;
    void setId(int id);
    int getQuestionId() const;  // the SessionQuestion it answers
    Clock::time_point getTime() const;
    const std::string& getText() const;

    // Replaces the text and remembers when (A8). No rule check here:
    // Permissions::canEditAnswer decides, AppModel calls.
    void editText(const std::string& text, Clock::time_point when);
    bool isEdited() const;
    std::optional<Clock::time_point> getEditedTime() const;
    void setEditedTime(std::optional<Clock::time_point> when);  // for loaders

    // Who wrote it. Id 0 and name "" unless getAuthorState() is Known.
    // A new Answer starts as Hidden. Same meaning as in SessionQuestion.
    int getAuthorId() const;
    const std::string& getAuthorName() const;
    AuthorState getAuthorState() const;
    void setAuthor(int userId, const std::string& userName);  // -> Known
    void hideAuthor();        // -> Hidden (Visibility: V3)
    void setAuthorDeleted();  // -> Deleted (U8)
    bool hasAuthor() const;   // getAuthorState() == AuthorState::Known
    bool isBy(int userId) const;  // userId != 0 and is the known author

    // True if the author is an educator of the course right now (A6); not
    // stored, worked out by Database when the answer is loaded. False for a
    // deleted author. Educator answers are pinned on top.
    bool isByEducator() const;
    void setByEducator(bool byEducator);

    // The display order (A5): educator answers first; inside each group the
    // oldest first; equal times by id. Use with std::sort / std::stable_sort.
    static bool isShownBefore(const Answer& a, const Answer& b);

private:
    int id = 0;
    int questionId = 0;
    Clock::time_point time;
    std::string text;
    std::optional<Clock::time_point> editedTime;
    int authorId = 0;
    std::string authorName;
    AuthorState authorState = AuthorState::Hidden;
    bool byEducator = false;
};
