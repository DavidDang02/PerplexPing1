#include "user.h"

User::User(int id, std::string name) {
	this->id = id;
	this->name = name;
}

int User::getId() const{
	return this->id;
}

const std::string& User::getName() const {
	return this->name;
}


