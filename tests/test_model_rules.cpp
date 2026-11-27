// AppModel rules found in the Nov 25 review, one test case per rule.
// Several AppModels on one in-memory Database = several users at once.

#include "doctest.h"

#include <chrono>
#include <string>

#include "appmodel.h"
#include "database.h"
#include "error.h"
#include "passwordhasher.h"
#include "recording.h"

namespace {

using Clock = AppModel::Clock;

// An educator (prof) and two students (ana, bob) in one course, with a live
// session. `now` moves forward by hand.
struct Class {
    Database db{":memory:"};
    long long seconds = 1700000000;
    AppModel::NowFunction clock = [this] { return Clock::time_point(std::chrono::seconds(seconds)); };
    AppModel prof{&db, clock, PasswordHasher::kTestIterations};
    AppModel ana{&db, clock, PasswordHasher::kTestIterations};
    AppModel bob{&db, clock, PasswordHasher::kTestIterations};
    int courseId = 0;
    int sessionId = 0;

    explicit Class(bool questionsShared = true) {
        REQUIRE(db.open().isOk());
        REQUIRE(prof.registerUser("prof", "password123", "prof@scu.edu").isOk());
        REQUIRE(ana.registerUser("ana", "password123", "ana@scu.edu").isOk());
        REQUIRE(bob.registerUser("bob", "password123", "bob@scu.edu").isOk());
        const auto course = prof.createCourse("CSCI 187", false, questionsShared);
        REQUIRE(course.isOk());
        courseId = course.value.getId();
        const auto started = prof.startSession(courseId, "Lecture 1");
        REQUIRE(started.isOk());
        sessionId = started.value.session.getId();
        const std::string code = db.getSession(sessionId).value.getJoinCode();
        REQUIRE(ana.joinSession(code).isOk());
        REQUIRE(bob.joinSession(code).isOk());
    }

    int ask(AppModel& who, const std::string& text) {
        const auto q = who.addQuestion(sessionId, text);
        REQUIRE(q.isOk());
        return q.value.question.getId();
    }
    void endAndPublish() {
        seconds += 3600;
        REQUIRE(prof.endSession(sessionId).isOk());
        REQUIRE(prof.publishSession(sessionId).isOk());
    }
};

}  // namespace

TEST_CASE("A2: a student cannot answer a question they cannot see (questions not shared)") {
    Class c(/*questionsShared=*/false);
    const int anasQuestion = c.ask(c.ana, "private question");
    c.endAndPublish();
    CHECK(c.bob.addAnswer(anasQuestion, "bob was here").error == Error::NoSuchInput);
    // Ana herself and the educator still can.
    CHECK(c.ana.addAnswer(anasQuestion, "never mind, got it").isOk());
    CHECK(c.prof.addAnswer(anasQuestion, "see slide 4").isOk());
}

TEST_CASE("A5: educator answers come first and carry names after a reload") {
    Class c;
    const int q = c.ask(c.ana, "why?");
    c.endAndPublish();
    REQUIRE(c.bob.addAnswer(q, "student guess").isOk());
    c.seconds += 60;
    REQUIRE(c.prof.addAnswer(q, "real answer").isOk());

    const auto opened = c.ana.openSession(c.sessionId);
    REQUIRE(opened.isOk());
    REQUIRE(opened.value.session.getQuestions().size() == 1);
    const auto& answers = opened.value.session.getQuestions()[0].getAnswers();
    REQUIRE(answers.size() == 2);
    CHECK(answers[0].getText() == "real answer");
    CHECK(answers[0].isByEducator());
    CHECK(answers[0].getAuthorName() == "prof");
    CHECK(answers[1].getAuthorName() == "bob");
}

