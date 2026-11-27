#include "recording.h"

#include <chrono>
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
    long long videoSeconds = this->getVideoSeconds(sessionSeconds);
    if (videoSeconds < 0) {
        videoSeconds = 0;
    }
    std::string link = this->viewerUrl;
    size_t pos = link.find("start=");
    if (pos != std::string::npos) {
        size_t end = link.find('&', pos);
        if (end == std::string::npos) {
            end = link.size();
        }
        link.erase(pos, end - pos);
    }
    char sep = (link.find('?') == std::string::npos) ? '?' : '&';
    link += sep;
    link += "start=" + std::to_string(videoSeconds);
    return link;
}

int Recording::findOverlapping(const std::vector<RecordingCandidate>& candidates,
                               std::chrono::system_clock::time_point sessionStart,
                               std::chrono::system_clock::time_point sessionEnd) {
    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        long long duration = c.durationSeconds > 0
            ? c.durationSeconds
            : Recording::kDefaultDurationSeconds;
        auto candStart = c.start;
        auto candEnd = c.start + std::chrono::seconds(duration);
        if (candStart < sessionEnd && candEnd > sessionStart) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

long long Recording::getOffsetBetween(
    std::chrono::system_clock::time_point recordingStart,
    std::chrono::system_clock::time_point sessionStart) {
    auto diff = sessionStart - recordingStart;
    long long offset = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
    if (offset < -kMaxOffsetSeconds || offset > kMaxOffsetSeconds) {
        return 0;
    }
    return offset;
}

bool Recording::parseTime(const std::string& text, long long& seconds) {
    if (text.empty()) {
        return false;
    }
    size_t pos = 0;
    bool negative = false;
    if (text[0] == '-') {
        negative = true;
        pos = 1;
    }
    std::vector<long long> parts;
    while (pos < text.size()) {
        size_t next = text.find(':', pos);
        if (next == std::string::npos) {
            next = text.size();
        }
        std::string part = text.substr(pos, next - pos);
        if (part.empty()) {
            return false;
        }
        for (char c : part) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        try {
            parts.push_back(std::stoll(part));
        } catch (...) {
            return false;
        }
        pos = next + 1;
    }
    if (parts.size() > 3) {
        return false;
    }
    long long total = 0;
    if (parts.size() == 1) {
        total = parts[0];
    } else if (parts.size() == 2) {
        total = parts[0] * 60 + parts[1];
    } else if (parts.size() == 3) {
        total = parts[0] * 3600 + parts[1] * 60 + parts[2];
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