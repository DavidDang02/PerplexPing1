// MODEL: the coordinator. Runs ON THE SERVER, one AppModel per connected
// app. Remembers who is logged in on that connection, applies every rule
// (Validation, Permissions, Visibility), and is the only class that calls
// Database. PerplexServer talks to this class, never to Database.
//
// It knows nothing about sockets, JSON or other connections, so every rule
// can be unit-tested with a fresh in-memory database:
//
//     Database db(":memory:");  REQUIRE(db.open());
//     AppModel ana(&db, fixedClock, PasswordHasher::kTestIterations);
//     AppModel bob(&db, fixedClock, PasswordHasher::kTestIterations);
//     // two "connections" sharing one database, exactly like the server
//
// Conventions:
//   * Every method first checks "logged in?" (Error::NotLoggedIn, U9) unless
//     it says otherwise, then validates, then checks permission, then saves.
//   * Text arguments are trimmed here (except passwords). Callers pass what
//     the user typed.
//   * All times come from the clock passed to the constructor, cut to whole
//     seconds (S10). Nothing takes a time from the caller.
//   * Returned objects are for THIS user unless the comment says "raw".
//     Raw objects carry every author and must go through Visibility before
//     they are sent to anyone (V7).
//   * A failed call changes nothing in the database.
//
// Owner: Model layer (+ tests/test_appmodel_*.cpp). Milestone tags: [F1]..[F5], [S] = stretch.

#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "answer.h"
#include "course.h"
#include "error.h"
#include "passwordhasher.h"
#include "permissions.h"
#include "recording.h"
#include "session.h"
#include "lostping.h"
#include "sessionquestion.h"
#include "user.h"

class Database;

// What openSession / joinSession / startSession give back. Just data.
struct OpenedSession {
    Session session;  // redacted for this user (safe to send to them)
    Course course;    // with this user's role; join code only if educator
    Viewer viewer;    // this user + role; the server keeps it for broadcasts
    bool newlyEnrolled = false;  // joinSession just made them a course member (S5)
};

// What every Lost ping change gives back, so the server can tell the right
// people (it runs Visibility once per recipient with these). Just data.
struct LostPingChange {
    Course course;    // the course with its CURRENT toggles (role field not meaningful)
    Session session;  // RAW details only (no pings/questions): live or ended, course id
    LostPing ping;    // RAW. After a delete: as it was just before.
};

// Same, for every question or answer change.
struct QuestionChange {
    Course course;
    Session session;
    SessionQuestion question;  // RAW, with its answers. After a delete: as it was just before.
    int answerId = 0;          // the answer added / edited / deleted, 0 for question changes
    bool byModerator = false;  // deleteQuestion only: the educator removed a student's question (I7)
};

class AppModel {
public:
    using Clock = std::chrono::system_clock;
    using NowFunction = std::function<Clock::time_point()>;

    // db is not owned. `now` is the clock (tests pass a fake one to test the
    // freeze and time order). `passwordIterations` is for new hashes.
    explicit AppModel(Database* db,
                      NowFunction now = [] { return Clock::now(); },
                      int passwordIterations = PasswordHasher::kDefaultIterations);

    // ================= Accounts =================

    // [F1] U1-U4, U6. Creates the account and logs this connection in.
    // Does not need a login. Every account is the same kind: it becomes an
    // educator only by creating a course.
    // Errors are checked in this order: name (U1), password (U3), email (U6),
    // then NameTaken, then EmailTaken.
    Result<User> registerUser(const std::string& name, const std::string& password,
                              const std::string& email);

    // [F1] U5. Does not need a login. EmptyName / EmptyPassword if blank,
    // otherwise WrongLogin for any mismatch. The name ignores case (U2).
    // Logging in while logged in switches user.
    Result<User> logIn(const std::string& name, const std::string& password);

    // [F1] Never fails.
    void logOut();

    bool isLoggedIn() const;
    const User* getCurrentUser() const;  // nullptr when logged out

    // [F4] U7. WrongPassword, then U3 errors for the new one.
    Error changePassword(const std::string& oldPassword, const std::string& newPassword);

    // [F4] U6. Returns the updated user. (An email can be changed, not removed.)
    Result<User> updateEmail(const std::string& email);

    // [F4] U8. WrongPassword if it doesn't match; HostingLiveSession if a
    // live session was started by this user (end it first). On success the
    // account is gone, their inputs and answers stay as "Deleted user",
    // their courses stay, and this connection is logged out. The server
    // must then log out every other connection of that user.
    Error deleteAccount(const std::string& password);

    // ================= Courses =================

    // [F2] C1-C3. Any logged-in user. The caller becomes the course's one
    // educator (Courses.educator_id) and is NOT added to CourseMembers.
    Result<Course> createCourse(const std::string& name, bool anonymous, bool questionsShared);

    // [F2] C4. NoSuchCourseCode if no course has it. The user becomes a
    // Student of it. Already a student, or its educator: no change,
    // returns the course.
    Result<Course> joinCourse(const std::string& joinCode);

    // [F2] C5. Every course the user is a member of, sorted by name
    // (ignoring case).
    Result<std::vector<Course>> getCourses() const;

