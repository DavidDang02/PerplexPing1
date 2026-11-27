// MODEL: one "I'm lost" press: who pressed it (if the viewer may know),
// in which session, and when. No text, no editing, no answers. Plain C++ (no Qt).
//
// Split from the M2 SessionInput (Nov 11 team decision): a Lost press and a
// question are different things, so each gets its own class and table.
//
// Owner: Model layer.

#pragma once

#include <chrono>
#include <string>

#include "user.h"

class LostPing {
public:
    using Clock = std::chrono::system_clock;

    LostPing();  // id 0, time 0
    explicit LostPing(Clock::time_point time);

    // Database id, 0 until it is saved.
    int getId() const;
    void setId(int id);
    int getSessionId() const;
    void setSessionId(int sessionId);

    Clock::time_point getTime() const;  // stamped by the server's clock

    // Who pressed it. Id 0 and name "" unless getAuthorState() is Known.
    // A new LostPing starts as Hidden.
    int getAuthorId() const;
    const std::string& getAuthorName() const;
    AuthorState getAuthorState() const;
    void setAuthor(int userId, const std::string& userName);  // -> Known
    void hideAuthor();        // -> Hidden (Visibility: V2, V4)
    void setAuthorDeleted();  // -> Deleted (Database: user_id is NULL, U8)
    bool hasAuthor() const;   // getAuthorState() == AuthorState::Known
    // True if userId is not 0 and is the known author ("is this mine?").
    bool isBy(int userId) const;

private:
    int id = 0;
    int sessionId = 0;
    Clock::time_point time;
    int authorId = 0;
    std::string authorName;
    AuthorState authorState = AuthorState::Hidden;
};
