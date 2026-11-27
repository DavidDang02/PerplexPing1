// Server data classes against their header comments (found in the Nov 25
// review). Plain C++: no database, no Qt.

#include "doctest.h"

#include <chrono>

#include "answer.h"
#include "course.h"
#include "error.h"
#include "joincodes.h"
#include "lostping.h"
#include "recording.h"
#include "session.h"
#include "sessionquestion.h"
#include "user.h"

#include <set>
#include <string>

namespace {
using Clock = std::chrono::system_clock;
Clock::time_point at(long long seconds) {
    return Clock::time_point(std::chrono::seconds(seconds));
}
}  // namespace

TEST_CASE("Recording: links open 10 s before the moment, never before 0 (R4)") {
    const Recording rec("https://x.panopto.com/Viewer.aspx?id=abc", 30);
    CHECK(rec.getLinkAt(100) == "https://x.panopto.com/Viewer.aspx?id=abc&start=120");
    CHECK(rec.getLinkAt(-35) == "https://x.panopto.com/Viewer.aspx?id=abc&start=0");
}

TEST_CASE("Recording: an existing start= is replaced, other parameters kept") {
    CHECK(Recording("https://x/V.aspx?id=abc&start=5", 0).getLinkAt(100) ==
          "https://x/V.aspx?id=abc&start=90");
    CHECK(Recording("https://x/V.aspx?start=5&id=abc", 0).getLinkAt(100) ==
          "https://x/V.aspx?id=abc&start=90");
    CHECK(Recording("https://x/V.aspx?id=restart=1", 0).getLinkAt(100) ==
          "https://x/V.aspx?id=restart=1&start=90");
    CHECK(Recording("https://x/V.aspx", 0).getLinkAt(100) == "https://x/V.aspx?start=90");
    CHECK(Recording("https://x/V.aspx?id=a#notes", 0).getLinkAt(100) ==
          "https://x/V.aspx?id=a&start=90#notes");
}

TEST_CASE("Recording::parseTime accepts times, rejects everything else untouched (R3)") {
    long long s = -1;
    CHECK(Recording::parseTime("1:23", s));
    CHECK(s == 83);
    CHECK(Recording::parseTime("1:02:03", s));
    CHECK(s == 3723);
    CHECK(Recording::parseTime("-0:30", s));
    CHECK(s == -30);
    CHECK(Recording::parseTime("83", s));
    CHECK(s == 83);
    s = 7;
    for (const char* bad : {"", "-", "1:", ":5", "1:75", "1:2:3:4", "a", "1:-2", "9999999999999999999"}) {
        CAPTURE(bad);
        CHECK_FALSE(Recording::parseTime(bad, s));
        CHECK(s == 7);
    }
    CHECK_FALSE(Recording::parseTime("25:00:00", s));  // over 24 h
}

TEST_CASE("Session: pings and questions stay in time order, however they arrive") {
    Session session(1, 1, at(1000));
    LostPing late(at(1020));
    late.setId(2);
    LostPing early(at(1010));
    early.setId(1);
    session.setLostPings({late, early});
    CHECK(session.getLostPings()[0].getId() == 1);

    SessionQuestion q2(at(1030), "second");
    q2.setId(6);
    SessionQuestion q1(at(1015), "first");
    q1.setId(5);
    session.upsertQuestion(q2);
    session.upsertQuestion(q1);  // a live push that arrived late
    CHECK(session.getQuestions()[0].getText() == "first");
    // Same time: lower id first.
    LostPing tie(at(1010));
    tie.setId(0);
    session.upsertLostPing(tie);
    CHECK(session.getLostPings()[0].getId() == 0);
}

TEST_CASE("Hidden authors keep no id or name inside the object (V2)") {
    LostPing ping(at(1));
    ping.setAuthor(7, "ana");
    ping.hideAuthor();
    CHECK(ping.getAuthorId() == 0);
    CHECK(ping.getAuthorName().empty());
    SessionQuestion q(at(1), "why?");
    q.setAuthor(7, "ana");
    q.hideAuthor();
    CHECK(q.getAuthorId() == 0);
    CHECK_FALSE(q.isBy(7));
    Answer a(1, 1, at(1), "because");
    a.setAuthor(7, "ana");
    a.hideAuthor();
    CHECK(a.getAuthorName().empty());
}

