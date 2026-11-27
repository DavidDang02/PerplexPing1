#pragma once

#include "answer.h"
#include "course.h"
#include "lostping.h"
#include "recording.h"
#include "session.h"
#include "sessionquestion.h"

struct Viewer {
    int userId = 0;
    CourseRole role = CourseRole::None;
};

// Who may do what: each check answers only yes or no. AppModel calls one
// of these before every change and, on a no, picks the error that says why
// (so the rules live here, and the error codes in AppModel). The viewer's
// role is worked out by AppModel for this course (never saved).
class Permissions {
public:
    static bool canManageCourse(const Viewer& viewer);   // C6, C9: rename, toggles, delete
    static bool canStartSession(const Viewer& viewer);   // S1
    static bool canEndSession(const Viewer& viewer, const Session& session);      // S7: live only
    static bool canPublishSession(const Viewer& viewer, const Session& session);  // S11: in review only
    static bool canRenameSession(const Viewer& viewer);  // S8
    static bool canDeleteSession(const Viewer& viewer, const Session& session);   // S9: not live
    static bool canOpenSession(const Viewer& viewer, const Session& session);     // S6, S12

    static bool canAddInput(const Viewer& viewer, const Session& session);  // I1: students, live
    static bool canEditQuestion(const Viewer& viewer, const Session& session, const SessionQuestion& question);  // I3, I4
    static bool canDeleteLost(const Viewer& viewer, const Session& session, const LostPing& ping);  // I5
    static bool canDeleteQuestion(const Viewer& viewer, const Session& session, const SessionQuestion& question);  // I5, I7

    static bool canAddAnswer(const Viewer& viewer, const Session& session);  // A1, A2
    static bool canEditAnswer(const Viewer& viewer, const Answer& answer);   // A3: author only
    static bool canDeleteAnswer(const Viewer& viewer, const Answer& answer); // A7: author or educator

    static bool canManageRecording(const Viewer& viewer, const Session& session);  // R1: educator, ended
};

class Visibility {
public:
    static void redactCourse(Course& course, const Viewer& viewer);
    static void redactSession(Session& session, const Course& course, const Viewer& viewer);
    static void redactSessionSummary(SessionSummary& summary, const Viewer& viewer);
};