// MODEL: the coordinator for the app. Remembers who is logged in and which
// session is running, applies the Session rules, and is the only class that
// calls Database. The controller talks to this class, never to Database.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "session.h"
#include "user.h"

class Database;

// Why logging in or registering failed. None means it worked.
// Login doesn't say whether the name or the password was wrong, so the app
// never reveals which usernames exist.
enum class AuthError { None, EmptyName, EmptyPassword, NameTaken, WrongLogin };

class AppModel {
public:
    explicit AppModel(Database* db);  // not owned

    // ---- Accounts ----
    // Both log the user in on success and return AuthError::None.
    // The name is trimmed first, so " ana " and "ana" are the same user.
    AuthError registerUser(const std::string& name, const std::string& password);
    AuthError logIn(const std::string& name, const std::string& password);
    void logOut();  // also forgets any running session

    bool isLoggedIn() const;
    const User* getCurrentUser() const;  // nullptr when logged out

    // ---- The running session ----
    // Starts and saves a new session for the logged-in user. False if logged
    // out or a session is already running.
    bool startSession();

    // Record and save one input. False if there is no running session, it has
    // ended, or (for questions) the text is empty.
    bool clickLostButton();
    bool askQuestion(const std::string& text);

    // Ends and saves the running session. Does nothing if there is none.
    void endSession();

    bool hasRunningSession() const;
    const Session* getCurrentSession() const;  // nullptr when none

    // ---- History ----
    // Every session the logged-in user started, newest first. Empty if logged out.
    std::vector<Session> getPastSessions() const;

private:
    Database* db;
    std::optional<User> currentUser;
    std::optional<Session> currentSession;
};