TEST_CASE("I5-I7: deleteQuestion says which rule stopped it") {
    Class c;
    const int q = c.ask(c.ana, "why?");
    // Live: the educator must wait for the end; a classmate can't even see it.
    CHECK(c.prof.deleteQuestion(q).error == Error::SessionNotEnded);
    CHECK(c.bob.deleteQuestion(q).error == Error::NoSuchInput);
    c.seconds += 3600;
    REQUIRE(c.prof.endSession(c.sessionId).isOk());
    // Ended: the author is frozen out; the educator moderates.
    CHECK(c.ana.deleteQuestion(q).error == Error::SessionEnded);
    const auto removed = c.prof.deleteQuestion(q);
    REQUIRE(removed.isOk());
    CHECK(removed.value.byModerator);
}

TEST_CASE("I5: a classmate's live items look like they don't exist") {
    Class c;
    const int q = c.ask(c.ana, "why?");
    const auto ping = c.ana.addLost(c.sessionId);
    REQUIRE(ping.isOk());
    CHECK(c.bob.deleteLost(ping.value.ping.getId()).error == Error::NoSuchInput);
    CHECK(c.bob.editQuestion(q, "hacked").error == Error::NoSuchInput);
    // Same answer as for an id that really doesn't exist.
    CHECK(c.bob.deleteLost(999999).error == Error::NoSuchInput);
    // The author still can.
    CHECK(c.ana.editQuestion(q, "why n log n?").isOk());
    CHECK(c.ana.deleteLost(ping.value.ping.getId()).isOk());
}

TEST_CASE("R2/R6: auto-match checks the candidate's link like a pasted one") {
    Class c;
    c.seconds += 3600;
    REQUIRE(c.prof.endSession(c.sessionId).isOk());
    RecordingCandidate bad;
    bad.viewerUrl = "javascript:alert(1)";
    bad.start = Clock::time_point(std::chrono::seconds(1700000000 - 120));
    bad.durationSeconds = 4000;
    CHECK(c.prof.matchRecording(c.sessionId, {bad}).error == Error::RecordingUrlInvalid);

    RecordingCandidate good = bad;
    good.viewerUrl = "https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=abc";
    const auto matched = c.prof.matchRecording(c.sessionId, {good});
    REQUIRE(matched.isOk());
    REQUIRE(matched.value.getRecording() != nullptr);
    CHECK(matched.value.getRecording()->getOffsetSeconds() == 120);
}

TEST_CASE("S9: a live session cannot be deleted until it is ended") {
    Class c;
    CHECK(c.prof.deleteSession(c.sessionId).error == Error::SessionNotEnded);
    CHECK(c.db.getSession(c.sessionId).isOk());
    c.seconds += 60;
    REQUIRE(c.prof.endSession(c.sessionId).isOk());
    CHECK(c.prof.deleteSession(c.sessionId).isOk());
    CHECK_FALSE(c.db.getSession(c.sessionId).isOk());
}

TEST_CASE("R3/R6: an auto-match more than a day off is an error, not a silent 0") {
    Class c;
    c.seconds += 3600;
    REQUIRE(c.prof.endSession(c.sessionId).isOk());
    // A "recording" that started two days before class and runs past it:
    // it overlaps, but the offset would be about +48 h.
    RecordingCandidate far;
    far.viewerUrl = "https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=abc";
    far.start = Clock::time_point(std::chrono::seconds(1700000000 - 2 * 24 * 3600));
    far.durationSeconds = 3 * 24 * 3600;
    CHECK(c.prof.matchRecording(c.sessionId, {far}).error == Error::RecordingOffsetInvalid);
    CHECK(c.db.getSession(c.sessionId).value.getRecording() == nullptr);
}

TEST_CASE("A7: the educator can remove a student's answer; classmates cannot") {
    Class c;
    const int q = c.ask(c.ana, "why?");
    c.endAndPublish();
    const auto rude = c.bob.addAnswer(q, "rude answer");
    REQUIRE(rude.isOk());
    const int answerId = rude.value.answerId;
    CHECK(c.ana.deleteAnswer(answerId).error == Error::NotYourAnswer);
    const auto removed = c.prof.deleteAnswer(answerId);
    REQUIRE(removed.isOk());
    CHECK(removed.value.byModerator);
    CHECK(removed.value.question.getAnswers().empty());
}