TEST_CASE("Course: a deleted account (id 0) is never the educator") {
    Course course(1, "CSCI 187");
    CHECK_FALSE(course.isEducator(0));  // educator deleted too: both 0
    course.setEducatorId(3);
    CHECK(course.isEducator(3));
    CHECK_FALSE(course.isEducator(4));
}

TEST_CASE("Session: ended means the status left Live, whatever the end time") {
    Session session(1, 1, at(1000));
    CHECK(session.isLive());
    session.endSession(Clock::time_point{});  // a zero end time
    CHECK(session.isEnded());
    CHECK_FALSE(session.addLostPing(LostPing(at(1001))));  // locked (I2)
    CHECK(session.publish(at(2000)));
    CHECK(session.isEnded());
}

TEST_CASE("Session::askQuestion refuses an empty or too-long question (I3)") {
    Session session(1, 1, at(1000));
    CHECK_FALSE(session.askQuestion(""));
    CHECK_FALSE(session.askQuestion("   "));
    CHECK_FALSE(session.askQuestion(std::string(501, 'q')));
    CHECK(session.askQuestion("  why?  "));
    REQUIRE(session.getQuestions().size() == 1);
    CHECK(session.getQuestions()[0].getText() == "why?");
}

TEST_CASE("Error: every code survives the trip over the network, and codes are distinct") {
    std::set<std::string> seen;
    for (int i = static_cast<int>(Error::None); i <= static_cast<int>(Error::PanoptoNoAccess); ++i) {
        const Error error = static_cast<Error>(i);
        const std::string code = getErrorCode(error);
        CAPTURE(code);
        CHECK(getErrorFromCode(code) == error);
        CHECK(seen.insert(code).second);
    }
    CHECK(getErrorFromCode("NoSuchThing") == Error::BadRequest);
}

TEST_CASE("Answer order: educator first, then oldest, then id (A5), also after edits") {
    Answer student(1, 9, at(100), "student, early");
    Answer educatorLate(2, 9, at(300), "educator, late");
    educatorLate.setByEducator(true);
    Answer educatorEarly(3, 9, at(200), "educator, early");
    educatorEarly.setByEducator(true);
    Answer tie(4, 9, at(100), "student, same time, higher id");
    SessionQuestion question;
    question.setAnswers({tie, educatorLate, student, educatorEarly});
    const auto& a = question.getAnswers();
    REQUIRE(a.size() == 4);
    CHECK(a[0].getId() == 3);
    CHECK(a[1].getId() == 2);
    CHECK(a[2].getId() == 1);
    CHECK(a[3].getId() == 4);
    // The educator's late answer is replaced by one no longer marked
    // educator: it moves down to its place.
    Answer demoted = educatorLate;
    demoted.setByEducator(false);
    question.upsertAnswer(demoted);
    CHECK(question.getAnswers()[1].getId() == 1);
    CHECK(question.getAnswers().back().getId() == 2);
    CHECK(question.removeAnswer(2));
    CHECK_FALSE(question.removeAnswer(2));
}

TEST_CASE("JoinCodes: course codes are 6 easy-to-read characters, session codes 6 digits") {
    const std::string allowed = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < 200; ++i) {
        const std::string course = JoinCodes::generateCourseCode();
        REQUIRE(course.size() == 6);
        CHECK(course.find_first_not_of(allowed) == std::string::npos);
        const std::string session = JoinCodes::generateSessionCode();
        REQUIRE(session.size() == 6);
        CHECK(session.find_first_not_of("0123456789") == std::string::npos);
    }
}

TEST_CASE("User: keeps its id, name and email; the default is nobody") {
    const User ana(7, "Ana", "ana@scu.edu");
    CHECK(ana.getId() == 7);
    CHECK(ana.getName() == "Ana");
    CHECK(ana.getEmail() == "ana@scu.edu");
    const User nobody;
    CHECK(nobody.getId() == 0);
    CHECK(nobody.getName().empty());
}
