// Every error the server can send has a real sentence for the user
// (M2 grading note: feedback on every failed action).

#include "doctest.h"

#include <chrono>

#include <QDateTime>
#include <QTimeZone>

#include "error.h"
#include "viewtext.h"

TEST_CASE("ViewText: every Error has its own sentence") {
    for (int i = static_cast<int>(Error::None) + 1;
         i <= static_cast<int>(Error::PanoptoNoAccess); ++i) {
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

TEST_CASE("ViewText: bell notifications") {
    AppNotification n;
    n.kind = AppNotification::Kind::StudentJoinedCourse;
    n.courseName = "CSCI 187";
    n.studentName = "ana";
    CHECK(ViewText::formatNotification(n) == QString("ana joined CSCI 187"));
    n.studentName = "";
    CHECK(ViewText::formatNotification(n) == QString("A student joined CSCI 187"));

    n.kind = AppNotification::Kind::StudentJoinedSession;
    n.studentName = "bob";
    n.sessionName = "Lecture 1";
    CHECK(ViewText::formatNotification(n) == QString("bob joined your live session Lecture 1"));

    n.kind = AppNotification::Kind::SessionPublished;
    CHECK(ViewText::formatNotification(n) == QString("Lecture 1 was published in CSCI 187"));
    n.courseName = "";
    CHECK(ViewText::formatNotification(n) == QString("Lecture 1 was published in your course"));

    const auto now = Session::Clock::now();
    CHECK(ViewText::formatTimeAgo(now - std::chrono::seconds(20), now) == QString("Just now"));
    CHECK(ViewText::formatTimeAgo(now - std::chrono::minutes(5), now) == QString("5 min ago"));
    CHECK(ViewText::formatTimeAgo(now - std::chrono::hours(3), now) == QString("3 h ago"));
    const auto old = now - std::chrono::hours(30);
    CHECK(ViewText::formatTimeAgo(old, now) == ViewText::formatClockTime(old));
}

TEST_CASE("ViewText: course and session cards") {
    Course course(1, "CSCI 187");
    course.setRole(CourseRole::Educator);
    course.setJoinCode("K7M2QX");
    CHECK(ViewText::formatCourseCardDetail(course) == "Course code K7M2QX");
    CHECK(ViewText::formatCourseTitle(course) == "CSCI 187");  // the educator
    course.setRole(CourseRole::Student);
    CHECK(ViewText::formatCourseCardDetail(course) == "");
    CHECK(ViewText::formatCourseTitle(course) == "CSCI 187");  // educator deleted
    course.setEducatorName("prof");
    CHECK(ViewText::formatCourseTitle(course) == "CSCI 187 - prof");

    const auto t0 = Session::Clock::time_point(std::chrono::hours(24 * 365 * 50));
    const QString when = ViewText::formatClockTime(t0);
    SessionSummary session;
    session.name = "Lecture 3";
    session.startTime = t0;
    session.status = SessionStatus::Live;
    session.lostCount = 3;
    session.questionCount = 1;
    CHECK(ViewText::formatSessionCardDetail(session) == when + "\nLost: 3  ·  Questions: 1");

    session.status = SessionStatus::Published;
    session.endTime = t0 + std::chrono::minutes(50);
    CHECK(ViewText::formatSessionCardDetail(session) ==
          when + "  ·  50 min\nLost: 3  ·  Questions: 1");

    // A student's empty review placeholder: just when.
    session.status = SessionStatus::InReview;
    session.lostCount = 0;
    session.questionCount = 0;
    CHECK(ViewText::formatSessionCardDetail(session) == when + "  ·  50 min");
}

TEST_CASE("ViewText: the nav bar's role view") {
    CHECK(ViewText::formatRoleView(CourseRole::Educator) == "Educator View");
    CHECK(ViewText::formatRoleView(CourseRole::Student) == "Student View");
    CHECK(ViewText::formatRoleView(CourseRole::None) == "");
}

TEST_CASE("ViewText: when a course was created") {
    Course course(1, "CSCI 187");
    CHECK(ViewText::formatCourseCreated(course) == "");  // not sent: no line
    // Noon UTC, so it's the same date in any time zone the tests run in.
    const QDateTime noon(QDate(2026, 11, 15), QTime(12, 0), QTimeZone::UTC);
    course.setCreatedTime(std::chrono::system_clock::time_point(
        std::chrono::milliseconds(noon.toMSecsSinceEpoch())));
    CHECK(ViewText::formatCourseCreated(course) == "Course created on November 15, 2026");
}

TEST_CASE("ViewText: the live session next to Join") {
    SessionSummary session;
    session.name = "Lecture 3";
    const QDateTime start = QDateTime(QDate(2026, 11, 15), QTime(14, 15));  // local time
    session.startTime = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(start.toMSecsSinceEpoch()));
    CHECK(ViewText::formatLiveSessionLabel(session) == "Lecture 3  ·  started 2:15 PM");
}
