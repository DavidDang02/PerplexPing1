#include "doctest.h"

#include <chrono>
#include <string>
#include <vector>

#include "appmodel.h"
#include "database.h"
#include "error.h"
#include "passwordhasher.h"

namespace {
AppModel::Clock::time_point makeTime(long long secondsFromEpoch) {
    return AppModel::Clock::time_point(std::chrono::seconds(secondsFromEpoch));
}
}

TEST_CASE("AppModel Server Accounts: registration and login rules") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());

    long long fixedTime = 100000;
    auto clock = [&]() { return makeTime(fixedTime); };
    AppModel model(&db, clock, PasswordHasher::kTestIterations);

    // Edge cases: validation failure order (name -> password -> email)
    CHECK(model.registerUser("ab", "pass1234", "a@scu.edu").error == Error::NameTooShort);
    CHECK(model.registerUser("valid_name", "short", "a@scu.edu").error == Error::PasswordTooShort);
    CHECK(model.registerUser("valid_name", "pass1234", "invalid_email").error == Error::EmailInvalid);
    CHECK(model.registerUser("valid_name", "pass1234", "user@gmail.com").error == Error::EmailNotSchool);

    // Expected: valid registration logs in
    const auto reg = model.registerUser("alice", "password123", "alice@scu.edu");
    REQUIRE(reg.isOk());
    CHECK(reg.value.getName() == "alice");
    CHECK(model.isLoggedIn());
    CHECK(model.getCurrentUser()->getId() == reg.value.getId());

    // Duplicate username / email
    model.logOut();
    CHECK(model.registerUser("Alice", "otherpass1", "other@scu.edu").error == Error::NameTaken);
    CHECK(model.registerUser("bob", "otherpass1", "alice@scu.edu").error == Error::EmailTaken);

    // Login checks
    CHECK(model.logIn("", "password123").error == Error::EmptyName);
    CHECK(model.logIn("alice", "").error == Error::EmptyPassword);
    CHECK(model.logIn("alice", "wrongpassword").error == Error::WrongLogin);
    CHECK(model.logIn("nonexistent", "password123").error == Error::WrongLogin);

    // Case-insensitive username login
    const auto login = model.logIn("ALICE", "password123");
    REQUIRE(login.isOk());
    CHECK(login.value.getName() == "alice");
}

TEST_CASE("AppModel Server Accounts: password change, email update, and account deletion") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());

    AppModel model(&db, []() { return makeTime(1000); }, PasswordHasher::kTestIterations);
    REQUIRE(model.registerUser("bob", "old_password12", "bob@scu.edu").isOk());

    // Change password
    CHECK(model.changePassword("wrong_old", "new_password12") == Error::WrongPassword);
    CHECK(model.changePassword("old_password12", "short") == Error::PasswordTooShort);
    CHECK(model.changePassword("old_password12", "new_password12") == Error::None);

    model.logOut();
    CHECK(model.logIn("bob", "old_password12").error == Error::WrongLogin);
    REQUIRE(model.logIn("bob", "new_password12").isOk());

    // Update email
    CHECK(model.updateEmail("bad_email").error == Error::EmailInvalid);
    CHECK(model.updateEmail("bob2@scu.edu").isOk());
    CHECK(model.getCurrentUser()->getEmail() == "bob2@scu.edu");

    // Delete account
    CHECK(model.deleteAccount("wrong_pass") == Error::WrongPassword);
    CHECK(model.deleteAccount("new_password12") == Error::None);
    CHECK_FALSE(model.isLoggedIn());
    CHECK(model.logIn("bob", "new_password12").error == Error::WrongLogin);
}

TEST_CASE("AppModel Server Courses: creation, role checking, joining and toggles") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());

    AppModel educator(&db, []() { return makeTime(1000); }, PasswordHasher::kTestIterations);
    AppModel student(&db, []() { return makeTime(1000); }, PasswordHasher::kTestIterations);

    REQUIRE(educator.registerUser("prof_smith", "password123", "smith@scu.edu").isOk());
    REQUIRE(student.registerUser("student_ann", "password123", "ann@scu.edu").isOk());

    // Educator creates course
    const auto courseRes = educator.createCourse("CSEN 187", false, true);
    REQUIRE(courseRes.isOk());
    const Course course = courseRes.value;
    CHECK(course.getEducatorId() == educator.getCurrentUser()->getId());
    CHECK(course.getRole() == CourseRole::Educator);
    CHECK_FALSE(course.getJoinCode().empty());

    // Student joins with join code
    CHECK(student.joinCourse("INVALID").error == Error::NoSuchCourseCode);
    const auto joinRes = student.joinCourse(course.getJoinCode());
    REQUIRE(joinRes.isOk());
    CHECK(joinRes.value.getRole() == CourseRole::Student);
    CHECK(joinRes.value.getJoinCode().empty()); // Redacted for students

    // Course listing
    const auto studentCourses = student.getCourses();
    REQUIRE(studentCourses.isOk());
    REQUIRE(studentCourses.value.size() == 1);
    CHECK(studentCourses.value[0].getJoinCode().empty());

    // Update course toggles
    CHECK(student.updateCourse(course.getId(), "Hacked", true, false).error == Error::NotCourseEducator);
    const auto updatedCourse = educator.updateCourse(course.getId(), "CSEN 187 Design", true, false);
    REQUIRE(updatedCourse.isOk());
    CHECK(updatedCourse.value.getName() == "CSEN 187 Design");
    CHECK(updatedCourse.value.isAnonymous());
    CHECK_FALSE(updatedCourse.value.areQuestionsShared());
}

