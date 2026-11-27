// Every error the server can send has a real sentence for the user
// (M2 grading note: feedback on every failed action).

#include "doctest.h"

#include "error.h"
#include "viewtext.h"

TEST_CASE("ViewText: every Error has its own sentence") {
    for (int i = static_cast<int>(Error::None) + 1;
         i <= static_cast<int>(Error::RecordingNotReady); ++i) {
        const Error error = static_cast<Error>(i);
        CAPTURE(getErrorCode(error));
        const QString text = ViewText::getErrorText(error);
        CHECK_FALSE(text.isEmpty());
        CHECK_FALSE(text.startsWith(QStringLiteral("Something went wrong")));
    }
    CHECK(ViewText::getErrorText(Error::None).isEmpty());
}
