// MODEL: one lecture session of a course. Knows its course, name, join
// code, where it is in its life (live -> in review -> published), when it
// started and ended, its recording, and its Lost pings and questions,
// each in time order.
// (Anonymity and sharing are NOT here: they are course settings, read from
// the Course every time - rule C7.)
// Enforces the rule that nothing is recorded after the session ends.
// Plain C++ (no Qt), so it is easy to test.
//
// The same class is used on the server (built by Database) and in the app
// (built by Protocol from JSON, then kept up to date from live messages with
// upsert... / remove...; an answer change arrives as its whole question,
// so it is an upsertQuestion too).
//
// Owner: Model layer. Extends the M2 class. lostButtonClicked(),
// askQuestion(), getPressCount() and the timestamp helpers keep their M2
// meaning; getUserId() is now getHostId(); getInputs() is replaced by
// getLostPings() + getQuestions() (the M2 SessionInput tests get rewritten).

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "recording.h"
#include "lostping.h"
#include "sessionquestion.h"

// The life of a session (S11). It only moves forward.
enum class SessionStatus {
    Live,       // students press Lost and ask questions
    InReview,   // ended; only the course's educators can open it
    Published,  // students can open it too
};

class Session {
public:
    using Clock = std::chrono::system_clock;

    // ---- Kept from M2 (old tests) ----
    Session();            // starts now, id 0, host 0
    Session(int hostId);  // starts now, id 0
    Session(int id, int hostId);
    Session(int id, int hostId, Clock::time_point startTime);
    // A session that has already ended (for example, loaded from the
    // database): status InReview. Loaders then call publish(when) if needed.
    Session(int id, int hostId, Clock::time_point startTime, Clock::time_point endTime);

    // ---- Identity ----
    int getId() const;
    void setId(int id);
    int getCourseId() const;
    void setCourseId(int courseId);
    int getHostId() const;  // educator who started it; 0 if that account was deleted
    const std::string& getName() const;
    void setName(const std::string& name);
    // 6 digits. "" for imported M2 sessions, and "" in the app unless the
    // viewer is an educator of the course (S4).
    const std::string& getJoinCode() const;
    void setJoinCode(const std::string& code);

    // ---- Start / end ----
    Clock::time_point getStartTime() const;
    Clock::time_point getEndTime() const;  // only meaningful once isEnded()
    bool isEnded() const;  // InReview or Published
    bool isLive() const;   // !isEnded()

    // ---- Review and publish (S11) ----
    SessionStatus getStatus() const;
    bool isInReview() const;
    bool isPublished() const;
    // InReview -> Published at `when`. Returns false (and changes nothing)
    // if the session is live or already published.
    bool publish(Clock::time_point when);
    Clock::time_point getPublishedTime() const;  // only meaningful once isPublished()
    // Saves the end time (now, or `when`) and moves Live -> InReview. After
    // this, inputs are no longer recorded. Ending twice does nothing.
    void endSession();
    void endSession(Clock::time_point when);
    // Seconds from start to end; for a live session, from start to `now`.
    long long getDurationSeconds(Clock::time_point now) const;

    // ---- Recording (R rules) ----
    const Recording* getRecording() const;  // nullptr if none attached
    void setRecording(const Recording& recording);
    void clearRecording();

    // ---- Lost pings and questions ----
    // Record one. Return false (and record nothing) once ended (I2, I6).
    bool addLostPing(const LostPing& ping);
    bool addQuestion(const SessionQuestion& question);
    // Shortcuts with the current time (kept from M2).
    bool lostButtonClicked();
    // Also false if the text is not a valid question (I3, Validation::checkQuestion).
    bool askQuestion(const std::string& text);

    // For loaders (Database, Protocol): replace the whole list, sorted by
    // time then id. Work on ended sessions too.
    void setLostPings(std::vector<LostPing> pings);
    void setQuestions(std::vector<SessionQuestion> questions);

    // For live updates in the app. They apply what the server already
    // decided, so they do NOT check isEnded().
    void upsertLostPing(const LostPing& ping);              // add, or replace same id
    bool removeLostPing(int pingId);                        // false if no such ping
    void upsertQuestion(const SessionQuestion& question);   // add, or replace same id
    bool removeQuestion(int questionId);                    // false if no such question

    const std::vector<LostPing>& getLostPings() const;          // by time, then id
    const std::vector<SessionQuestion>& getQuestions() const;   // by time, then id
    const LostPing* findLostPing(int pingId) const;             // nullptr if none
    const SessionQuestion* findQuestion(int questionId) const;  // nullptr if none

    int getPressCount() const;     // number of Lost pings (M2 name)
    int getQuestionCount() const;

    // Times of the Lost pings, in order (kept from M1).
    std::vector<Clock::time_point> getLostButtonTimestamps() const;
    // Seconds from the start of the session to each Lost ping, in order.
    std::vector<long long> getPressOffsetsSeconds() const;
    // Seconds from the start of the session to a moment. This is the number
    // the timeline and Recording::getLinkAt use (pass ping.getTime() or
    // question.getTime()).
    long long getOffsetSeconds(Clock::time_point time) const;

private:
    SessionQuestion* findQuestionMutable(int questionId);

    int id = 0;
    int courseId = 0;
    int hostId = 0;
    std::string name;
    std::string joinCode;
    Clock::time_point startTime;
    Clock::time_point endTime = Clock::time_point{};
    Clock::time_point publishedTime = Clock::time_point{};
    SessionStatus status = SessionStatus::Live;
    std::optional<Recording> recording;
    std::vector<LostPing> lostPings;
    std::vector<SessionQuestion> questions;
};

// One row of a course's session list: enough to show the list without
// sending every ping and question. Just data. Counts are totals for the whole class.
// For a STUDENT, a session that is in review is only a placeholder: the
// server sends its name, start time and status, with both counts 0 and
// hasRecording false (S12).
struct SessionSummary {
    int id = 0;
    int courseId = 0;
    std::string name;
    Session::Clock::time_point startTime;
    Session::Clock::time_point endTime;  // only meaningful unless Live
    SessionStatus status = SessionStatus::Live;
    int lostCount = 0;
    int questionCount = 0;
    bool hasRecording = false;

    bool isLive() const { return status == SessionStatus::Live; }
    bool isInReview() const { return status == SessionStatus::InReview; }
    bool isPublished() const { return status == SessionStatus::Published; }
};
