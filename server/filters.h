// MODEL (shared): search, filtering and the timeline's numbers. Pure
// functions over model objects, compiled into the app (and the tests). They
// decide WHICH items a page shows; the page only decides how they look.
// No Qt, no network, no database: everything here works on what the app
// already holds, so filtering is instant and needs no server request.

#pragma once

#include <string>
#include <vector>

#include "session.h"
#include "sessionquestion.h"

namespace Filters {

// True if every word of `query` appears in `text`, ignoring case and extra
// spaces. An empty or all-space query matches everything.
//   matches("Lecture 3: Recursion", "rec lec") == true
bool matches(const std::string& text, const std::string& query);

// The course page's search box: sessions whose name matches `query`
// (see matches()), in the order given (newest first).
std::vector<SessionSummary> filterSessions(const std::vector<SessionSummary>& sessions,
                                           const std::string& query);

// The session viewer's search box and "Unanswered only" toggle: questions
// whose text, or any answer's text, matches `query`; with
// `unansweredOnly`, only questions with no answers yet. Order is kept
// (by time).
std::vector<SessionQuestion> filterQuestions(const std::vector<SessionQuestion>& questions,
                                             const std::string& query, bool unansweredOnly);

}  // namespace Filters

namespace Timeline {

// One bar of the confusion timeline.
struct Bucket {
    long long startSeconds = 0;  // into the session, e.g. 300 = 5:00
    int lostCount = 0;
    int questionCount = 0;
};

// The default bar width: one minute.
inline constexpr long long kBucketSeconds = 60;

// Splits the session into bars of `bucketSeconds` from its start to its
// end (or to the last item for a live session) and counts the Lost pings
// and questions in each. Always at least one bar. Items before the start
// count in the first bar. bucketSeconds <= 0 uses kBucketSeconds.
std::vector<Bucket> getBuckets(const Session& session, long long bucketSeconds = kBucketSeconds);

// The bar with the most Lost pings (the earliest one on a tie), or -1 if
// nobody pressed Lost. The page can highlight it as "most confusing".
int getPeakIndex(const std::vector<Bucket>& buckets);

}  // namespace Timeline
