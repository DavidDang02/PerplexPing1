// Tests for Validation: every limit from CONTRACT.md section 3, at its edges.

#include "doctest.h"
#include "validation.h"

#include <string>

TEST_CASE("Validation: trim and length") {
    CHECK(Validation::trim("  ana \t\n") == "ana");
    CHECK(Validation::trim("   ") == "");
    CHECK(Validation::getLength("naïve") == 5);  // ï is 2 bytes but 1 character
    CHECK(Validation::toUpper("k7m2qx") == "K7M2QX");
}

TEST_CASE("Validation: usernames are 3-20 letters, digits, . _ -") {
    CHECK(Validation::checkUsername("") == Error::EmptyName);
    CHECK(Validation::checkUsername("ab") == Error::NameTooShort);
    CHECK(Validation::checkUsername("abc") == Error::None);
    CHECK(Validation::checkUsername(std::string(20, 'a')) == Error::None);
    CHECK(Validation::checkUsername(std::string(21, 'a')) == Error::NameTooLong);
    CHECK(Validation::checkUsername("ana smith") == Error::NameBadCharacters);
    CHECK(Validation::checkUsername("ana!") == Error::NameBadCharacters);
    CHECK(Validation::checkUsername("ana.b_c-1") == Error::None);
}

TEST_CASE("Validation: passwords are 8-64 characters") {
    CHECK(Validation::checkPassword("") == Error::EmptyPassword);
    CHECK(Validation::checkPassword("1234567") == Error::PasswordTooShort);
    CHECK(Validation::checkPassword("12345678") == Error::None);
    CHECK(Validation::checkPassword(std::string(64, 'p')) == Error::None);
    CHECK(Validation::checkPassword(std::string(65, 'p')) == Error::PasswordTooLong);
    CHECK(Validation::checkPassword("has spaces ok") == Error::None);  // any characters
}

TEST_CASE("Validation: questions and answers") {
    CHECK(Validation::checkQuestion("") == Error::EmptyQuestion);
    CHECK(Validation::checkQuestion(std::string(500, 'q')) == Error::None);
    CHECK(Validation::checkQuestion(std::string(501, 'q')) == Error::QuestionTooLong);
    CHECK(Validation::checkAnswer("") == Error::EmptyAnswer);
    CHECK(Validation::checkAnswer(std::string(1001, 'a')) == Error::AnswerTooLong);
}

TEST_CASE("Validation: emails, names, recordings") {
    CHECK(Validation::checkEmail("") == Error::EmptyEmail);
    CHECK(Validation::checkEmail("no-at-sign") == Error::EmailInvalid);
    CHECK(Validation::checkEmail("a@b@scu.edu") == Error::EmailInvalid);
    CHECK(Validation::checkEmail("me@gmail.com") == Error::EmailNotSchool);
    CHECK(Validation::checkEmail("Me@SCU.EDU") == Error::None);
    CHECK(Validation::checkCourseName(std::string(61, 'c')) == Error::CourseNameTooLong);
    CHECK(Validation::checkSessionName("") == Error::EmptySessionName);
    CHECK(Validation::checkRecordingUrl("ftp://x") == Error::RecordingUrlInvalid);
    CHECK(Validation::checkRecordingUrl("https://scu.hosted.panopto.com/x") == Error::None);
    CHECK(Validation::checkRecordingOffset(-90) == Error::None);
    CHECK(Validation::checkRecordingOffset(24LL * 60 * 60 + 1) == Error::RecordingOffsetInvalid);
}
