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

class Permissions {
public:
    static bool canManageCourse(const Viewer& viewer);
    static bool canStartSession(const Viewer& viewer);
    static bool canEndSession(const Viewer& viewer);
    static bool canPublishSession(const Viewer& viewer, const Session& session);
    static bool canRenameSession(const Viewer& viewer);
    static bool canDeleteSession(const Viewer& viewer);
    static bool canOpenSession(const Viewer& viewer, const Session& session);

    static bool canAddInput(const Viewer& viewer, const Session& session);
    static bool canEditQuestion(const Viewer& viewer, const Session& session, const SessionQuestion& question);
    static bool canDeleteLost(const Viewer& viewer, const Session& session, const LostPing& ping);
    static bool canDeleteQuestion(const Viewer& viewer, const Session& session, const SessionQuestion& question);

    static bool canAddAnswer(const Viewer& viewer, const Session& session);
    static bool canEditAnswer(const Viewer& viewer, const Answer& answer);
    static bool canDeleteAnswer(const Viewer& viewer, const Answer& answer);

    static bool canManageRecording(const Viewer& viewer, const Session& session);
};

class Visibility {
public:
    static void redactCourse(Course& course, const Viewer& viewer);
    static void redactSession(Session& session, const Course& course, const Viewer& viewer);
    static void redactSessionSummary(SessionSummary& summary, const Viewer& viewer);
};