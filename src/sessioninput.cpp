#include "sessioninput.h"

#include <utility>

SessionInput::SessionInput(InputType type, Clock::time_point time, std::string questionText)
    : type(type), time(time), questionText(std::move(questionText)) {}

InputType SessionInput::getType() const {
    return type;
}

bool SessionInput::isQuestion() const {
    return type == InputType::Question;
}

SessionInput::Clock::time_point SessionInput::getTime() const {
    return time;
}

const std::string& SessionInput::getQuestionText() const {
    return questionText;
}