// MODEL (storage): saves and loads Users, Sessions and SessionInputs in a
// SQLite file using Qt SQL. Translates between table rows and model objects;
// the rules live in the model classes, not here. Implements the DataStore
// interface so AppModel can hold a local SQLite store or a remote one.
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

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <QSqlDatabase>
#include <QString>

#include "datastore.h"
#include "session.h"
#include "sessioninput.h"
#include "user.h"

class Database : public DataStore {
public:
    // path is a file such as "perplexping.db", or ":memory:" in tests.
    // Uses Qt's default connection, so open only one Database at a time.
    explicit Database(const QString& path);
    ~Database() override;

    // Opens the file and creates the tables if they don't exist.
    // Returns false if the file can't be opened. (Not part of DataStore:
    // the remote ServerClient is connected by other means.)
    bool open();

    // ---- Accounts ----
    std::optional<User> registerUser(const std::string& name, const std::string& password) override;
    std::optional<User> login(const std::string& name, const std::string& password) override;

    // ---- Sessions ----
    Session createSession(int userId) override;
    void saveInput(int sessionId, int userId, SessionInput& input) override;
    void saveSessionEnd(int sessionId, Session::Clock::time_point endTime) override;
    std::optional<Session> loadSession(int sessionId) override;
    std::vector<Session> listSessionsForUser(int userId) override;

private:
    // Converts between a time and the UTC text stored in the tables,
    // e.g. "2026-10-31T00:53:00Z".
    std::string convertTimeToString(std::chrono::system_clock::time_point time);
    std::chrono::system_clock::time_point convertStringToTime(std::string time);

    QString connectionName;
    QSqlDatabase db;
};