TEST_CASE("R6: only the educator links a Panopto folder, and only a real folder id") {
    Class c;
    const std::string folder = "1b2c3d4e-0000-4a5b-8c9d-0123456789ab";
    CHECK(c.ana.linkPanoptoFolder(c.courseId, folder).error == Error::NotCourseEducator);
    CHECK(c.prof.linkPanoptoFolder(c.courseId, "https://evil.example/").error == Error::PanoptoFolderInvalid);
    const auto linked = c.prof.linkPanoptoFolder(c.courseId, "  " + folder + " ");
    REQUIRE(linked.isOk());
    CHECK(linked.value.getPanoptoFolderId() == folder);
    // Students don't get the folder id (or the join code).
    const auto asStudent = c.ana.getCourse(c.courseId);
    REQUIRE(asStudent.isOk());
    CHECK_FALSE(asStudent.value.course.hasPanoptoFolder());
    // Unlink.
    CHECK_FALSE(c.prof.linkPanoptoFolder(c.courseId, "").value.hasPanoptoFolder());
}

TEST_CASE("I8: deleting a press doesn't reset the 5-second cooldown") {
    Class c;
    const auto first = c.ana.addLost(c.sessionId);
    REQUIRE(first.isOk());
    REQUIRE(c.ana.deleteLost(first.value.ping.getId()).isOk());
    c.seconds += 1;
    CHECK(c.ana.addLost(c.sessionId).error == Error::LostTooSoon);
    c.seconds += 5;
    CHECK(c.ana.addLost(c.sessionId).isOk());
}

TEST_CASE("C5/A2: after the end, a classmate can't probe for hidden items") {
    Class c(/*questionsShared=*/false);
    const int q = c.ask(c.ana, "private");
    const auto ping = c.ana.addLost(c.sessionId);
    REQUIRE(ping.isOk());
    c.seconds += 3600;
    REQUIRE(c.prof.endSession(c.sessionId).isOk());
    // In review: nothing of the session is visible to bob.
    CHECK(c.bob.deleteQuestion(q).error == Error::NoSuchInput);
    CHECK(c.bob.editQuestion(q, "x").error == Error::NoSuchInput);
    CHECK(c.bob.deleteLost(ping.value.ping.getId()).error == Error::NoSuchInput);
    REQUIRE(c.prof.publishSession(c.sessionId).isOk());
    // Published, questions not shared: the question is still hidden, the
    // (nameless) press is visible but not his.
    CHECK(c.bob.deleteQuestion(q).error == Error::NoSuchInput);
    CHECK(c.bob.deleteLost(ping.value.ping.getId()).error == Error::NotYourInput);
    // The author is told it's frozen.
    CHECK(c.ana.editQuestion(q, "x").error == Error::SessionEnded);
}

TEST_CASE("C8: students don't get the list of classmates' ids") {
    Class c;
    const auto asStudent = c.ana.getCourse(c.courseId);
    REQUIRE(asStudent.isOk());
    CHECK(asStudent.value.course.getStudentIds().empty());
    const auto asEducator = c.prof.getCourse(c.courseId);
    REQUIRE(asEducator.isOk());
    CHECK(asEducator.value.course.getStudentIds().size() == 2);
}

TEST_CASE("R6: Find recording picks the recording that overlaps the most") {
    using std::chrono::seconds;
    const Clock::time_point ten(seconds(1700000000));
    RecordingCandidate earlier;  // 9:00 with no length: counted as 9:00-10:10
    earlier.viewerUrl = "https://scu.hosted.panopto.com/v?id=a";
    earlier.start = ten - seconds(3600);
    RecordingCandidate real;  // 10:00-11:00
    real.viewerUrl = "https://scu.hosted.panopto.com/v?id=b";
    real.start = ten;
    real.durationSeconds = 3600;
    CHECK(Recording::findOverlapping({earlier, real}, ten, ten + seconds(3600)) == 1);
    CHECK(Recording::findOverlapping({real, earlier}, ten, ten + seconds(3600)) == 0);
    CHECK(Recording::findOverlapping({earlier}, ten + seconds(7200), ten + seconds(9000)) == -1);
}
