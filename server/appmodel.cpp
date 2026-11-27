#include "appmodel.h"

#include <chrono>
#include <string>
#include <vector>

#include "database.h"
#include "joincodes.h"
#include "passwordhasher.h"
#include "permissions.h"
#include "validation.h"

AppModel::AppModel(Database* db, NowFunction now, int passwordIterations)
    : db(db), now(std::move(now)), passwordIterations(passwordIterations) {}

AppModel::Clock::time_point AppModel::getNow() const {
    const auto current = now();
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(current.time_since_epoch());
    return Clock::time_point(secs);
}

// ================= Accounts [F1, F4] =================

Result<User> AppModel::registerUser(const std::string& name,
                                    const std::string& password,
                                    const std::string& email) {
    const std::string trimmedName = Validation::trim(name);
    if (const Error err = Validation::checkUsername(trimmedName); err != Error::None) {
        return err;
    }
    if (const Error err = Validation::checkPassword(password); err != Error::None) {
        return err;
    }
    const std::string trimmedEmail = Validation::trim(email);
    if (const Error err = Validation::checkEmail(trimmedEmail); err != Error::None) {
        return err;
    }

    if (!db) {
        return Error::StorageError;
    }

    if (db->findUserByName(trimmedName).isOk()) {
        return Error::NameTaken;
    }
    if (db->findUserByEmail(trimmedEmail).isOk()) {
        return Error::EmailTaken;
    }

    const std::string salt = PasswordHasher::generateSaltHex();
    const std::string hash = PasswordHasher::hashPassword(password, salt, passwordIterations);

    Result<User> created = db->createUser(trimmedName, trimmedEmail, hash, salt, passwordIterations, getNow());
    if (!created.isOk()) {
        return created.error;
    }

    currentUser = created.value;
    return *currentUser;
}

Result<User> AppModel::logIn(const std::string& name, const std::string& password) {
    const std::string trimmedName = Validation::trim(name);
    if (trimmedName.empty()) {
        return Error::EmptyName;
    }
    if (password.empty()) {
        return Error::EmptyPassword;
    }

    if (!db) {
        return Error::StorageError;
    }

    Result<User> found = db->findUserByName(trimmedName);
    if (!found.isOk()) {
        // Hash anyway, so an unknown name takes as long as a wrong password
        // and the timing doesn't tell whether the name exists (U5).
        PasswordHasher::hashPassword(password, PasswordHasher::generateSaltHex(), passwordIterations);
        return Error::WrongLogin;
    }

    Result<Credentials> creds = db->getCredentials(found.value.getId());
    if (!creds.isOk()) {
        return Error::WrongLogin;
    }

    if (!PasswordHasher::verifyPassword(password, creds.value.saltHex, creds.value.iterations, creds.value.hashHex)) {
        return Error::WrongLogin;
    }

    currentUser = found.value;
    return *currentUser;
}

void AppModel::logOut() {
    currentUser.reset();
}

bool AppModel::isLoggedIn() const {
    return currentUser.has_value();
}

const User* AppModel::getCurrentUser() const {
    return currentUser ? &*currentUser : nullptr;
}

Error AppModel::changePassword(const std::string& oldPassword, const std::string& newPassword) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<Credentials> creds = db->getCredentials(currentUser->getId());
    if (!creds.isOk() || !PasswordHasher::verifyPassword(oldPassword, creds.value.saltHex, creds.value.iterations, creds.value.hashHex)) {
        return Error::WrongPassword;
    }

    if (const Error err = Validation::checkPassword(newPassword); err != Error::None) {
        return err;
    }

    const std::string salt = PasswordHasher::generateSaltHex();
    const std::string hash = PasswordHasher::hashPassword(newPassword, salt, passwordIterations);
    if (!db->setPassword(currentUser->getId(), hash, salt, passwordIterations).isOk()) {
        return Error::StorageError;
    }

    return Error::None;
}

Result<User> AppModel::updateEmail(const std::string& email) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    const std::string trimmed = Validation::trim(email);
    if (const Error err = Validation::checkEmail(trimmed); err != Error::None) {
        return err;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<User> existing = db->findUserByEmail(trimmed);
    if (existing.isOk() && existing.value.getId() != currentUser->getId()) {
        return Error::EmailTaken;
    }

    Result<bool> res = db->setEmail(currentUser->getId(), trimmed);
    if (!res.isOk()) {
        return res.error;
    }

    Result<User> refreshed = db->getUser(currentUser->getId());
    if (refreshed.isOk()) {
        currentUser = refreshed.value;
        return *currentUser;
    }
    return Error::StorageError;
}

