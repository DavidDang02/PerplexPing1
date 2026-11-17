// MODEL (storage): abstraction over how the app stores data. The local
// Database (SQLite, for the offline prototype / tests) and the ServerClient
// (Qt WebSocket to the central PerplexPing server) both implement this
// interface, so AppModel never cares which one is in use.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "session.h"
#include "sessioninput.h"
#include "user.h"

// Why logging in or registering failed. None means it worked.
// Login doesn't say whether the name or the password was wrong, so the app
// never reveals which usernames exist.
enum class AuthError { None, EmptyName, EmptyPassword, NameTaken, WrongLogin };

class DataStore {
public:
    virtual ~DataStore() = default;

    // ---- Accounts ----
    // Returns the User on success, std::nullopt if validation failed (empty
    // fields) or the name is taken / login failed.
    virtual std::optional<User> registerUser(const std::string& name, const std::string& password) = 0;
    virtual std::optional<User> login(const std::string& name, const std::string& password) = 0;

    // ---- Sessions ----
    // Inserts a running session for userId, starting now, and returns it with
    // the id it picked.
    virtual Session createSession(int userId) = 0;

    // Saves one input of a session and sets its id. Call after
    // Session::addInput returned true.
    virtual void saveInput(int sessionId, int userId, SessionInput& input) = 0;

    // Saves the end time. Call after Session::endSession.
    virtual void saveSessionEnd(int sessionId, Session::Clock::time_point endTime) = 0;

    // Rebuilds a Session (with all its inputs, in the order they were saved).
    // nullopt if there is no such session.
    virtual std::optional<Session> loadSession(int sessionId) = 0;

    // Every session the user started, newest first, each with its inputs.
    virtual std::vector<Session> listSessionsForUser(int userId) = 0;
};
