#include "filters.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::vector<std::string> splitWords(const std::string& text) {
    std::vector<std::string> words;
    std::istringstream in(toLower(text));
    std::string word;
    while (in >> word) {
        words.push_back(word);
    }
    return words;
}

}  // namespace

namespace Filters {

bool matches(const std::string& text, const std::string& query) {
    const std::string haystack = toLower(text);
    for (const std::string& word : splitWords(query)) {
        if (haystack.find(word) == std::string::npos) {
            return false;
        }
    }
    return true;
}

std::vector<SessionSummary> filterSessions(const std::vector<SessionSummary>& sessions,
                                           const std::string& query) {
    std::vector<SessionSummary> kept;
    for (const SessionSummary& session : sessions) {
        if (matches(session.name, query)) {
            kept.push_back(session);
        }
    }
    return kept;
}

std::vector<SessionQuestion> filterQuestions(const std::vector<SessionQuestion>& questions,
                                             const std::string& query, bool unansweredOnly) {
    std::vector<SessionQuestion> kept;
    for (const SessionQuestion& question : questions) {
        if (unansweredOnly && !question.getAnswers().empty()) {
            continue;
        }
        bool found = matches(question.getText(), query);
        for (const Answer& answer : question.getAnswers()) {
            found = found || matches(answer.getText(), query);
        }
        if (found) {
            kept.push_back(question);
        }
    }
    return kept;
}

}  // namespace Filters

namespace Timeline {

std::vector<Bucket> getBuckets(const Session& session, long long bucketSeconds) {
    if (bucketSeconds <= 0) {
        bucketSeconds = kBucketSeconds;
    }
    // How long the bars cover: the whole session once ended, otherwise up
    // to the latest item.
    long long length = 0;
    if (session.isEnded()) {
        length = session.getOffsetSeconds(session.getEndTime());
    }
    for (const LostPing& ping : session.getLostPings()) {
        length = std::max(length, session.getOffsetSeconds(ping.getTime()));
    }
    for (const SessionQuestion& question : session.getQuestions()) {
        length = std::max(length, session.getOffsetSeconds(question.getTime()));
    }
    // A session that ends at exactly 60:00 has 60 one-minute bars; an item
    // at 60:00 goes in the last one (indexFor clamps).
    const long long count = std::max(1LL, (length + bucketSeconds - 1) / bucketSeconds);

    std::vector<Bucket> buckets(static_cast<size_t>(count));
    for (long long i = 0; i < count; ++i) {
        buckets[static_cast<size_t>(i)].startSeconds = i * bucketSeconds;
    }
    auto indexFor = [&](Session::Clock::time_point time) {
        const long long offset = std::max(0LL, session.getOffsetSeconds(time));
        return static_cast<size_t>(std::min(count - 1, offset / bucketSeconds));
    };
    for (const LostPing& ping : session.getLostPings()) {
        ++buckets[indexFor(ping.getTime())].lostCount;
    }
    for (const SessionQuestion& question : session.getQuestions()) {
        ++buckets[indexFor(question.getTime())].questionCount;
    }
    return buckets;
}

int getPeakIndex(const std::vector<Bucket>& buckets) {
    int peak = -1;
    for (size_t i = 0; i < buckets.size(); ++i) {
        if (buckets[i].lostCount > 0 &&
            (peak < 0 || buckets[i].lostCount > buckets[static_cast<size_t>(peak)].lostCount)) {
            peak = static_cast<int>(i);
        }
    }
    return peak;
}

}  // namespace Timeline
