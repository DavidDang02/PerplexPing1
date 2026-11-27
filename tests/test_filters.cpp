// Search, filtering and the timeline's numbers (server/filters.h).

#include "doctest.h"

#include <chrono>

#include "filters.h"

namespace {

using Clock = Session::Clock;

Clock::time_point at(long long seconds) {
    return Clock::time_point(std::chrono::seconds(seconds));
}

SessionSummary summary(int id, const std::string& name) {
    SessionSummary s;
    s.id = id;
    s.name = name;
    return s;
}

SessionQuestion question(int id, long long seconds, const std::string& text) {
    SessionQuestion q(at(seconds), text);
    q.setId(id);
    return q;
}

}  // namespace

TEST_CASE("Filters: every word must appear, ignoring case and spacing") {
    CHECK(Filters::matches("Lecture 3: Recursion", "rec lec"));
    CHECK(Filters::matches("Lecture 3: Recursion", "  RECURSION  "));
    CHECK_FALSE(Filters::matches("Lecture 3: Recursion", "recursion trees"));
    // Empty or blank query keeps everything.
    CHECK(Filters::matches("anything", ""));
    CHECK(Filters::matches("", "   "));
    CHECK_FALSE(Filters::matches("", "x"));
}

TEST_CASE("Filters: session search keeps the order and finds by name") {
    const std::vector<SessionSummary> all = {summary(3, "Lecture 3: Recursion"),
                                             summary(2, "Lecture 2: Big O"),
                                             summary(1, "Lecture 1: Intro")};
    const auto found = Filters::filterSessions(all, "lecture");
    REQUIRE(found.size() == 3);
    CHECK(found[0].id == 3);
    const auto one = Filters::filterSessions(all, "big o");
    REQUIRE(one.size() == 1);
    CHECK(one[0].id == 2);
    CHECK(Filters::filterSessions(all, "graphs").empty());
    CHECK(Filters::filterSessions({}, "x").empty());
}

TEST_CASE("Filters: question search looks in answers too, and can show only unanswered") {
    SessionQuestion answered = question(1, 100, "Why is merge sort n log n?");
    answered.setAnswers({Answer(10, 1, at(5000), "Each level does n work.")});
    const SessionQuestion open = question(2, 200, "What is a base case?");
    const std::vector<SessionQuestion> all = {answered, open};

    CHECK(Filters::filterQuestions(all, "", false).size() == 2);
    // Matches an answer's words.
    const auto byAnswer = Filters::filterQuestions(all, "level", false);
    REQUIRE(byAnswer.size() == 1);
    CHECK(byAnswer[0].getId() == 1);
    // Unanswered only.
    const auto unanswered = Filters::filterQuestions(all, "", true);
    REQUIRE(unanswered.size() == 1);
    CHECK(unanswered[0].getId() == 2);
    // Both at once: nothing unanswered mentions "merge".
    CHECK(Filters::filterQuestions(all, "merge", true).empty());
}

TEST_CASE("Timeline: one bar per minute from start to end, counting pings and questions") {
    Session session(9, 1, at(0), at(5 * 60 + 30));  // 5.5 minutes
    LostPing early(at(10));
    early.setId(1);
    LostPing peak1(at(185));
    peak1.setId(2);
    LostPing peak2(at(190));
    peak2.setId(3);
    session.setLostPings({early, peak1, peak2});
    session.setQuestions({question(4, 200, "huh?")});

    const auto buckets = Timeline::getBuckets(session);
    REQUIRE(buckets.size() == 6);  // 0:00 .. 5:00
    CHECK(buckets[0].startSeconds == 0);
    CHECK(buckets[0].lostCount == 1);
    CHECK(buckets[3].startSeconds == 180);
    CHECK(buckets[3].lostCount == 2);
    CHECK(buckets[3].questionCount == 1);
    CHECK(buckets[5].lostCount == 0);
    CHECK(Timeline::getPeakIndex(buckets) == 3);
}

TEST_CASE("Timeline: edge cases never crash or lose an item") {
    // Live, nothing yet: one empty bar, no peak.
    Session live(9, 1, at(0));
    const auto empty = Timeline::getBuckets(live);
    REQUIRE(empty.size() == 1);
    CHECK(Timeline::getPeakIndex(empty) == -1);

    // A ping stamped before the start (clock skew) lands in the first bar;
    // a bad bar width falls back to one minute.
    Session ended(9, 1, at(100), at(220));
    LostPing skewed(at(90));
    skewed.setId(1);
    ended.setLostPings({skewed});
    const auto buckets = Timeline::getBuckets(ended, 0);
    REQUIRE(buckets.size() == 2);  // exactly 2:00 long
    CHECK(buckets[0].lostCount == 1);

    // Ties go to the earliest bar.
    std::vector<Timeline::Bucket> tie(3);
    tie[1].lostCount = 2;
    tie[2].lostCount = 2;
    CHECK(Timeline::getPeakIndex(tie) == 1);
}

TEST_CASE("Timeline: a session of exactly one hour has 60 bars, the last item in the last") {
    Session hour(9, 1, at(0), at(3600));
    LostPing last(at(3600));
    last.setId(1);
    hour.setLostPings({last});
    const auto buckets = Timeline::getBuckets(hour);
    REQUIRE(buckets.size() == 60);
    CHECK(buckets[59].lostCount == 1);
}