Error AppModel::deleteAccount(const std::string& password) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<Credentials> creds = db->getCredentials(currentUser->getId());
    if (!creds.isOk() || !PasswordHasher::verifyPassword(password, creds.value.saltHex, creds.value.iterations, creds.value.hashHex)) {
        return Error::WrongPassword;
    }

    const int hosting = db->countLiveSessionsHostedBy(currentUser->getId());
    if (hosting < 0) {
        return Error::StorageError;
    }
    if (hosting > 0) {
        return Error::HostingLiveSession;
    }

    if (!db->deleteUser(currentUser->getId()).isOk()) {
        return Error::StorageError;
    }

    currentUser.reset();
    return Error::None;
}

// ================= Courses [F2, F4] =================

Result<Course> AppModel::createCourse(const std::string& name, bool anonymous, bool questionsShared) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    const std::string trimmed = Validation::trim(name);
    if (const Error err = Validation::checkCourseName(trimmed); err != Error::None) {
        return err;
    }
    if (!db) {
        return Error::StorageError;
    }

    const std::string joinCode = makeFreeCourseCode();
    return db->createCourse(trimmed, joinCode, anonymous, questionsShared, currentUser->getId(), getNow());
}

Result<Course> AppModel::joinCourse(const std::string& joinCode) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    const std::string code = Validation::toUpper(Validation::trim(joinCode));
    Result<Course> found = db->findCourseByJoinCode(code, currentUser->getId());
    if (!found.isOk()) {
        return Error::NoSuchCourseCode;
    }

    if (found.value.getRole() == CourseRole::None &&
        !db->addStudent(found.value.getId(), currentUser->getId(), getNow()).isOk()) {
        return Error::StorageError;
    }

    Result<Course> courseRes = db->getCourse(found.value.getId(), currentUser->getId());
    if (!courseRes.isOk()) {
        return courseRes.error;
    }

    Course course = courseRes.value;
    Viewer viewer{currentUser->getId(), course.getRole()};
    Visibility::redactCourse(course, viewer);
    return course;
}

Result<std::vector<Course>> AppModel::getCourses() const {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<std::vector<Course>> res = db->listCoursesForUser(currentUser->getId());
    if (!res.isOk()) {
        return res.error;
    }

    std::vector<Course> courses = std::move(res.value);
    for (auto& c : courses) {
        Viewer viewer{currentUser->getId(), c.getRole()};
        Visibility::redactCourse(c, viewer);
    }
    return courses;
}

Result<CourseDetail> AppModel::getCourse(int courseId) const {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<Course> cRes = db->getCourse(courseId, currentUser->getId());
    if (!cRes.isOk() || cRes.value.getRole() == CourseRole::None) {
        return Error::NoSuchCourse;
    }

    Result<std::vector<SessionSummary>> sRes = db->listSessionsForCourse(courseId);
    if (!sRes.isOk()) {
        return sRes.error;
    }

    Course course = cRes.value;
    Viewer viewer{currentUser->getId(), course.getRole()};
    Visibility::redactCourse(course, viewer);

    std::vector<SessionSummary> summaries = std::move(sRes.value);
    for (auto& s : summaries) {
        Visibility::redactSessionSummary(s, viewer);
    }

    return CourseDetail{course, summaries};
}

Result<Course> AppModel::updateCourse(int courseId, const std::string& name, bool anonymous, bool questionsShared) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<Course> cRes = db->getCourse(courseId, currentUser->getId());
    if (!cRes.isOk() || cRes.value.getRole() == CourseRole::None) {
        return Error::NoSuchCourse;
    }
    if (!Permissions::canManageCourse(Viewer{currentUser->getId(), cRes.value.getRole()})) {
        return Error::NotCourseEducator;
    }

    const std::string trimmed = Validation::trim(name);
    if (const Error err = Validation::checkCourseName(trimmed); err != Error::None) {
        return err;
    }

    if (!db->updateCourse(courseId, trimmed, anonymous, questionsShared).isOk()) {
        return Error::StorageError;
    }
    return db->getCourse(courseId, currentUser->getId());
}

