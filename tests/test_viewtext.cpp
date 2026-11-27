// Every error the server can send has a real sentence for the user
// (M2 grading note: feedback on every failed action).

#include "doctest.h"

#include <chrono>

#include "error.h"
#include "viewtext.h"

TEST_CASE("ViewText: every Error has its own sentence") {
    for (int i = static_cast<int>(Error::None) + 1;
         i <= static_cast<int>(Error::PanoptoFolderNotLinked); ++i) {
        const Error error = static_cast<Error>(i);
        CAPTURE(getErrorCode(error));
        const QString text = ViewText::getErrorText(error);
        CHECK_FALSE(text.isEmpty());
        CHECK_FALSE(text.startsWith(QStringLiteral("Something went wrong")));
    }
    CHECK(ViewText::getErrorText(Error::None).isEmpty());
}

TEST_CASE("ViewText: every Notice has its own sentence") {
    using N = AppController::Notice;
    for (int i = static_cast<int>(N::Registered); i <= static_cast<int>(N::Reconnected); ++i) {
        CAPTURE(i);
        CHECK_FALSE(ViewText::getNoticeText(static_cast<N>(i)).isEmpty());
    }
}

TEST_CASE("ViewText: offsets, durations and authors read naturally") {
    CHECK(ViewText::formatOffset(0) == "0:00");
    CHECK(ViewText::formatOffset(754) == "12:34");
    CHECK(ViewText::formatOffset(3725) == "1:02:05");
    CHECK(ViewText::formatOffset(-5) == "0:00");
    const auto t0 = Session::Clock::time_point(std::chrono::seconds(0));
    CHECK(ViewText::formatDuration(t0, t0 + std::chrono::seconds(30)) == "<1 min");
    CHECK(ViewText::formatDuration(t0, t0 + std::chrono::minutes(50)) == "50 min");
    CHECK(ViewText::formatDuration(t0, t0 + std::chrono::minutes(75)) == "1 h 15 min");
    CHECK(ViewText::formatAuthor(AuthorState::Hidden, "ana") == "Anonymous");
    CHECK(ViewText::formatAuthor(AuthorState::Deleted, "") == "Deleted user");
    CHECK(ViewText::formatAuthor(AuthorState::Known, "ana") == "ana");
}
