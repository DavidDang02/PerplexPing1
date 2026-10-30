// MODEL: one person who can log in. Only an id and a name; the password
// never leaves Database (it stores a salted hash, not the password).

#pragma once

#include <string>

class User {
public:
    User(int id, std::string name);

    int getId() const;
    const std::string& getName() const;

private:
    int id;
    std::string name;
};
