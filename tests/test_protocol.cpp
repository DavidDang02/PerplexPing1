// Protocol round trips (CONTRACT.md 8): every object the server sends comes
// back the same after toJson -> encode -> decode -> fromJson, so the app
// and the server read each other's messages the same way.

#include "doctest.h"

#include <chrono>

#include "protocol.h"

namespace {

using namespace Protocol;
using Clock = std::chrono::system_clock;

Clock::time_point at(long long ms) {
    return Clock::time_point(std::chrono::milliseconds(ms));
}

// Through text, the way it really travels.
QJsonObject throughWire(const QJsonObject& json) {
    return decode(encode(json));
}

}  // namespace

TEST_CASE("Protocol: user round trip") {
    const User back = userFromJson(throughWire(toJson(User(7, "ana", "ana@scu.edu"))));
    CHECK(back.getId() == 7);
    CHECK(back.getName() == "ana");
    CHECK(back.getEmail() == "ana@scu.edu");
}

TEST_CASE("Protocol: course round trip keeps toggles, role and ids") {
    Course course(3, "CSCI 187");
    course.setJoinCode("K7M2QX");
    course.setAnonymous(true);
    course.setQuestionsShared(false);
    course.setEducatorId(1);
    course.setEducatorName("prof");
    const auto created = std::chrono::system_clock::time_point(std::chrono::seconds(1764000000));
    course.setCreatedTime(created);
    course.setRole(CourseRole::Educator);
    course.setLiveSessionId(9);
    course.setStudentIds({4, 5});
    course.setSessionIds({9, 8});

    const Course back = courseFromJson(throughWire(toJson(course)));
    CHECK(back.getId() == 3);
    CHECK(back.getName() == "CSCI 187");
    CHECK(back.getJoinCode() == "K7M2QX");
    CHECK(back.isAnonymous());
    CHECK_FALSE(back.areQuestionsShared());
    CHECK(back.getEducatorId() == 1);
    CHECK(back.getEducatorName() == "prof");
    CHECK(back.getCreatedTime() == created);
    CHECK(back.getRole() == CourseRole::Educator);
    CHECK(back.getLiveSessionId() == 9);
    CHECK(back.getStudentIds() == std::vector<int>{4, 5});
    CHECK(back.getSessionIds() == std::vector<int>{9, 8});
}

TEST_CASE("Protocol: session summary round trip") {
    SessionSummary summary;
    summary.id = 9;
    summary.courseId = 3;
    summary.name = "Lecture 1";
    summary.startTime = at(1000000);
    summary.endTime = at(4000000);
    summary.status = SessionStatus::Published;
    summary.lostCount = 12;
    summary.questionCount = 4;
    summary.hasRecording = true;

    const SessionSummary back = summaryFromJson(throughWire(toJson(summary)));
    CHECK(back.id == 9);
    CHECK(back.courseId == 3);
    CHECK(back.name == "Lecture 1");
    CHECK(back.startTime == at(1000000));
    CHECK(back.endTime == at(4000000));
    CHECK(back.isPublished());
    CHECK(back.lostCount == 12);
    CHECK(back.questionCount == 4);
    CHECK(back.hasRecording);
}

