#include "doctest.h"
#include "sessioninput.h"

#include <chrono>

TEST_CASE("SessionInput properly initializes Lost inputs") {
    const auto now = SessionInput::Clock::now();
    SessionInput input(InputType::Lost, now);

    CHECK(input.getType() == InputType::Lost);
    CHECK(input.isQuestion() == false);
    CHECK(input.getTime() == now);
    CHECK(input.getQuestionText().empty());
}

TEST_CASE("SessionInput properly initializes Question inputs with question text") {
    const auto now = SessionInput::Clock::now();
    const std::string text = "Could you repeat that slide?";
    SessionInput input(InputType::Question, now, text);

    CHECK(input.getType() == InputType::Question);
    CHECK(input.isQuestion() == true);
    CHECK(input.getTime() == now);
    CHECK(input.getQuestionText() == text);
}

TEST_CASE("SessionInput preserves epoch and boundary timestamps") {
    const SessionInput::Clock::time_point epoch{}; // 1970-01-01 00:00:00 UTC
    SessionInput input(InputType::Lost, epoch);

    CHECK(input.getTime() == epoch);
    CHECK(input.getType() == InputType::Lost);
    CHECK_FALSE(input.isQuestion());

    // Distant future timestamp
    const auto futureTime = epoch + std::chrono::hours(24 * 365 * 100); // ~100 years out
    SessionInput futureInput(InputType::Question, futureTime, "Future question?");
    CHECK(futureInput.getTime() == futureTime);
    CHECK(futureInput.isQuestion());
}

TEST_CASE("SessionInput preserves multi-line and special character question text") {
    const auto now = SessionInput::Clock::now();
    const std::string complexText = 
        "Is this O(N^2) or O(N log N)?\n"
        "Also, how does 'nullptr' behave here: \"foo\" & <bar>?";

    SessionInput input(InputType::Question, now, complexText);

    CHECK(input.getType() == InputType::Question);
    CHECK(input.isQuestion());
    CHECK(input.getQuestionText() == complexText);
}