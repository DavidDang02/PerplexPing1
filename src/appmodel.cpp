#include "appmodel.h"

#include "database.h"

AppModel::AppModel(Database* db) : db(db) {}

// ---- Accounts ----

// Removes spaces, tabs and newlines from both ends.
static std::string trim(const std::string& text) {
    const char* space = " \t\r\n";
    const auto first = text.find_first_not_of(space);
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(space);
    return text.substr(first, last - first + 1);
}

// The checks both actions share, before asking the database.
static AuthError checkFields(const std::string& name, const std::string& password) {
    if (name.empty()) {
        return AuthError::EmptyName;
    }
    if (password.empty()) {
        return AuthError::EmptyPassword;
    }
    return AuthError::None;
}

AuthError AppModel::registerUser(const std::string& rawName, const std::string& password) {
    const std::string name = trim(rawName);
    if (AuthError error = checkFields(name, password); error != AuthError::None) {
        return error;
    }
    // The fields are valid, so the database only refuses a name that is taken.
    currentUser = db->registerUser(name, password);
    return currentUser ? AuthError::None : AuthError::NameTaken;
}

AuthError AppModel::logIn(const std::string& rawName, const std::string& password) {
    const std::string name = trim(rawName);
    if (AuthError error = checkFields(name, password); error != AuthError::None) {
        return error;
    }
    currentUser = db->login(name, password);
    return currentUser ? AuthError::None : AuthError::WrongLogin;
}

void AppModel::logOut() {
    currentUser.reset();
    currentSession.reset();
}

bool AppModel::isLoggedIn() const {
    return currentUser.has_value();
}

const User* AppModel::getCurrentUser() const {
    return currentUser ? &*currentUser : nullptr;
}

// ---- The running session ----
// Each action: let Session apply its rule first, and only save if it agreed.

bool AppModel::startSession() {
    if (!currentUser || currentSession) {
        return false;  // logged out, or a session is already running
    }
    // The database saves a new running session (start = now) and returns it
    // with the id it picked.
    currentSession = db->createSession(currentUser->getId());
    return true;
}

bool AppModel::clickLostButton() {
    if (!currentSession || !currentSession->lostButtonClicked()) {
        return false;
    }
    SessionInput input = currentSession->getInputs().back();
    db->saveInput(currentSession->getId(), currentUser->getId(), input);
    return true;
}

bool AppModel::askQuestion(const std::string& text) {
    if (!currentSession || !currentSession->askQuestion(text)) {
        return false;
    }
    SessionInput input = currentSession->getInputs().back();
    db->saveInput(currentSession->getId(), currentUser->getId(), input);
    return true;
}

void AppModel::endSession() {
    if (!currentSession) {
        return;
    }
    currentSession->endSession();
    db->saveSessionEnd(currentSession->getId(), currentSession->getEndTime());
    currentSession.reset();
}

bool AppModel::hasRunningSession() const {
    return currentSession.has_value();
}

const Session* AppModel::getCurrentSession() const {
    return currentSession ? &*currentSession : nullptr;
}

// ---- History ----

std::vector<Session> AppModel::getPastSessions() const {
    if (!currentUser) {
        return {};
    }
    return db->listSessionsForUser(currentUser->getId());
}
