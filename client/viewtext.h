// VIEW: every piece of text the pages show, built from model objects.
// The controller hands the pages Sessions, Users and error codes; the pages
// call these to turn them into words (M2 grading note: formatting display
// text is the view's job, not the controller's).

#pragma once

#include <QString>

#include "appcontroller.h"
#include "course.h"
#include "answer.h"
#include "error.h"
#include "filters.h"
#include "lostping.h"
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

// The middle of the nav bar: "Educator View" / "Student View" ("" for None).
QString formatRoleView(CourseRole role);

// The author line: the name, "Anonymous" or "Deleted user".
QString formatAuthor(AuthorState state, const std::string& name);

// One row of a course's session list:
// "Lecture 3  ·  Nov 15, 2:15 PM  ·  50 min  ·  Lost: 3  ·  Questions: 1  ·  Published"
QString formatSessionRow(const SessionSummary& session);

// "Live  ·  Lost: 3  ·  Questions: 1"
QString formatLiveStatus(const Session& session);

// One row of My Courses: "CSCI 187  ·  Educator  ·  Course code K7M2QX"
// or "CSCI 187  ·  Student".
QString formatCourseRow(const Course& course);

// A course's card on My Courses: the course name is its title, its role
// ("Educator" / "Student") its badge, and this its detail line:
// "Course code K7M2QX" for the educator, "" for a student.
QString formatCourseCardDetail(const Course& course);

// A session's card: its name is the title, formatStatus its badge, and these
// two detail lines: "Nov 15, 2:15 PM  ·  50 min" (just the time while live)
// and "Lost: 3  ·  Questions: 1" ("" for an empty review placeholder).
QString formatSessionCardDetail(const SessionSummary& session);

// A student's own items in a live session:
// "12:34  ·  I'm lost" and "12:34  ·  Q: what is a foreign key?  (edited)".
QString formatLostRow(const Session& session, const LostPing& ping);
QString formatQuestionRow(const Session& session, const SessionQuestion& question);

// A question in the session viewer, with who asked it:
// "12:34  ·  what is a foreign key?  —  ana  (edited)".
QString formatViewerQuestion(const Session& session, const SessionQuestion& question);

// One answer under a question: "Prof. Lee (educator): use a JOIN  (edited)".
QString formatAnswerRow(const Answer& answer);

// A timeline bar's tooltip: "5:00–6:00  ·  Lost: 3  ·  Questions: 1"
// (+ "  ·  most confusing" on the peak bar).
QString formatBucket(const Timeline::Bucket& bucket, long long bucketSeconds, bool isPeak);

// "Starts at 4:30 in the video  ·  matched from Panopto" / "  ·  pasted link".
QString formatRecording(const Recording& recording);

// How long ago, for the bell menu: "Just now", "5 min ago", "3 h ago",
// then the clock time ("Nov 15, 2:15 PM").
QString formatTimeAgo(Session::Clock::time_point time,
                      Session::Clock::time_point now = Session::Clock::now());

// One bell-menu entry:
//   "ana joined CSCI 187"                      (a student joined your course)
//   "A student joined CSCI 187"                (if the name is missing)
//   "ana joined your live session Lecture 1"
//   "Lecture 1 was published in CSCI 187"
QString formatNotification(const AppNotification& notification);

// The short confirmation for each Notice the controller sends.
QString getNoticeText(AppController::Notice notice);

// The sentence shown for each Error. "" for None. The one place error
// codes become words.
QString getErrorText(Error error);

}  // namespace ViewText
