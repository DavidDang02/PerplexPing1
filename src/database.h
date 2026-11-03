// MODEL (storage): saves and loads Users, Sessions and SessionInputs in a
// SQLite file using Qt SQL. Translates between table rows and model objects;
// the rules live in the model classes, not here.
//
// Tables (created by open() if missing):
//   Users         (user_id INTEGER PRIMARY KEY, user_name UNIQUE NOT NULL,
//                  user_password NOT NULL)            -- plain text in this prototype
//   Sessions      (session_id INTEGER PRIMARY KEY, host_id NOT NULL,
//                  start_date NOT NULL, end_date)      -- end_date is NULL while running
//   Sessioninputs (input_id INTEGER PRIMARY KEY, user_id NOT NULL, session_id NOT NULL,
//                  input_time NOT NULL, input_type NOT NULL,   -- "Lost" or "Question"
//                  input_question)                     -- NULL for Lost
// Times are stored as UTC ISO 8601 text, e.g. "2026-10-31T00:53:00Z".

#pragma once

#include <optional>
#include <string>
#include <vector>
#include <QSqlDatabase>
#include <QString>

#include "session.h"
#include "sessioninput.h"
#include "user.h"

class Database {
public:
    // path is a file such as "perplexping.db", or ":memory:" in tests.
    // Uses Qt's default connection, so open only one Database at a time.
    explicit Database(const QString& path);
    ~Database();

    // Opens the file and creates the tables if they don't exist.
    // Returns false if the file can't be opened.
    bool open();

    // ---- Users ----
    // Saves a new user. nullopt if the name or password is empty, or the name
    // is taken.
    std::optional<User> registerUser(const std::string& name, const std::string& password);

    // nullopt if there is no such user or the password is wrong.
    std::optional<User> login(const std::string& name, const std::string& password);

    // ---- Sessions ----
    // Inserts a running session for userId, starting now, and returns it
    // with its new sessionID.
    Session createSession(int userId);

    // Saves one input of a session and sets its id. Call after
    // Session::addInput returned true.
    void saveInput(int sessionId, int userId, SessionInput& input);

    // Saves the end time. Call after Session::endSession.
    void saveSessionEnd(int sessionId, Session::Clock::time_point endTime);

    // Rebuilds a Session (with all its inputs, in the order they were saved).
    // nullopt if there is no such session.
    std::optional<Session> loadSession(int sessionId);

    // Every session the user started, newest first, each with its inputs.
    std::vector<Session> listSessionsForUser(int userId);

private:
    // Converts between a time and the UTC text stored in the tables,
    // e.g. "2026-10-31T00:53:00Z".
    std::string convertTimeToString(std::chrono::system_clock::time_point time);
    std::chrono::system_clock::time_point convertStringToTime(std::string time);

    QString connectionName;
    QSqlDatabase db;
};
