#include "recording.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

Recording::Recording() : viewerUrl(""), offsetSeconds(0), source(RecordingSource::Manual) {}

Recording::Recording(std::string viewerUrl, long long offsetSeconds,
                      RecordingSource source)
    : viewerUrl(std::move(viewerUrl)), offsetSeconds(offsetSeconds), source(source) {}

const std::string& Recording::getViewerUrl() const {
    return this->viewerUrl;
}

long long Recording::getOffsetSeconds() const {
    return this->offsetSeconds;
}

RecordingSource Recording::getSource() const {
    return this->source;
}

long long Recording::getVideoSeconds(long long sessionSeconds) const {
    return sessionSeconds + this->offsetSeconds;
}

std::string Recording::getLinkAt(long long sessionSeconds) const {
    // Open a little before the moment, so the student hears what led up to
    // the confusion (R4), but never before the video starts.
    const long long videoSeconds =
        std::max(0LL, this->getVideoSeconds(sessionSeconds) - kLeadInSeconds);

    // Split off any #fragment, then rebuild the query without an existing
    // "start" parameter (matched by its exact key, so "restart=1" stays).
    std::string link = this->viewerUrl;
    std::string fragment;
    if (const size_t hash = link.find('#'); hash != std::string::npos) {
        fragment = link.substr(hash);
        link.erase(hash);
    }
    std::string base = link;
    std::string kept;
    if (const size_t question = link.find('?'); question != std::string::npos) {
        base = link.substr(0, question);
        const std::string query = link.substr(question + 1);
        size_t pos = 0;
        while (pos <= query.size()) {
            size_t amp = query.find('&', pos);
            if (amp == std::string::npos) {
                amp = query.size();
            }
            const std::string param = query.substr(pos, amp - pos);
            const std::string key = param.substr(0, param.find('='));
            if (!param.empty() && key != "start") {
                kept += (kept.empty() ? "" : "&") + param;
            }
            pos = amp + 1;
        }
    }
    const std::string start = "start=" + std::to_string(videoSeconds);
    return base + "?" + (kept.empty() ? start : kept + "&" + start) + fragment;
}

int Recording::findOverlapping(const std::vector<RecordingCandidate>& candidates,
                               std::chrono::system_clock::time_point sessionStart,
                               std::chrono::system_clock::time_point sessionEnd) {
    // The candidate that overlaps the session the most wins; on a tie, the
    // one that starts closest to the session. (A recording with no length
    // counts as 70 minutes, so it can overlap the next lecture a little:
    // taking the FIRST overlap could pick the wrong one.)
    int best = -1;
    long long bestOverlap = 0;
    long long bestDistance = 0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        long long duration = c.durationSeconds > 0
            ? c.durationSeconds
            : Recording::kDefaultDurationSeconds;
        auto candStart = c.start;
        auto candEnd = c.start + std::chrono::seconds(duration);
        if (!(candStart < sessionEnd && candEnd > sessionStart)) {
            continue;
        }
        const long long overlap = std::chrono::duration_cast<std::chrono::seconds>(
            std::min(candEnd, sessionEnd) - std::max(candStart, sessionStart)).count();
        const long long distance = std::llabs(std::chrono::duration_cast<std::chrono::seconds>(
            candStart - sessionStart).count());
        if (best < 0 || overlap > bestOverlap || (overlap == bestOverlap && distance < bestDistance)) {
            best = static_cast<int>(i);
            bestOverlap = overlap;
            bestDistance = distance;
        }
    }
    return best;
}

long long Recording::getOffsetBetween(
    std::chrono::system_clock::time_point recordingStart,
    std::chrono::system_clock::time_point sessionStart) {
    // The raw difference; the caller checks it against kMaxOffsetSeconds
    // (Validation::checkRecordingOffset) so a bad match is an error, never
    // a silent 0 that sends every jump to the wrong place.
    auto diff = sessionStart - recordingStart;
    return std::chrono::duration_cast<std::chrono::seconds>(diff).count();
}

bool Recording::parseTime(const std::string& text, long long& seconds) {
    // "[-]h:mm:ss", "[-]m:ss" or "[-]s". Minutes and seconds after a ':'
    // must be 0-59; each part at most 6 digits (no overflow before the
    // 24 h check). Anything else is not a time and `seconds` is untouched.
    size_t pos = 0;
    const bool negative = !text.empty() && text[0] == '-';
    if (negative) {
        pos = 1;
    }
    if (pos >= text.size() || text.back() == ':') {
        return false;  // "", "-", "1:"
    }
    std::vector<long long> parts;
    while (pos <= text.size()) {
        size_t next = text.find(':', pos);
        if (next == std::string::npos) {
            next = text.size();
        }
        const std::string part = text.substr(pos, next - pos);
        if (part.empty() || part.size() > 6) {
            return false;
        }
        for (char c : part) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        parts.push_back(std::stoll(part));
        pos = next + 1;
    }
    if (parts.size() > 3) {
        return false;
    }
    for (size_t i = 1; i < parts.size(); ++i) {
        if (parts[i] > 59) {
            return false;  // "1:75"
        }
    }
    long long total = 0;
    for (long long part : parts) {
        total = total * 60 + part;
    }
    if (negative) {
        total = -total;
    }
    if (std::llabs(total) > kMaxOffsetSeconds) {
        return false;
    }
    seconds = total;
    return true;
}
