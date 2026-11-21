// LOCAL PERSISTENCE: a SQLite-backed database layer implementing the v2 contract.
// Owns the single SQLite file on the server. Uses Result<T> (custom datatype, not std::optional).
// Each Database object opens its own connection with a unique name.
// Every query uses prepared statements with bound values.
// Multi-row writes go in one transaction.
// Times stored as UTC ISO 8601 text to the second (e.g. "2026-11-16T22:15:03Z").
// PRAGMA foreign_keys = ON on open.
// The tables are in schema.sql (that file is the contract;
// change both in the same commit and bump kSchemaVersion).

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <QSqlDatabase>
#include <QString>

#include "answer.h"
#include "course.h"
#include "error.h"
#include "lostping.h"
#include "recording.h"
#include "session.h"
#include "sessionquestion.h"
#include "user.h"

// What is stored about a password. Never leaves the server. Just data.
struct Credentials {
    int userId = 0;
    std::string hashHex;
    std::string saltHex;
    int iterations = 0;
};

class Database {
public:
    using Clock = std::chrono::system_clock;

    // The schema version this code reads and writes (PRAGMA user_version).
    static constexpr int kSchemaVersion = 2;

    // path is a file such as "perplexping.db", or ":memory:" in tests.
    explicit Database(const QString& path);
    ~Database();

    // Opens the connection, turns on foreign keys, detects the schema
    // version and creates/migrates as needed. False on failure.
    bool open();

    // The schema version in effect after open().
    int getSchemaVersion() const;

    // [F1] Users
    // Inserts a user. nullopt if the name (ignoring case) or the email
    // (ignoring case) is taken. email "" is stored as NULL (only the
    // migration does that).
    Result<User> createUser(const std::string& name,
                            const std::string& email, const std::string& hashHex,
                            const std::string& saltHex, int iterations,
                            Clock::time_point createdTime);

    // Returns the user with the given id, or nullopt if no such user.
    Result<User> getUser(int userId);

    // Ignores case ("ANA" finds "ana"). Returns the name as stored.
    Result<User> findUserByName(const std::string& name);

    // Ignores case. For telling NameTaken from EmailTaken (U6).
    Result<User> findUserByEmail(const std::string& email);

    // Returns the credentials for the given user, or nullopt if no such user.
    Result<Credentials> getCredentials(int userId);

    // [F4] False if there is no such user.
    bool setPassword(int userId, const std::string& hashHex,
                      const std::string& saltHex, int iterations);

    // [F4] False if another user has that email (ignoring case) or there is
    // no such user.
    bool setEmail(int userId, const std::string& email);

    // [F4] U8: number of live sessions this user started (host_id).
    int countLiveSessionsHostedBy(int userId);

    // [F4] U8: deletes the user row, nothing else. The foreign keys do the
    // rest: their memberships go (CASCADE); their inputs and answers stay
    // with user_id NULL ("Deleted user"); sessions they hosted keep
    // host_id NULL; courses they created stay with educator_id NULL.
    // False if there is no such user.
    bool deleteUser(int userId);

    // [F2] Courses
    // Inserts the course with educator_id = educatorUserId (no CourseMembers
    // row). nullopt if the join code is taken. The returned course has role
    // Educator and no students.
    Result<Course> createCourse(const std::string& name, const std::string& joinCode,
                                  bool anonymous, bool questionsShared,
                                  int educatorUserId, Clock::time_point createdTime);

    // The course as seen by userId: role (worked out from educator_id and
    // CourseMembers; None if neither), student ids, session ids and live
    // session id filled in. The join code is always filled in
    // (AppModel blanks it for students, C8).
    Result<Course> getCourse(int courseId, int userId);
    std::optional<Course> findCourseByJoinCode(const std::string& joinCode, int userId);

    // Every course userId created or joined, sorted by name ignoring case,
    // each filled in like getCourse.
    std::vector<Course> listCoursesForUser(int userId);

    // [F4] False if there is no such course.
    bool updateCourse(int courseId, const std::string& name, bool anonymous,
                      bool questionsShared);

    // [S] Deletes the course with its members, sessions, inputs and answers.
    bool deleteCourse(int courseId);