TEST_CASE("Protocol: a full published session round trips with items, authors and answers") {
    Session session(9, 1, at(1000000), at(4000000));
    session.publish(at(5000000));
    session.setCourseId(3);
    session.setName("Lecture 1");
    session.setRecording(Recording("https://x.panopto.com/Viewer.aspx?id=a", -30,
                                   RecordingSource::Panopto));

    LostPing mine(at(1500000));
    mine.setId(20);
    mine.setSessionId(9);
    mine.setAuthor(4, "ana");
    LostPing hidden(at(1600000));
    hidden.setId(21);
    hidden.setSessionId(9);
    hidden.hideAuthor();
    session.setLostPings({mine, hidden});

    SessionQuestion question(at(1700000), "Why n log n?");
    question.setId(30);
    question.setSessionId(9);
    question.setAuthorDeleted();
    question.setEditedTime(at(1800000));
    Answer student(41, 30, at(4100000), "Because of the halving.");
    student.setAuthor(5, "bob");
    Answer educator(40, 30, at(4200000), "Each level is n work.");
    educator.setAuthor(1, "prof");
    educator.setByEducator(true);
    question.setAnswers({student, educator});
    session.setQuestions({question});

    const Session back = sessionFromJson(throughWire(toJson(session)));
    CHECK(back.getId() == 9);
    CHECK(back.getHostId() == 1);
    CHECK(back.getCourseId() == 3);
    CHECK(back.getName() == "Lecture 1");
    CHECK(back.isPublished());
    CHECK(back.getStartTime() == at(1000000));
    CHECK(back.getEndTime() == at(4000000));
    CHECK(back.getPublishedTime() == at(5000000));
    REQUIRE(back.getRecording() != nullptr);
    CHECK(back.getRecording()->getOffsetSeconds() == -30);
    CHECK(back.getRecording()->getSource() == RecordingSource::Panopto);

    REQUIRE(back.getLostPings().size() == 2);
    CHECK(back.getLostPings()[0].getAuthorName() == "ana");
    CHECK(back.getLostPings()[0].getAuthorId() == 4);
    CHECK(back.getLostPings()[1].getAuthorState() == AuthorState::Hidden);
    CHECK(back.getLostPings()[1].getAuthorId() == 0);

    REQUIRE(back.getQuestions().size() == 1);
    const SessionQuestion& q = back.getQuestions()[0];
    CHECK(q.getText() == "Why n log n?");
    CHECK(q.getAuthorState() == AuthorState::Deleted);
    REQUIRE(q.getEditedTime().has_value());
    CHECK(*q.getEditedTime() == at(1800000));
    REQUIRE(q.getAnswers().size() == 2);
    // Educator answer first (A5), even though it is newer.
    CHECK(q.getAnswers()[0].getText() == "Each level is n work.");
    CHECK(q.getAnswers()[0].isByEducator());
    CHECK(q.getAnswers()[1].getAuthorName() == "bob");
}

TEST_CASE("Protocol: a live session has no end time and stays live") {
    Session live(9, 1, at(1000000));
    live.setJoinCode("123456");
    const QJsonObject json = toJson(live);
    CHECK_FALSE(json.contains(Field::kEndTime));
    const Session back = sessionFromJson(throughWire(json));
    CHECK(back.isLive());
    CHECK(back.getJoinCode() == "123456");
}

TEST_CASE("Protocol: participants and recording candidates round trip") {
    const auto people = participantsFromJson(toJson(std::vector<Participant>{{4, "ana"}, {0, ""}}));
    REQUIRE(people.size() == 2);
    CHECK(people[0].userId == 4);
    CHECK(people[0].name == "ana");
    CHECK(people[1].userId == 0);

    RecordingCandidate candidate;
    candidate.viewerUrl = "https://x.panopto.com/Viewer.aspx?id=a";
    candidate.name = "CSCI 187 Nov 25";
    candidate.start = at(900000);
    candidate.durationSeconds = 4200;
    const auto candidates = candidatesFromJson(toJson(std::vector<RecordingCandidate>{candidate}));
    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0].viewerUrl == candidate.viewerUrl);
    CHECK(candidates[0].start == candidate.start);
    CHECK(candidates[0].durationSeconds == 4200);
}

TEST_CASE("Protocol: junk decodes to zero values, never throws") {
    CHECK(decode(QStringLiteral("not json")).isEmpty());
    CHECK(decode(QStringLiteral("[1,2]")).isEmpty());
    const Session empty = sessionFromJson(QJsonObject{{Field::kLostPings, QStringLiteral("x")}});
    CHECK(empty.getId() == 0);
    CHECK(empty.getLostPings().empty());
    CHECK(getError(makeError(Request::kAddLost, Error::SessionEnded)) == Error::SessionEnded);
}

TEST_CASE("Protocol: details-only session JSON leaves out items and counts") {
    Session ended(9, 1, at(1000000), at(4000000));
    ended.setName("Lecture 1");
    const QJsonObject json = toDetailsJson(ended);
    CHECK(json[Field::kName].toString() == "Lecture 1");
    CHECK(json.contains(Field::kEndTime));
    // No 0 counts or empty lists that would look like an empty session.
    CHECK_FALSE(json.contains(Field::kLostPings));
    CHECK_FALSE(json.contains(Field::kQuestions));
    CHECK_FALSE(json.contains(Field::kLostCount));
    CHECK_FALSE(json.contains(Field::kQuestionCount));
    // The full form always has them.
    CHECK(toJson(ended).contains(Field::kLostPings));
}
