#include "user.h"

#include <utility>

User::User() = default;

User::User(int id, std::string name)
    : id(id), name(std::move(name)) {}

int User::getId() const {
    return this->id;
}

const std::string& User::getName() const {
    return this->name;
}
