#include "permissions.h"

bool Permissions::canManageCourse(const Viewer& viewer) {
    return viewer.role == CourseRole::Educator;
}

bool Permissions::canStartSession(const Viewer& viewer) {
    return viewer.role == CourseRole::Educator;
}

bool Permissions::canEndSession(const Viewer& viewer, const Session& session) {
    return viewer.role == CourseRole::Educator && session.isLive();
}

bool Permissions::canPublishSession(const Viewer& viewer, const Session& session) {
    return viewer.role == CourseRole::Educator && session.isInReview();
}

bool Permissions::canRenameSession(const Viewer& viewer) {
    return viewer.role == CourseRole::Educator;
}

bool Permissions::canDeleteSession(const Viewer& viewer, const Session& session) {
    return viewer.role == CourseRole::Educator && !session.isLive();  // S9: end it first
}

bool Permissions::canOpenSession(const Viewer& viewer, const Session& session) {
    if (viewer.role == CourseRole::Educator) {
        return true;
    }
    if (viewer.role == CourseRole::Student) {
        return session.isLive() || session.isPublished();
    }
    return false;
}

bool Permissions::canAddInput(const Viewer& viewer, const Session& session) {
    return viewer.role == CourseRole::Student && session.isLive();
}

bool Permissions::canEditQuestion(const Viewer& viewer, const Session& session, const SessionQuestion& question) {
    return session.isLive() && question.isBy(viewer.userId);
}

bool Permissions::canDeleteLost(const Viewer& viewer, const Session& session, const LostPing& ping) {
    return session.isLive() && ping.isBy(viewer.userId);
}

bool Permissions::canDeleteQuestion(const Viewer& viewer, const Session& session, const SessionQuestion& question) {
    if (session.isLive()) {
        return question.isBy(viewer.userId);
    }
    // Ended sessions allow educator moderation (rule I7).
    return viewer.role == CourseRole::Educator;
}

bool Permissions::canAddAnswer(const Viewer& viewer, const Session& session) {
    if (session.isLive()) {
        return false;
    }
    if (viewer.role == CourseRole::Educator) {
        return true;
    }
    if (viewer.role == CourseRole::Student) {
        return session.isPublished();
    }
    return false;
}

bool Permissions::canEditAnswer(const Viewer& viewer, const Answer& answer) {
    return answer.isBy(viewer.userId);
}

bool Permissions::canDeleteAnswer(const Viewer& viewer, const Answer& answer) {
    // The author, or the course's educator as moderation (A7). Answers only
    // exist once the session has ended.
    return answer.isBy(viewer.userId) || viewer.role == CourseRole::Educator;
}

bool Permissions::canManageRecording(const Viewer& viewer, const Session& session) {
    return viewer.role == CourseRole::Educator && session.isEnded();
}

void Visibility::redactCourse(Course& course, const Viewer& viewer) {
    if (viewer.role != CourseRole::Educator) {
        course.setJoinCode("");
        course.setPanoptoFolderId("");  // only "Find recording" (educator) needs it
        course.setStudentIds({});       // classmates' ids are the educator's business
    }
}

void Visibility::redactSession(Session& session, const Course& course, const Viewer& viewer) {
    if (viewer.role != CourseRole::Educator) {
        session.setJoinCode("");
    }

    // While live, a student sees only their own pings and questions
    // (CONTRACT.md 3, "Live"). The educator sees all of them (counts).
    const bool ownOnly = session.isLive() && viewer.role != CourseRole::Educator;

    // Lost ping author visibility (V2, V4)
    std::vector<LostPing> pings;
    for (const auto& ping : session.getLostPings()) {
        if (!ownOnly || ping.isBy(viewer.userId)) {
            pings.push_back(ping);
        }
    }
    for (auto& ping : pings) {
        if (course.isAnonymous() || ping.getAuthorId() != viewer.userId) {
            ping.hideAuthor();
        }
    }
    session.setLostPings(std::move(pings));

    // Question author & sharing visibility (V1, V2)
    std::vector<SessionQuestion> filteredQuestions;
    for (auto question : session.getQuestions()) {
        if (ownOnly && !question.isBy(viewer.userId)) {
            continue;
        }
        if (viewer.role == CourseRole::Student && !course.areQuestionsShared() && !question.isBy(viewer.userId)) {
            continue; // Student does not see other students' questions if not shared
        }

        if (course.isAnonymous() && !question.isBy(viewer.userId)) {
            question.hideAuthor();
        }

        std::vector<Answer> answers = question.getAnswers();
        for (auto& answer : answers) {
            if (course.isAnonymous() && !answer.isByEducator() && !answer.isBy(viewer.userId)) {
                answer.hideAuthor();
            }
        }
        question.setAnswers(std::move(answers));
        filteredQuestions.push_back(std::move(question));
    }
    session.setQuestions(std::move(filteredQuestions));
}

void Visibility::redactSessionSummary(SessionSummary& summary, const Viewer& viewer) {
    if (viewer.role == CourseRole::Student && summary.isInReview()) {
        summary.lostCount = 0;
        summary.questionCount = 0;
        summary.hasRecording = false;
    }
}