Error AppModel::deleteCourse(int courseId) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<Course> cRes = db->getCourse(courseId, currentUser->getId());
    if (!cRes.isOk() || cRes.value.getRole() == CourseRole::None) {
        return Error::NoSuchCourse;
    }
    if (!Permissions::canManageCourse(Viewer{currentUser->getId(), cRes.value.getRole()})) {
        return Error::NotCourseEducator;
    }

    if (!db->deleteCourse(courseId).isOk()) {
        return Error::StorageError;
    }
    return Error::None;
}

Result<Course> AppModel::linkPanoptoFolder(int courseId, const std::string& folderId) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<Course> cRes = db->getCourse(courseId, currentUser->getId());
    if (!cRes.isOk() || cRes.value.getRole() == CourseRole::None) {
        return Error::NoSuchCourse;
    }
    if (!Permissions::canManageCourse(Viewer{currentUser->getId(), cRes.value.getRole()})) {
        return Error::NotCourseEducator;
    }

    const std::string trimmed = Validation::trim(folderId);
    if (const Error err = Validation::checkPanoptoFolderId(trimmed); err != Error::None) {
        return err;
    }

    if (!db->setPanoptoFolder(courseId, trimmed).isOk()) {
        return Error::StorageError;
    }
    return db->getCourse(courseId, currentUser->getId());
}

CourseRole AppModel::getRole(int courseId) const {
    if (!currentUser || !db) {
        return CourseRole::None;
    }
    return db->getRole(courseId, currentUser->getId());
}

bool AppModel::isCourseEducator(int courseId) const {
    return getRole(courseId) == CourseRole::Educator;
}

// ================= Sessions [F2, F4] =================

Result<OpenedSession> AppModel::startSession(int courseId, const std::string& name) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<Course> cRes = db->getCourse(courseId, currentUser->getId());
    if (!cRes.isOk() || cRes.value.getRole() == CourseRole::None) {
        return Error::NoSuchCourse;
    }
    if (!Permissions::canStartSession(Viewer{currentUser->getId(), cRes.value.getRole()})) {
        return Error::NotCourseEducator;
    }
    if (cRes.value.getLiveSessionId() != 0) {
        return Error::CourseHasLiveSession;
    }

    const std::string trimmed = Validation::trim(name);
    if (const Error err = Validation::checkSessionName(trimmed); err != Error::None) {
        return err;
    }

    const std::string joinCode = makeFreeSessionCode();
    Result<Session> sRes = db->createSession(courseId, currentUser->getId(), trimmed, joinCode, getNow());
    if (!sRes.isOk()) {
        return sRes.error;
    }

    Course course = cRes.value;
    course.setLiveSessionId(sRes.value.getId());
    Viewer viewer{currentUser->getId(), CourseRole::Educator};
    return OpenedSession{sRes.value, course, viewer, false};
}

Result<OpenedSession> AppModel::joinSession(const std::string& joinCode) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    const std::string code = Validation::trim(joinCode);
    Result<Session> sRes = db->findLiveSessionByJoinCode(code);
    if (!sRes.isOk()) {
        return Error::NoSuchSessionCode;
    }

    CourseRole role = db->getRole(sRes.value.getCourseId(), currentUser->getId());
    bool newlyEnrolled = false;
    if (role == CourseRole::None) {
        if (!db->addStudent(sRes.value.getCourseId(), currentUser->getId(), getNow()).isOk()) {
            return Error::StorageError;
        }
        role = CourseRole::Student;
        newlyEnrolled = true;
    }

    Result<Course> cRes = db->getCourse(sRes.value.getCourseId(), currentUser->getId());
    Result<Session> full = db->loadSession(sRes.value.getId());
    if (!cRes.isOk() || !full.isOk()) {
        return Error::StorageError;
    }

    Viewer viewer{currentUser->getId(), role};
    Course course = cRes.value;
    Session session = full.value;
    Visibility::redactCourse(course, viewer);
    Visibility::redactSession(session, course, viewer);
    return OpenedSession{session, course, viewer, newlyEnrolled};
}

