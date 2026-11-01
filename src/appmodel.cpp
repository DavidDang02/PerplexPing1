#include "appmodel.h"

#include "database.h"

AppModel::AppModel(Database* db) : db(db) {}

// ---- Accounts ----

std::optional<User> AppModel::registerUser(const std::string& name, const std::string& password) {
    currentUser = db->registerUser(name, password);
    return currentUser;
}

std::optional<User> AppModel::logIn(const std::string& name, const std::string& password) {
    currentUser = db->login(name, password);
    return currentUser;
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
    if (!currentUser) {
        return false;
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
    db->saveInput(currentSession->getId(), currentSession->getInputs().back());
    return true;
}

bool AppModel::askQuestion(const std::string& text) {
    if (!currentSession || !currentSession->askQuestion(text)) {
        return false;
    }
    db->saveInput(currentSession->getId(), currentSession->getInputs().back());
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
