// MODEL: the lecture recording (e.g. on SCU Panopto) a session lines up
// with. Turns "seconds into the session" into "seconds into the video" and
// builds the link that opens the video at that moment. Also picks the right
// recording out of a list (auto-match). Plain C++ (no Qt, no networking), so
// all of the time maths is unit-tested.
//
//   video seconds = session seconds + offset
//   offset        = where the session starts in the video
//                 = session start time - recording start time
//
// Owner: Controller/Server layer (class + tests/test_recording.cpp).

#pragma once

#include <chrono>
#include <string>
#include <vector>

enum class RecordingSource {
    Manual,   // educator pasted the link and typed "session starts at mm:ss"
    Panopto,  // matched through the Panopto API
};

// A recording the Panopto API listed, used to pick the right one. Just data.
struct RecordingCandidate {
    std::string viewerUrl;  // Urls.ViewerUrl
    std::string name;
    std::chrono::system_clock::time_point start;
    long long durationSeconds = 0;  // already defaulted to 70 min if Panopto sent none (R6)
};

class Recording {
public:
    using Clock = std::chrono::system_clock;

    // Links open this many seconds BEFORE the moment, so the viewer hears
    // what led up to the confusion (R4).
    static constexpr long long kLeadInSeconds = 10;
    // Used when Panopto sends an empty Duration (R6).
    static constexpr long long kDefaultDurationSeconds = 70 * 60;
    // Largest offset accepted either way: 24 hours (R3).
    static constexpr long long kMaxOffsetSeconds = 24 * 60 * 60;

    Recording();  // empty link, offset 0
    // viewerUrl: e.g. https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=...
    // offsetSeconds: where the session starts in the video (negative if the
    // session started before the recording did).
    Recording(std::string viewerUrl, long long offsetSeconds,
              RecordingSource source = RecordingSource::Manual);

    const std::string& getViewerUrl() const;
    long long getOffsetSeconds() const;
    RecordingSource getSource() const;

    // Seconds into the video for a moment `sessionSeconds` into the session.
    // Can be negative (the moment is before the video starts).
    long long getVideoSeconds(long long sessionSeconds) const;

    // The link that opens the video kLeadInSeconds before that moment, never
    // below 0, e.g. "...Viewer.aspx?id=abc&start=744". An existing start=...
    // in the link is replaced, not repeated.
    std::string getLinkAt(long long sessionSeconds) const;

    // ---- Auto-match (R6) ----
    // Index of the candidate whose [start, start + duration] overlaps
    // [sessionStart, sessionEnd] the most (tie: the closest start), or -1
    // if none does.
    static int findOverlapping(const std::vector<RecordingCandidate>& candidates,
                               Clock::time_point sessionStart, Clock::time_point sessionEnd);

    // Offset for a session that started at sessionStart, in a recording that
    // started at recordingStart (whole seconds). Not limited: check it with
    // Validation::checkRecordingOffset before saving.
    static long long getOffsetBetween(Clock::time_point recordingStart,
                                      Clock::time_point sessionStart);

    // ---- "Session starts at mm:ss in the video" (R3) ----
    // "1:23", "1:02:03", "83" or "-0:30" -> seconds. False if the text isn't
    // a time; `seconds` is then untouched. (Showing seconds as "mm:ss" is the
    // view's job: ViewText::formatOffset.)
    static bool parseTime(const std::string& text, long long& seconds);

private:
    std::string viewerUrl;
    long long offsetSeconds = 0;
    RecordingSource source = RecordingSource::Manual;
};