Result<OpenedSession> AppModel::openSession(int sessionId) const {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }

    if (!Permissions::canOpenSession(viewer, rawRes.value)) {
        return Error::SessionNotPublished;  // a student, while it is in review (S12)
    }

    Result<Session> full = db->loadSession(sessionId);
    if (!full.isOk()) {
        return full.error;
    }

    Session session = full.value;
    Visibility::redactCourse(course, viewer);
    Visibility::redactSession(session, course, viewer);
    return OpenedSession{session, course, viewer, false};
}

Result<Session> AppModel::endSession(int sessionId) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (!Permissions::canEndSession(viewer, rawRes.value)) {
        return viewer.role != CourseRole::Educator ? Error::NotCourseEducator : Error::SessionEnded;
    }

    const Result<bool> ended = db->setSessionEnd(sessionId, getNow());
    if (!ended.isOk()) {
        return Error::StorageError;
    }
    if (!ended.value) {
        return Error::SessionEnded;  // ended a moment ago on another device
    }
    return db->getSession(sessionId);
}

Result<Session> AppModel::publishSession(int sessionId) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (!Permissions::canPublishSession(viewer, rawRes.value)) {
        if (viewer.role != CourseRole::Educator) {
            return Error::NotCourseEducator;
        }
        return rawRes.value.isLive() ? Error::SessionNotEnded : Error::SessionAlreadyPublished;
    }

    const Result<bool> published = db->setSessionPublished(sessionId, getNow());
    if (!published.isOk()) {
        return Error::StorageError;
    }
    if (!published.value) {
        return Error::SessionAlreadyPublished;  // published a moment ago elsewhere
    }
    return db->getSession(sessionId);
}

Result<Session> AppModel::renameSession(int sessionId, const std::string& name) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (!Permissions::canRenameSession(viewer)) {
        return Error::NotCourseEducator;
    }

    const std::string trimmed = Validation::trim(name);
    if (const Error err = Validation::checkSessionName(trimmed); err != Error::None) {
        return err;
    }

    if (!db->renameSession(sessionId, trimmed).isOk()) {
        return Error::StorageError;
    }
    return db->getSession(sessionId);
}

Result<Session> AppModel::deleteSession(int sessionId) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    // A live session still has people in it pressing Lost; end it first.
    if (!Permissions::canDeleteSession(viewer, rawRes.value)) {
        return viewer.role != CourseRole::Educator ? Error::NotCourseEducator : Error::SessionNotEnded;
    }

    Session before = rawRes.value;
    if (!db->deleteSession(sessionId).isOk()) {
        return Error::StorageError;
    }
    return before;
}

// ================= Lost pings & Questions [F2, F3] =================

Result<LostPingChange> AppModel::addLost(int sessionId) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (!Permissions::canAddInput(viewer, rawRes.value)) {
        return viewer.role != CourseRole::Student ? Error::NotAStudent : Error::SessionEnded;
    }

    // Cooldown check (I8)
    Result<std::optional<Clock::time_point>> lastTime = db->getLastLostTime(sessionId, currentUser->getId());
    if (!lastTime.isOk()) {
        return Error::StorageError;  // never skip the cooldown on a database error
    }
    if (lastTime.value.has_value()) {
        const auto diffSecs = std::chrono::duration_cast<std::chrono::seconds>(getNow() - *lastTime.value).count();
        if (diffSecs < 5) {
            return Error::LostTooSoon;
        }
    }

    Result<LostPing> pingRes = db->addLostPing(sessionId, currentUser->getId(), getNow());
    if (!pingRes.isOk()) {
        return pingRes.error;
    }

    return LostPingChange{course, rawRes.value, pingRes.value};
}

// Why a change to an item of an ENDED session is refused. The author and
// the educator are told it's frozen (I6). A classmate is told it isn't
// theirs only if they can see it (published, and for a question, questions
// shared); otherwise it "doesn't exist" for them (C5, A2), so ids can't be
// probed.
static Error getEndedRefusal(const Viewer& viewer, const Course& course, const Session& session,
                             bool isAuthor, bool isQuestion) {
    if (isAuthor || viewer.role == CourseRole::Educator) {
        return Error::SessionEnded;
    }
    const bool visible = session.isPublished() && (!isQuestion || course.areQuestionsShared());
    return visible ? Error::NotYourInput : Error::NoSuchInput;
}

