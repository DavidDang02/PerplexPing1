#include "doctest.h"
#include "session.h"

#include <chrono>

TEST_CASE("Session rejects any input after ending") {
    Session session;
    CHECK(session.lostButtonClicked());
    session.endSession();

    CHECK(session.isEnded());
    // Direct shortcut methods fail after end
    CHECK_FALSE(session.lostButtonClicked());
    CHECK_FALSE(session.askQuestion("Will this be recorded?"));

    // addInput fails after end
    SessionInput input(InputType::Lost, Session::Clock::now());
    CHECK_FALSE(session.addInput(input));

    // Inputs collection does not record rejected entries
    CHECK(session.getInputs().size() == 1);
    CHECK(session.getPressCount() == 1);
    CHECK(session.getQuestionCount() == 0);
}

TEST_CASE("Session rejects empty question text") {
    Session session;

    CHECK_FALSE(session.askQuestion(""));
    CHECK(session.getQuestionCount() == 0);
    CHECK(session.getInputs().empty());

    // Valid question succeeds
    CHECK(session.askQuestion("What is the time complexity?"));
    CHECK(session.getQuestionCount() == 1);
    CHECK(session.getInputs().size() == 1);
}

TEST_CASE("Session splits press count and question count accurately") {
    Session session;

    CHECK(session.lostButtonClicked());
    CHECK(session.askQuestion("Question 1"));
    CHECK(session.lostButtonClicked());
    CHECK(session.askQuestion("Question 2"));
    CHECK(session.lostButtonClicked());

    CHECK(session.getPressCount() == 3);
    CHECK(session.getQuestionCount() == 2);
    REQUIRE(session.getInputs().size() == 5);

    // Verify ordering and types
    CHECK(session.getInputs()[0].getType() == InputType::Lost);
    CHECK(session.getInputs()[1].isQuestion());
    CHECK(session.getInputs()[1].getQuestionText() == "Question 1");
    CHECK(session.getInputs()[2].getType() == InputType::Lost);
    CHECK(session.getInputs()[3].isQuestion());
    CHECK(session.getInputs()[3].getQuestionText() == "Question 2");
    CHECK(session.getInputs()[4].getType() == InputType::Lost);

    CHECK(session.getLostButtonTimestamps().size() == 3);
}

TEST_CASE("Session measures input offsets from session start time") {
    const auto t0 = Session::Clock::time_point(std::chrono::seconds(10000));
    Session session(101, 42, t0);

    CHECK(session.getId() == 101);
    CHECK(session.getUserId() == 42);
    CHECK(session.getStartTime() == t0);

    SessionInput inputLost(InputType::Lost, t0 + std::chrono::seconds(35));
    SessionInput inputQuestion(InputType::Question, t0 + std::chrono::seconds(125), "Why?");

    CHECK(session.addInput(inputLost));
    CHECK(session.addInput(inputQuestion));

    CHECK(session.getOffsetSeconds(inputLost) == 35);
    CHECK(session.getOffsetSeconds(inputQuestion) == 125);

    const auto lostOffsets = session.getPressOffsetsSeconds();
    REQUIRE(lostOffsets.size() == 1);
    CHECK(lostOffsets[0] == 35);
}

TEST_CASE("endSession(when) sets and preserves the specific end time") {
    const auto t0 = Session::Clock::time_point(std::chrono::seconds(5000));
    const auto tEnd = t0 + std::chrono::seconds(1800);

    Session session(1, 1, t0);
    session.endSession(tEnd);

    CHECK(session.isEnded());
    CHECK(session.getEndTime() == tEnd);

    // Second call with different time point must not overwrite original end time
    session.endSession(tEnd + std::chrono::seconds(60));
    CHECK(session.getEndTime() == tEnd);

    session.endSession();
    CHECK(session.getEndTime() == tEnd);
}