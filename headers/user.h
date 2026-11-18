// MODEL: one person who can log in. An id, a name and a school email.
// There is no "educator account": being an educator is per course
// (Course::getEducatorId), never stored on the user. The password and its hash never
// leave the server's model/database. Plain C++ (no Qt).
//
// Also here, because every other model header needs them: AuthorState (how
// much the viewer may know about who wrote something) and Participant.
//
// Owner: Model layer.

#pragma once

#include <string>

class User {
public:
    User();  // id 0, empty name: "nobody"
    User(int id, std::string name, std::string email = "");

    int getId() const;
    const std::string& getName() const;   // as typed at registration (U2)
    // The school email (U6). "" only for accounts imported from the M2
    // database that have not set one yet.
    const std::string& getEmail() const;
    void setEmail(const std::string& email);

private:
    int id = 0;
    std::string name;
    std::string email;
};

// What the viewer may know about the author of an input or an answer.
// The VIEW words it (ViewText::formatAuthor): Known -> the name,
// Hidden -> "Anonymous", Deleted -> "Deleted user".
enum class AuthorState {
    Known,    // id and name are filled in
    Hidden,   // stripped by Visibility for this viewer (V2, V3, V4): id 0, name ""
    Deleted,  // the account was deleted (U8): id 0, name ""
};

// A student who is connected to a live session right now (rule V5).
// Just data.
struct Participant {
    int userId = 0;
    std::string name;
};