Result<LostPingChange> AppModel::deleteLost(int pingId) {
    if (!currentUser || !db) {
        return Error::NotLoggedIn;
    }

    Result<LostPing> pingRes = db->getLostPing(pingId);
    if (!pingRes.isOk()) {
        return Error::NoSuchInput;
    }

    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(pingRes.value.getSessionId(), course, viewer);
    if (!rawRes.isOk()) {
        return Error::NoSuchInput;
    }
    if (!Permissions::canDeleteLost(viewer, rawRes.value, pingRes.value)) {
        if (!rawRes.value.isLive()) {
            return getEndedRefusal(viewer, course, rawRes.value, pingRes.value.isBy(viewer.userId), false);
        }
        // A student can't see a classmate's live ping, so it "doesn't exist"
        // for them (appmodel.h: NoSuchInput if not visible to the user).
        return viewer.role == CourseRole::Educator ? Error::NotYourInput : Error::NoSuchInput;
    }

    if (!db->deleteLostPing(pingId).isOk()) {
        return Error::StorageError;
    }
    return LostPingChange{course, rawRes.value, pingRes.value};
}

Result<QuestionChange> AppModel::addQuestion(int sessionId, const std::string& text) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (!Permissions::canAddInput(viewer, rawRes.value)) {
        return viewer.role != CourseRole::Student ? Error::NotAStudent : Error::SessionEnded;
    }

    const std::string trimmed = Validation::trim(text);
    if (const Error err = Validation::checkQuestion(trimmed); err != Error::None) {
        return err;
    }

    Result<SessionQuestion> qRes = db->addQuestion(sessionId, currentUser->getId(), getNow(), trimmed);
    if (!qRes.isOk()) {
        return qRes.error;
    }

    return QuestionChange{course, rawRes.value, qRes.value, 0, false};
}

Result<QuestionChange> AppModel::editQuestion(int questionId, const std::string& text) {
    if (!currentUser || !db) {
        return Error::NotLoggedIn;
    }

    Result<SessionQuestion> qRes = db->getQuestion(questionId);
    if (!qRes.isOk()) {
        return Error::NoSuchInput;
    }

    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(qRes.value.getSessionId(), course, viewer);
    if (!rawRes.isOk()) {
        return Error::NoSuchInput;
    }
    if (!Permissions::canEditQuestion(viewer, rawRes.value, qRes.value)) {
        if (!rawRes.value.isLive()) {
            return getEndedRefusal(viewer, course, rawRes.value, qRes.value.isBy(viewer.userId), true);
        }
        // Same as deleteLost: a classmate's live question is invisible.
        return viewer.role == CourseRole::Educator ? Error::NotYourInput : Error::NoSuchInput;
    }

    const std::string trimmed = Validation::trim(text);
    if (const Error err = Validation::checkQuestion(trimmed); err != Error::None) {
        return err;
    }

    if (!db->updateQuestionText(questionId, trimmed, getNow()).isOk()) {
        return Error::StorageError;
    }

    Result<SessionQuestion> updated = db->getQuestion(questionId);
    if (!updated.isOk()) {
        return Error::StorageError;
    }
    return QuestionChange{course, rawRes.value, updated.value, 0, false};
}

Result<QuestionChange> AppModel::deleteQuestion(int questionId) {
    if (!currentUser || !db) {
        return Error::NotLoggedIn;
    }

    Result<SessionQuestion> qRes = db->getQuestion(questionId);
    if (!qRes.isOk()) {
        return Error::NoSuchInput;
    }

    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(qRes.value.getSessionId(), course, viewer);
    if (!rawRes.isOk()) {
        return Error::NoSuchInput;
    }

    // Live: only the author (I5). Ended: only the educator, as moderation
    // (I7). Each refusal says which rule stopped it.
    const bool isAuthor = qRes.value.isBy(currentUser->getId());
    if (!Permissions::canDeleteQuestion(viewer, rawRes.value, qRes.value)) {
        if (rawRes.value.isLive()) {
            // The educator moderates after the session; a classmate's live
            // question is invisible to a student.
            return viewer.role == CourseRole::Educator ? Error::SessionNotEnded : Error::NoSuchInput;
        }
        return getEndedRefusal(viewer, course, rawRes.value, isAuthor, true);  // I6 freeze
    }
    const bool byModerator = !rawRes.value.isLive();

    if (!db->deleteQuestion(questionId).isOk()) {
        return Error::StorageError;
    }
    return QuestionChange{course, rawRes.value, qRes.value, 0, byModerator};
}

