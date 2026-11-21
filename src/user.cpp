#include "user.h"

#include <utility>

User::User() : id(0), name(""), email("") {}

User::User(int id, std::string name, std::string email)
    : id(id), name(std::move(name)), email(std::move(email)) {}

int User::getId() const {
    return this->id;
}

const std::string& User::getName() const {
    return this->name;
}

const std::string& User::getEmail() const {
    return this->email;
}

void User::setEmail(const std::string& email) {
    this->email = email;
}