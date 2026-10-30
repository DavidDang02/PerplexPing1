// MODEL (storage): saves and loads Users, Sessions and SessionInputs in a
// SQLite file using Qt SQL. Translates between table rows and model objects;
// the rules live in the model classes, not here.
//
// Tables (created by open() if missing):
//   User          (userID INTEGER PRIMARY KEY, name TEXT UNIQUE NOT NULL,
//                  passwordHash TEXT NOT NULL, salt TEXT NOT NULL)
//   Session       (sessionID INTEGER PRIMARY KEY, userID INTEGER NOT NULL,
//                  startTime INTEGER NOT NULL, endTime INTEGER)        -- NULL while running
//   SessionInputs (inputID INTEGER PRIMARY KEY, sessionID INTEGER NOT NULL,
//                  timestamp INTEGER NOT NULL, isQuestion INTEGER NOT NULL,  -- 0 or 1
//                  questionText TEXT)                                  -- NULL for Lost
// All times are milliseconds since 1970 (INTEGER), which sort and compare
// easily and convert directly to std::chrono.

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
    // Each Database gets its own Qt connection name, so tests can make many.
    explicit Database(const QString& path);
    ~Database();

    // Opens the file and creates the tables if they don't exist.
    // Returns false if the file can't be opened.
    bool open();

    // ---- Users ----
    // Saves a new user with a salted hash of the password.
    // nullopt if the name is empty, the password is empty, or the name is taken.
    std::optional<User> registerUser(const std::string& name, const std::string& password);

    // nullopt if there is no such user or the password is wrong.
    std::optional<User> login(const std::string& name, const std::string& password);

    // ---- Sessions ----
    // Inserts a running session for userId, starting now, and returns it
    // with its new sessionID.
    Session createSession(int userId);

    // Saves one input of a session. Call after Session::addInput returned true.
    void saveInput(int sessionId, const SessionInput& input);

    // Saves the end time. Call after Session::endSession.
    void saveSessionEnd(int sessionId, Session::Clock::time_point endTime);

    // Rebuilds a Session (with all its inputs, in time order) from the tables.
    // nullopt if there is no such session.
    std::optional<Session> loadSession(int sessionId);

    // Every session the user started, newest first, each with its inputs.
    std::vector<Session> listSessionsForUser(int userId);

private:
    QString connectionName;
};