// ================= Answers [F4] =================

Result<QuestionChange> AppModel::addAnswer(int questionId, const std::string& text) {
    if (!currentUser || !db) {
        return Error::NotLoggedIn;
    }

    Result<SessionQuestion> qRes = db->getQuestion(questionId);
    if (!qRes.isOk()) {
        return Error::NoSuchInput;
    }

    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(qRes.value.getSessionId(), course, viewer);
    if (!rawRes.isOk()) {
        return Error::NoSuchInput;
    }
    if (!Permissions::canAddAnswer(viewer, rawRes.value)) {
        return rawRes.value.isLive() ? Error::SessionNotEnded : Error::SessionNotPublished;
    }
    // Only a question this user can see can be answered (A2). Visibility
    // decides, on a copy holding just this question, so the rule (questions
    // shared or not) lives in one place.
    {
        Session only = rawRes.value;
        only.setLostPings({});
        only.setQuestions({qRes.value});
        Visibility::redactSession(only, course, viewer);
        if (only.getQuestions().empty()) {
            return Error::NoSuchInput;
        }
    }

    const std::string trimmed = Validation::trim(text);
    if (const Error err = Validation::checkAnswer(trimmed); err != Error::None) {
        return err;
    }

    Result<Answer> aRes = db->addAnswer(questionId, currentUser->getId(), trimmed, getNow());
    if (!aRes.isOk()) {
        return aRes.error;
    }

    Result<SessionQuestion> updatedQ = db->getQuestion(questionId);
    if (!updatedQ.isOk()) {
        return Error::StorageError;
    }
    return QuestionChange{course, rawRes.value, updatedQ.value, aRes.value.getId(), false};
}

Result<QuestionChange> AppModel::editAnswer(int answerId, const std::string& text) {
    if (!currentUser || !db) {
        return Error::NotLoggedIn;
    }

    Result<Answer> aRes = db->getAnswer(answerId);
    if (!aRes.isOk()) {
        return Error::NoSuchAnswer;
    }

    Result<SessionQuestion> qRes = db->getQuestion(aRes.value.getQuestionId());
    if (!qRes.isOk()) {
        return Error::NoSuchAnswer;
    }

    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(qRes.value.getSessionId(), course, viewer);
    if (!rawRes.isOk()) {
        return Error::NoSuchAnswer;
    }
    if (!Permissions::canEditAnswer(viewer, aRes.value)) {
        return Error::NotYourAnswer;
    }

    const std::string trimmed = Validation::trim(text);
    if (const Error err = Validation::checkAnswer(trimmed); err != Error::None) {
        return err;
    }

    if (!db->updateAnswerText(answerId, trimmed, getNow()).isOk()) {
        return Error::StorageError;
    }

    Result<SessionQuestion> updatedQ = db->getQuestion(qRes.value.getId());
    if (!updatedQ.isOk()) {
        return Error::StorageError;
    }
    return QuestionChange{course, rawRes.value, updatedQ.value, answerId, false};
}

Result<QuestionChange> AppModel::deleteAnswer(int answerId) {
    if (!currentUser || !db) {
        return Error::NotLoggedIn;
    }

    Result<Answer> aRes = db->getAnswer(answerId);
    if (!aRes.isOk()) {
        return Error::NoSuchAnswer;
    }

    Result<SessionQuestion> qRes = db->getQuestion(aRes.value.getQuestionId());
    if (!qRes.isOk()) {
        return Error::NoSuchAnswer;
    }

    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(qRes.value.getSessionId(), course, viewer);
    if (!rawRes.isOk()) {
        return Error::NoSuchAnswer;
    }
    // The author can delete their own answer; the educator can remove any
    // answer in their course (A7, moderation, like I7 for questions).
    if (!Permissions::canDeleteAnswer(viewer, aRes.value)) {
        return Error::NotYourAnswer;
    }
    const bool byModerator = !aRes.value.isBy(viewer.userId);

    if (!db->deleteAnswer(answerId).isOk()) {
        return Error::StorageError;
    }

    Result<SessionQuestion> updatedQ = db->getQuestion(qRes.value.getId());
    if (!updatedQ.isOk()) {
        return Error::StorageError;
    }
    return QuestionChange{course, rawRes.value, updatedQ.value, answerId, byModerator};
}