TEST_CASE("AppModel Server Sessions: lifecycle live -> in_review -> published") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());

    long long currentTime = 20000;
    auto fakeClock = [&]() { return makeTime(currentTime); };

    AppModel educator(&db, fakeClock, PasswordHasher::kTestIterations);
    AppModel student(&db, fakeClock, PasswordHasher::kTestIterations);

    REQUIRE(educator.registerUser("prof", "password123", "prof@scu.edu").isOk());
    REQUIRE(student.registerUser("stud", "password123", "stud@scu.edu").isOk());

    const auto cRes = educator.createCourse("Software Architecture", false, true);
    const int courseId = cRes.value.getId();
    REQUIRE(student.joinCourse(cRes.value.getJoinCode()).isOk());

    // Students cannot start sessions
    CHECK(student.startSession(courseId, "Lecture 1").error == Error::NotCourseEducator);

    // Educator starts session
    const auto sessionRes = educator.startSession(courseId, "Lecture 1");
    REQUIRE(sessionRes.isOk());
    const int sessionId = sessionRes.value.session.getId();
    CHECK(sessionRes.value.session.isLive());

    // Cannot start a second live session in the same course (S3)
    CHECK(educator.startSession(courseId, "Lecture 2").error == Error::CourseHasLiveSession);

    // Student joins via session code
    const auto joinRes = student.joinSession(sessionRes.value.session.getJoinCode());
    REQUIRE(joinRes.isOk());
    CHECK(joinRes.value.session.getId() == sessionId);

    // End session moves to in_review
    currentTime += 3600;
    CHECK(student.endSession(sessionId).error == Error::NotCourseEducator);
    const auto endedRes = educator.endSession(sessionId);
    REQUIRE(endedRes.isOk());
    CHECK(endedRes.value.isInReview());

    // In review: student cannot open session (S12)
    CHECK(student.openSession(sessionId).error == Error::SessionNotPublished);
    CHECK(educator.openSession(sessionId).isOk());

    // Educator publishes session
    currentTime += 600;
    CHECK(educator.publishSession(sessionId).isOk());
    CHECK(educator.publishSession(sessionId).error == Error::SessionAlreadyPublished);

    // Published: student can now open session
    CHECK(student.openSession(sessionId).isOk());
}

