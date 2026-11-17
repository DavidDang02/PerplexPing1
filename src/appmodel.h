// MODEL: the coordinator for the app. Remembers who is logged in and which
// session is running, applies the Session rules, and is the only class that
// calls DataStore. The controller talks to this class, never to a Database or
// any concrete implementation: both the local SQLite Database and the remote
// ServerClient (Qt WebSocket) implement DataStore.
//
// Every method that can fail returns Result<T>: check isOk(), or read error.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "datastore.h"
#include "error.h"
#include "session.h"
#include "sessioninput.h"
#include "user.h"

class AppModel {
public:
    static constexpr std::size_t kMaxUsernameLength = 64;
    static constexpr std::size_t kMaxPasswordLength = 64;

    explicit AppModel(DataStore* store);  // not owned

    // ---- Accounts ----
    // Both log the user in on success and return Result<User>.
    // The name is trimmed first, so " ana " and "ana" are the same user.
    Result<User> registerUser(const std::string& name, const std::string& password);
    Result<User> logIn(const std::string& name, const std::string& password);
    void logOut();  // also forgets any running session

    bool isLoggedIn() const;
    const User* getCurrentUser() const;  // nullptr when logged out

    // ---- The running session ----
    // Starts and saves a new session for the logged-in user.
    Result<Session> startSession();

    // Record and save one input.
    Result<SessionInput> clickLostButton();
    Result<SessionInput> askQuestion(const std::string& text);

    // Ends and saves the running session.
    Result<Session> endSession();

    bool hasRunningSession() const;
    const Session* getCurrentSession() const;  // nullptr when none

    // ---- History ----
    // Every session the logged-in user started, newest first.
    Result<std::vector<Session>> getPastSessions() const;

private:
    DataStore* store;
    std::optional<User> currentUser;
    std::optional<Session> currentSession;
};