    // Adds a student (a CourseMembers row). False if they already are one.
    bool addStudent(int courseId, int userId, Clock::time_point joinedTime);
    // Educator if educator_id = userId, Student if a CourseMembers row,
    // None otherwise (or no such course).
    CourseRole getRole(int courseId, int userId);

    // [F2] Sessions
    // Inserts a live session. nullopt if the course already has a live
    // session or the code is used by another live session (the two unique
    // indexes). Returns its details (no inputs).
    Result<Session> createSession(int courseId, int hostId,
                                  const std::string& name,
                                  const std::string& joinCode,
                                  Clock::time_point startTime);

    // Details only: no inputs. Recording included if attached.
    Result<Session> getSession(int sessionId);

    // Only finds LIVE sessions.
    Result<Session> findLiveSessionByJoinCode(const std::string& joinCode);

    // Details + every Lost ping and question (raw, questions with their
    // answers), each ordered by time then id.
    Result<Session> loadSession(int sessionId);

    // One summary per session of the course, newest start first, with the
    // status and the total Lost and Question counts (AppModel blanks the
    // counts of in-review sessions for students, S12).
    std::vector<SessionSummary> listSessionsForCourse(int courseId);

    // Sets end_date and status 'in_review'. False if there is no such
    // session or it is not live.
    bool setSessionEnd(int sessionId, Clock::time_point endTime);

    // [F4] Sets published_at and status 'published'. False if there is no
    // such session or it is not in review.
    bool setSessionPublished(int sessionId, Clock::time_point publishedTime);

    // [F4] U9
    bool renameSession(int sessionId, const std::string& name);
    bool deleteSession(int sessionId);  // pings, questions and answers go with it

    // Attaches or replaces / removes the recording.
    bool setRecording(int sessionId, const Recording& recording);
    bool clearRecording(int sessionId);  // [F5]

    // [F2] Lost pings [F2]
    // Inserts one ping. Returns it raw, with id, session id and author.
    // nullopt if the session or user doesn't exist.
    Result<LostPing> addLostPing(int sessionId, int userId, Clock::time_point time);
    Result<LostPing> getLostPing(int pingId);  // raw
    std::vector<LostPing> listLostPings(int sessionId);  // raw, by time then id
    // [S] I8: time of the user's latest Lost in the session, if any.
    std::optional<Clock::time_point> getLastLostTime(int sessionId, int userId);
    // [F3] False if no such ping.
    bool deleteLostPing(int pingId);

    // [F2] Questions [F2]
    // Inserts one question. Returns it raw, with id, session id and author.
    // nullopt if the session or user doesn't exist.
    Result<SessionQuestion> addQuestion(int sessionId, int userId,
                                         Clock::time_point time, const std::string& text);
    Result<SessionQuestion> getQuestion(int questionId);  // raw
    std::vector<SessionQuestion> listQuestions(int sessionId);  // raw, by time then id
    // [F3] Sets the text and edited_time. False if no such question.
    bool updateQuestionText(int questionId, const std::string& text,
                             Clock::time_point editedTime);
    // [F3] Its answers go with it (CASCADE). False if no such question.
    bool deleteQuestion(int questionId);

    // [F4] Answers [F4]
    // Returns it raw, with author and isByEducator() (author is an educator of
    // the question's course right now, A6). nullopt if the question or user
    // doesn't exist.
    Result<Answer> addAnswer(int questionId, int userId, const std::string& text,
                              Clock::time_point time);
    Result<Answer> getAnswer(int answerId);
    // Sets the text and edited_time (A8). False if no such answer.
    bool updateAnswerText(int answerId, const std::string& text,
                           Clock::time_point editedTime);
    bool deleteAnswer(int answerId);

private:
    bool createTables();     // schema.sql
    bool migrateV1ToV2();    // migration_v1_to_v2.sql

    // What kind of file was opened: 0 = empty, 1 = made by the M2 app
    // (user_version 0 and a Users table with a user_password column),
    // otherwise PRAGMA user_version.
    int detectVersion();

    // Converts between a time and the UTC text stored in the tables.
    static std::string convertTimeToString(Clock::time_point time);
    static std::chrono::system_clock::time_point
    convertStringToTime(const std::string& text);

    QString path;
    QString connectionName;  // unique per object
    QSqlDatabase db;
    int schemaVersion = 0;
};