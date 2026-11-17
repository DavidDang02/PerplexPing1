// DATASTORE: the pure interface for persistence. Both the local SQLite
// Database and the remote ServerClient (Qt WebSocket) implement this.
//
// All methods that produce a value or can fail return Result<T>. Check
// isOk() on the result, then read .value; on failure read .error.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "error.h"
#include "session.h"
#include "sessioninput.h"
#include "user.h"

class DataStore {
public:
    virtual ~DataStore() = default;

    // ---- Accounts ----
    // Create a new user account. Fails if the username is already taken
    // (Error::NameTaken) or on storage problems (Error::StorageError).
    virtual Result<User> registerUser(const std::string& name,
                                      const std::string& password) = 0;

    // Verify credentials and return the user. Fails with Error::WrongLogin
    // on bad credentials, Error::StorageError on storage problems.
    virtual Result<User> logIn(const std::string& name,
                               const std::string& password) = 0;

    // ---- Sessions ----
    // Create a new running session for the given user. Returns the session
    // with the storage-assigned id.
    virtual Result<Session> createSession(int userId) = 0;

    // Save an input for a running session. The input is passed by value so
    // the implementation can assign the storage id and return the saved copy.
    // Fails with Error::StorageError.
    virtual Result<SessionInput> saveInput(int sessionId, int userId,
                                           SessionInput input) = 0;

    // Mark a session as ended at the given time. Returns true on success.
    virtual Result<bool> saveSessionEnd(int sessionId,
                                        Session::Clock::time_point endTime) = 0;

    // Load a single session by id. Returns the session or Error::StorageError.
    virtual Result<Session> loadSession(int sessionId) = 0;

    // All sessions started by this user, newest first.
    virtual Result<std::vector<Session>> listSessionsForUser(int userId) = 0;
};