// ================= Recording [F2, F5] =================

Result<Session> AppModel::attachRecording(int sessionId, const std::string& url,
                                          long long offsetSeconds, RecordingSource source) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (!Permissions::canManageRecording(viewer, rawRes.value)) {
        return viewer.role != CourseRole::Educator ? Error::NotCourseEducator : Error::SessionNotEnded;
    }

    const std::string trimmed = Validation::trim(url);
    if (const Error err = Validation::checkRecordingUrl(trimmed); err != Error::None) {
        return err;
    }
    if (const Error err = Validation::checkRecordingOffset(offsetSeconds); err != Error::None) {
        return err;
    }

    Recording rec(trimmed, offsetSeconds, source);
    if (!db->setRecording(sessionId, rec).isOk()) {
        return Error::StorageError;
    }
    return db->getSession(sessionId);
}

Result<Session> AppModel::removeRecording(int sessionId) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (!Permissions::canManageRecording(viewer, rawRes.value)) {
        return viewer.role != CourseRole::Educator ? Error::NotCourseEducator : Error::SessionNotEnded;
    }

    if (!db->clearRecording(sessionId).isOk()) {
        return Error::StorageError;
    }
    return db->getSession(sessionId);
}

Result<Session> AppModel::matchRecording(int sessionId, const std::vector<RecordingCandidate>& candidates) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (!Permissions::canManageRecording(viewer, rawRes.value)) {
        return viewer.role != CourseRole::Educator ? Error::NotCourseEducator : Error::SessionNotEnded;
    }

    const int index = Recording::findOverlapping(candidates, rawRes.value.getStartTime(), rawRes.value.getEndTime());
    if (index < 0) {
        return Error::RecordingNotReady;
    }

    // The candidates come from the app, so they get the same checks as a
    // pasted link (R2): no javascript: or other non-http(s) URLs saved.
    const auto& candidate = candidates[index];
    const std::string url = Validation::trim(candidate.viewerUrl);
    if (const Error err = Validation::checkRecordingUrl(url); err != Error::None) {
        return err;
    }
    const long long offset = Recording::getOffsetBetween(candidate.start, rawRes.value.getStartTime());
    // More than a day apart means Panopto sent bad data: say so, and let
    // the educator paste the link instead (R3).
    if (const Error err = Validation::checkRecordingOffset(offset); err != Error::None) {
        return err;
    }
    Recording rec(url, offset, RecordingSource::Panopto);
    if (!db->setRecording(sessionId, rec).isOk()) {
        return Error::StorageError;
    }
    return db->getSession(sessionId);
}

// ================= Private helpers =================

Result<Session> AppModel::loadSessionDetails(int sessionId, Course& course, Viewer& viewer) const {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }

    Result<Session> sRes = db->getSession(sessionId);
    if (!sRes.isOk()) {
        return Error::NoSuchSession;
    }

    const int courseId = sRes.value.getCourseId();
    const CourseRole role = db->getRole(courseId, currentUser->getId());
    if (role == CourseRole::None) {
        return Error::NoSuchSession; // User cannot see this session exists
    }

    Result<Course> cRes = db->getCourse(courseId, currentUser->getId());
    if (!cRes.isOk()) {
        return Error::NoSuchSession;
    }

    course = cRes.value;
    viewer = Viewer{currentUser->getId(), role};
    return sRes;
}

std::string AppModel::makeFreeCourseCode() const {
    for (int attempts = 0; attempts < 1000; ++attempts) {
        std::string code = JoinCodes::generateCourseCode();
        if (!db->findCourseByJoinCode(code, 0).isOk()) {
            return code;
        }
    }
    return JoinCodes::generateCourseCode();
}

std::string AppModel::makeFreeSessionCode() const {
    for (int attempts = 0; attempts < 1000; ++attempts) {
        std::string code = JoinCodes::generateSessionCode();
        if (!db->findLiveSessionByJoinCode(code).isOk()) {
            return code;
        }
    }
    return JoinCodes::generateSessionCode();
}