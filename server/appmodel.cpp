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

    if (db->countLiveSessionsHostedBy(currentUser->getId()) > 0) {
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

    if (found.value.getRole() == CourseRole::None) {
        db->addStudent(found.value.getId(), currentUser->getId(), getNow());
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
    if (cRes.value.getRole() != CourseRole::Educator) {
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
    if (cRes.value.getRole() != CourseRole::Educator) {
        return Error::NotCourseEducator;
    }

    if (!db->deleteCourse(courseId).isOk()) {
        return Error::StorageError;
    }
    return Error::None;
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
    if (cRes.value.getRole() != CourseRole::Educator) {
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
        db->addStudent(sRes.value.getCourseId(), currentUser->getId(), getNow());
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

    if (viewer.role == CourseRole::Student && rawRes.value.isInReview()) {
        return Error::SessionNotPublished;
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
    if (viewer.role != CourseRole::Educator) {
        return Error::NotCourseEducator;
    }
    if (rawRes.value.isEnded()) {
        return Error::SessionEnded;
    }

    if (!db->setSessionEnd(sessionId, getNow()).isOk()) {
        return Error::StorageError;
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
    if (viewer.role != CourseRole::Educator) {
        return Error::NotCourseEducator;
    }
    if (rawRes.value.isLive()) {
        return Error::SessionNotEnded;
    }
    if (rawRes.value.isPublished()) {
        return Error::SessionAlreadyPublished;
    }

    if (!db->setSessionPublished(sessionId, getNow()).isOk()) {
        return Error::StorageError;
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
    if (viewer.role != CourseRole::Educator) {
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
    if (viewer.role != CourseRole::Educator) {
        return Error::NotCourseEducator;
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
    if (viewer.role != CourseRole::Student) {
        return Error::NotAStudent;
    }
    if (!rawRes.value.isLive()) {
        return Error::SessionEnded;
    }

    // Cooldown check (I8)
    Result<std::optional<Clock::time_point>> lastTime = db->getLastLostTime(sessionId, currentUser->getId());
    if (lastTime.isOk() && lastTime.value.has_value()) {
        const auto diffSecs = std::chrono::duration_cast<std::chrono::seconds>(getNow() - *lastTime.value).count();
        if (diffSecs < 5) {
            return Error::LostTooSoon;
        }
    }

    Result<LostPing> pingRes = db->addLostPing(sessionId, currentUser->getId(), getNow());
    if (!pingRes.isOk()) {
        return pingRes.error;
    }

    LostPing ping = pingRes.value;
    ping.setAuthor(currentUser->getId(), currentUser->getName());
    return LostPingChange{course, rawRes.value, ping};
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
    if (!rawRes.value.isLive()) {
        return Error::SessionEnded;
    }
    if (pingRes.value.getAuthorId() != currentUser->getId()) {
        return Error::NotYourInput;
    }

    LostPing rawBefore = pingRes.value;
    rawBefore.setAuthor(currentUser->getId(), currentUser->getName());
    if (!db->deleteLostPing(pingId).isOk()) {
        return Error::StorageError;
    }
    return LostPingChange{course, rawRes.value, rawBefore};
}

Result<QuestionChange> AppModel::addQuestion(int sessionId, const std::string& text) {
    Course course;
    Viewer viewer;
    Result<Session> rawRes = loadSessionDetails(sessionId, course, viewer);
    if (!rawRes.isOk()) {
        return rawRes.error;
    }
    if (viewer.role != CourseRole::Student) {
        return Error::NotAStudent;
    }
    if (!rawRes.value.isLive()) {
        return Error::SessionEnded;
    }

    const std::string trimmed = Validation::trim(text);
    if (const Error err = Validation::checkQuestion(trimmed); err != Error::None) {
        return err;
    }

    Result<SessionQuestion> qRes = db->addQuestion(sessionId, currentUser->getId(), getNow(), trimmed);
    if (!qRes.isOk()) {
        return qRes.error;
    }

    SessionQuestion q = qRes.value;
    q.setAuthor(currentUser->getId(), currentUser->getName());
    return QuestionChange{course, rawRes.value, q, 0, false};
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
    if (!rawRes.value.isLive()) {
        return Error::SessionEnded;
    }
    if (qRes.value.getAuthorId() != currentUser->getId()) {
        return Error::NotYourInput;
    }

    const std::string trimmed = Validation::trim(text);
    if (const Error err = Validation::checkQuestion(trimmed); err != Error::None) {
        return err;
    }

    if (!db->updateQuestionText(questionId, trimmed, getNow()).isOk()) {
        return Error::StorageError;
    }

    SessionQuestion updated = db->getQuestion(questionId).value;
    updated.setAuthor(currentUser->getId(), currentUser->getName());
    return QuestionChange{course, rawRes.value, updated, 0, false};
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

    bool byModerator = false;
    if (rawRes.value.isLive()) {
        if (qRes.value.getAuthorId() != currentUser->getId()) {
            return Error::NotYourInput;
        }
    } else {
        if (viewer.role != CourseRole::Educator) {
            return Error::NotCourseEducator;
        }
        byModerator = true;
    }

    SessionQuestion before = qRes.value;
    if (before.getAuthorId() > 0) {
        const auto uRes = db->getUser(before.getAuthorId());
        if (uRes.isOk()) {
            before.setAuthor(before.getAuthorId(), uRes.value.getName());
        }
    }

    if (!db->deleteQuestion(questionId).isOk()) {
        return Error::StorageError;
    }
    return QuestionChange{course, rawRes.value, before, 0, byModerator};
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
    if (rawRes.value.isLive()) {
        return Error::SessionNotEnded;
    }
    if (viewer.role == CourseRole::Student && !rawRes.value.isPublished()) {
        return Error::SessionNotPublished;
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
    for (auto& a : const_cast<std::vector<Answer>&>(updatedQ.value.getAnswers())) {
        a.setByEducator(course.isEducator(a.getAuthorId()));
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
    if (aRes.value.getAuthorId() != currentUser->getId()) {
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
    for (auto& a : const_cast<std::vector<Answer>&>(updatedQ.value.getAnswers())) {
        a.setByEducator(course.isEducator(a.getAuthorId()));
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
    if (aRes.value.getAuthorId() != currentUser->getId()) {
        return Error::NotYourAnswer;
    }

    if (!db->deleteAnswer(answerId).isOk()) {
        return Error::StorageError;
    }

    Result<SessionQuestion> updatedQ = db->getQuestion(qRes.value.getId());
    for (auto& a : const_cast<std::vector<Answer>&>(updatedQ.value.getAnswers())) {
        a.setByEducator(course.isEducator(a.getAuthorId()));
    }
    return QuestionChange{course, rawRes.value, updatedQ.value, answerId, false};
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
    if (viewer.role != CourseRole::Educator) {
        return Error::NotCourseEducator;
    }
    if (rawRes.value.isLive()) {
        return Error::SessionNotEnded;
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
    if (viewer.role != CourseRole::Educator) {
        return Error::NotCourseEducator;
    }
    if (rawRes.value.isLive()) {
        return Error::SessionNotEnded;
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
    if (viewer.role != CourseRole::Educator) {
        return Error::NotCourseEducator;
    }
    if (rawRes.value.isLive()) {
        return Error::SessionNotEnded;
    }

    const int index = Recording::findOverlapping(candidates, rawRes.value.getStartTime(), rawRes.value.getEndTime());
    if (index < 0) {
        return Error::RecordingNotReady;
    }

    const auto& candidate = candidates[index];
    const long long offset = Recording::getOffsetBetween(candidate.start, rawRes.value.getStartTime());
    Recording rec(candidate.viewerUrl, offset, RecordingSource::Panopto);
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