// LOCAL PERSISTENCE: a SQLite-backed implementation of DataStore. Uses Qt's
// QSqlDatabase (the SQLite driver) and returns Result<T> like every other
// layer, so the model, controller and server all see the same error scheme.

#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <QSqlDatabase>
#include <QString>

#include "datastore.h"

class Database : public DataStore {
public:
    explicit Database(const QString& path);
    ~Database() override;

    // Opens the database and creates the tables if they don't exist yet.
    // Returns false (and prints why) if the file can't be opened.
    bool open();

    // ---- Accounts ----
    Result<User> registerUser(const std::string& name,
                              const std::string& password) override;
    Result<User> logIn(const std::string& name,
                       const std::string& password) override;

    // ---- Sessions ----
    Result<Session> createSession(int userId) override;
    Result<SessionInput> saveInput(int sessionId, int userId,
                                   SessionInput input) override;
    Result<bool> saveSessionEnd(int sessionId,
                                Session::Clock::time_point endTime) override;
    Result<Session> loadSession(int sessionId) override;
    Result<std::vector<Session>> listSessionsForUser(int userId) override;

private:
    QString connectionName;
    QSqlDatabase db;

    // Stored as UTC ISO 8601 text so a time loads back exactly as it was saved.
    static std::string convertTimeToString(std::chrono::system_clock::time_point time);
    static std::chrono::system_clock::time_point convertStringToTime(const std::string& time);
};