    // [F2] C5. The course and its sessions, newest first. For a student,
    // sessions in review are placeholders (S12: counts 0, no recording
    // flag). ([S] plus the member list when the user is an educator of it.)
    Result<CourseDetail> getCourse(int courseId) const;

    // [F4] C6, C7. Changes the name and the two toggles (send all three).
    // The toggles take effect at once for every session of the course.
    Result<Course> updateCourse(int courseId, const std::string& name, bool anonymous,
                                bool questionsShared);

    // [S] C9. Educator only. Deletes the course with everything in it.
    Error deleteCourse(int courseId);

    // The logged-in user's role in a course, worked out every time from
    // Courses.educator_id and CourseMembers, never saved: Educator if they
    // created it, Student if they joined it, None if logged out or neither.
    // Every educator-only call checks this in the model (the view hiding a
    // button is not a check).
    CourseRole getRole(int courseId) const;
    bool isCourseEducator(int courseId) const;  // getRole(courseId) == Educator

    // ================= Sessions =================

    // [F2] S1-S4. Starts a live session now with a fresh 6-digit code.
    // CourseHasLiveSession if the course already has one (S3).
    Result<OpenedSession> startSession(int courseId, const std::string& name);

    // [F2] S5. Finds the LIVE session with this code; enrolls the user in
    // its course as a student if they aren't a member yet; then like openSession.
    Result<OpenedSession> joinSession(const std::string& joinCode);

    // [F2] S6, S12. Educators: any session. Students: live or published
    // ones (SessionNotPublished while in review). This is also how an app
    // reloads a session after it changed status or after the course's
    // toggles changed (C7).
    Result<OpenedSession> openSession(int sessionId) const;

    // [F2] S7. Live -> in review. Returns the session's raw details (no
    // inputs) with its end time.
    Result<Session> endSession(int sessionId);

    // [F4] S11. In review -> published. Educators of the course only.
    // Works with or without a recording. Returns raw details (no inputs).
    Result<Session> publishSession(int sessionId);

    // [F4] S8. Returns raw details (no inputs).
    Result<Session> renameSession(int sessionId, const std::string& name);

    // [F4] S9. Returns the raw details as they were (so the server knows the
    // course and can tell the room).
    Result<Session> deleteSession(int sessionId);

    // ================= Lost pings and questions (live) =================

    // [F2] I1, I2 (+ I8 [S]).
    Result<LostPingChange> addLost(int sessionId);

    // [F3] I5, I6: only the student who pressed it, only while live.
    // NoSuchInput if unknown or not visible to the user.
    Result<LostPingChange> deleteLost(int pingId);

    // [F2] I1-I3.
    Result<QuestionChange> addQuestion(int sessionId, const std::string& text);

    // [F3] I3, I4, I6. NoSuchInput if the id is unknown or in a course the
    // user is not a member of.
    Result<QuestionChange> editQuestion(int questionId, const std::string& text);

    // [F3] I5, I6 for the author while live; [F4] I7 for the educator once
    // the session has ended (moderation; sets byModerator). Deletes its
    // answers too (I9).
    Result<QuestionChange> deleteQuestion(int questionId);

    // ================= Answers (after the session) =================

    // [F4] A1-A4. Educators: in review or published. Students: published
    // only. `question` in the result has the new answer in place (A5).
    Result<QuestionChange> addAnswer(int questionId, const std::string& text);

    // [F4] A3, A8. Only the author. `question` in the result has the edited answer.
    Result<QuestionChange> editAnswer(int answerId, const std::string& text);

    // [F4] A7. NoSuchAnswer if unknown or not visible to the user.
    Result<QuestionChange> deleteAnswer(int answerId);

    // ================= Recording =================

    // [F2 model, F5 screen] R1-R3. Attaches or replaces. Only once the
    // session has ended (in review or published). Returns raw details (no
    // inputs) with the recording.
    Result<Session> attachRecording(int sessionId, const std::string& url,
                                    long long offsetSeconds, RecordingSource source);

    // [F5] R1. No error if there was none.
    Result<Session> removeRecording(int sessionId);

    // [F5] R1, R6 ("Auto-match recording"). `candidates` is what the app got
    // from Panopto for the course's folder. Picks the first one that
    // overlaps the session (Recording::findOverlapping with the session's
    // stored start and end), works out the offset
    // (Recording::getOffsetBetween) and attaches it with source Panopto.
    // RecordingNotReady if none overlaps (Panopto is still processing, or
    // the lecture was not recorded); nothing changes then.
    Result<Session> matchRecording(int sessionId,
                                   const std::vector<RecordingCandidate>& candidates);

private:
    // now() cut to whole seconds.
    Clock::time_point getNow() const;
    // The session's raw details, its course and the viewer for the current
    // user, or the error (NotLoggedIn, NoSuchSession).
    Result<Session> loadSessionDetails(int sessionId, Course& course, Viewer& viewer) const;
    // Tries JoinCodes::make...() until Database says the code is free.
    std::string makeFreeSessionCode() const;
    std::string makeFreeCourseCode() const;

    Database* db;
    NowFunction now;
    int passwordIterations;
    std::optional<User> currentUser;
};
