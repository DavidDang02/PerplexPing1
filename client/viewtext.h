// VIEW: every piece of text the pages show, built from model objects.
// The controller hands the pages Sessions, Users and error codes; the pages
// call these to turn them into words (M2 grading note: formatting display
// text is the view's job, not the controller's).

#pragma once

#include <QString>

#include "appcontroller.h"
#include "course.h"
#include "error.h"
#include "session.h"
#include "sessionquestion.h"

namespace ViewText {

// "Nov 15, 2:15 PM" in the computer's time zone.
QString formatClockTime(Session::Clock::time_point time);

// "<1 min", "50 min", "1 h 15 min".
QString formatDuration(Session::Clock::time_point start, Session::Clock::time_point end);

// Seconds into a session or video: 754 -> "12:34", 3725 -> "1:02:05".
QString formatOffset(long long seconds);

// "Live now" / "In review" / "Published".
QString formatStatus(SessionStatus status);

// "Educator" / "Student".
QString formatRole(CourseRole role);

// The author line: the name, "Anonymous" or "Deleted user".
QString formatAuthor(AuthorState state, const std::string& name);

// One row of a course's session list:
// "Lecture 3  ·  Nov 15, 2:15 PM  ·  50 min  ·  Lost: 3  ·  Questions: 1  ·  Published"
QString formatSessionRow(const SessionSummary& session);

// "Live  ·  Lost: 3  ·  Questions: 1"
QString formatLiveStatus(const Session& session);

// The short confirmation for each Notice the controller sends.
QString getNoticeText(AppController::Notice notice);

// The sentence shown for each Error. "" for None. The one place error
// codes become words.
QString getErrorText(Error error);

}  // namespace ViewText