TEST_CASE("AppModel Server Inputs: lost pings, questions, moderation, and answers") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());

    long long currentTime = 50000;
    auto fakeClock = [&]() { return makeTime(currentTime); };

    AppModel educator(&db, fakeClock, PasswordHasher::kTestIterations);
    AppModel student(&db, fakeClock, PasswordHasher::kTestIterations);

    REQUIRE(educator.registerUser("educator1", "password123", "ed@scu.edu").isOk());
    REQUIRE(student.registerUser("student1", "password123", "st@scu.edu").isOk());

    const auto cRes = educator.createCourse("Databases", false, true);
    REQUIRE(student.joinCourse(cRes.value.getJoinCode()).isOk());

    const auto sRes = educator.startSession(cRes.value.getId(), "Lecture on SQL");
    const int sessionId = sRes.value.session.getId();

    // Educator cannot send inputs (I1)
    CHECK(educator.addLost(sessionId).error == Error::NotAStudent);
    CHECK(educator.addQuestion(sessionId, "What is SQL?").error == Error::NotAStudent);

    // Student sends lost ping and question
    const auto lostRes = student.addLost(sessionId);
    REQUIRE(lostRes.isOk());
    const int pingId = lostRes.value.ping.getId();

    // Cooldown on lost ping (I8)
    CHECK(student.addLost(sessionId).error == Error::LostTooSoon);
    currentTime += 6;
    CHECK(student.addLost(sessionId).isOk());

    // Delete own lost ping
    CHECK(student.deleteLost(pingId).isOk());

    // Add and edit question
    const auto qRes = student.addQuestion(sessionId, "Explain B-Trees");
    REQUIRE(qRes.isOk());
    const int questionId = qRes.value.question.getId();

    currentTime += 10;
    const auto editQ = student.editQuestion(questionId, "Explain B+ Trees");
    REQUIRE(editQ.isOk());
    CHECK(editQ.value.question.getText() == "Explain B+ Trees");
    CHECK(editQ.value.question.isEdited());

    // Educator cannot answer while live (A1)
    CHECK(educator.addAnswer(questionId, "A B+ tree stores values in leaves").error == Error::SessionNotEnded);

    // End session
    currentTime += 1800;
    REQUIRE(educator.endSession(sessionId).isOk());

    // No inputs allowed after session end (I2, I6)
    CHECK(student.addLost(sessionId).error == Error::SessionEnded);
    CHECK(student.addQuestion(sessionId, "New question?").error == Error::SessionEnded);
    CHECK(student.editQuestion(questionId, "Another edit").error == Error::SessionEnded);

    // In-review: educator can answer, student cannot answer yet
    CHECK(student.addAnswer(questionId, "My answer").error == Error::SessionNotPublished);
    const auto ansRes = educator.addAnswer(questionId, "B+ trees keep keys in nodes and records in leaves.");
    REQUIRE(ansRes.isOk());
    const int answerId = ansRes.value.answerId;

    // Publish session
    REQUIRE(educator.publishSession(sessionId).isOk());

    // Student can answer published session
    const auto studentAns = student.addAnswer(questionId, "Thanks, very clear!");
    REQUIRE(studentAns.isOk());

    // Edit answer
    const auto editAns = student.editAnswer(studentAns.value.answerId, "Thanks, crystal clear!");
    REQUIRE(editAns.isOk());

    // Student cannot delete educator answer
    CHECK(student.deleteAnswer(answerId).error == Error::NotYourAnswer);
    CHECK(student.deleteAnswer(studentAns.value.answerId).isOk());

    // Educator moderation: delete question after session end (I7)
    CHECK(student.deleteQuestion(questionId).error == Error::NotCourseEducator);
    const auto modDel = educator.deleteQuestion(questionId);
    REQUIRE(modDel.isOk());
    CHECK(modDel.value.byModerator);
}

TEST_CASE("AppModel Server Recordings: attach, auto-match, and remove") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());

    long long currentTime = 80000;
    auto fakeClock = [&]() { return makeTime(currentTime); };

    AppModel educator(&db, fakeClock, PasswordHasher::kTestIterations);
    REQUIRE(educator.registerUser("dr_jones", "password123", "jones@scu.edu").isOk());

    const auto cRes = educator.createCourse("Algorithms", false, true);
    const auto sRes = educator.startSession(cRes.value.getId(), "Sorting");
    const int sessionId = sRes.value.session.getId();

    // Cannot attach recording to live session (R1)
    CHECK(educator.attachRecording(sessionId, "https://scu.hosted.panopto.com/video1", 30, RecordingSource::Manual).error == Error::SessionNotEnded);

    currentTime += 3600;
    REQUIRE(educator.endSession(sessionId).isOk());

    // Validation checks on recording URL and offset
    CHECK(educator.attachRecording(sessionId, "ftp://invalid-url", 0, RecordingSource::Manual).error == Error::RecordingUrlInvalid);
    CHECK(educator.attachRecording(sessionId, "https://scu.hosted.panopto.com/video1", 1000000, RecordingSource::Manual).error == Error::RecordingOffsetInvalid);

    // Valid attach
    const auto attached = educator.attachRecording(sessionId, "https://scu.hosted.panopto.com/video1", -15, RecordingSource::Manual);
    REQUIRE(attached.isOk());
    REQUIRE(attached.value.getRecording() != nullptr);
    CHECK(attached.value.getRecording()->getViewerUrl() == "https://scu.hosted.panopto.com/video1");
    CHECK(attached.value.getRecording()->getOffsetSeconds() == -15);

    // Auto-match candidates
    std::vector<RecordingCandidate> candidates;
    RecordingCandidate match;
    match.viewerUrl = "https://scu.hosted.panopto.com/panopto_auto";
    match.start = makeTime(79900); // overlaps session [80000, 83600]
    match.durationSeconds = 4500;
    candidates.push_back(match);

    const auto matched = educator.matchRecording(sessionId, candidates);
    REQUIRE(matched.isOk());
    REQUIRE(matched.value.getRecording() != nullptr);
    CHECK(matched.value.getRecording()->getViewerUrl() == "https://scu.hosted.panopto.com/panopto_auto");
    CHECK(matched.value.getRecording()->getSource() == RecordingSource::Panopto);

    // Remove recording
    const auto removed = educator.removeRecording(sessionId);
    REQUIRE(removed.isOk());
    CHECK(removed.value.getRecording() == nullptr);
}