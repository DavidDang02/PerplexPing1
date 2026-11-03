#include "sessioninput.h"

#include <utility>

SessionInput::SessionInput(InputType type, Clock::time_point time, std::string questionText)
    : type(type), time(time), questionText(std::move(questionText)) {}

void SessionInput::setId(int id){
    this->id = id;
}

int SessionInput::getId(){
    return id;
}

InputType SessionInput::getType() const {
    return type;
}

SessionInput::Clock::time_point SessionInput::getTime() const {
    return time;
}

const std::string& SessionInput::getQuestionText() const {
    return questionText;
}
