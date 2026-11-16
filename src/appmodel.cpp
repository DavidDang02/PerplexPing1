#include "appmodel.h"

#include "database.h"
#include "validation.h"

AppModel::AppModel(Database* db) : db(db) {}

// ---- Accounts ----

Result<User> AppModel::registerUser(const std::string& rawName, const std::string& password) {
    const std::string name = Validation::trim(rawName);
    if (Error error = Validation::checkUsername(name); error != Error::None) {
        return error;
    }
    if (Error error = Validation::checkPassword(password); error != Error::None) {
        return error;
    }
    if (!db) {
        return Error::StorageError;
    }
    // The fields are valid, so the database only refuses a name that is taken.
    std::optional<User> created = db->registerUser(name, password);
    if (!created) {
        return Error::NameTaken;
    }
    currentUser = *created;
    currentSession.reset();
    return *currentUser;
}

Result<User> AppModel::logIn(const std::string& rawName, const std::string& password) {
    const std::string name = Validation::trim(rawName);
    if (name.empty()) {
        return Error::EmptyName;
    }
    if (password.empty()) {
        return Error::EmptyPassword;
    }
    if (!db) {
        return Error::StorageError;
    }
    std::optional<User> found = db->login(name, password);
    if (!found) {
        return Error::WrongLogin;
    }
    currentUser = *found;
    currentSession.reset();
    return *currentUser;
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
// Each action: verify login/session/input rules first, then save.

Result<Session> AppModel::startSession() {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (currentSession) {
        return Error::CourseHasLiveSession;
    }
    if (!db) {
        return Error::StorageError;
    }
    // The database saves a new running session (start = now) and returns it
    // with the id it picked.
    Session created = db->createSession(currentUser->getId());
    if (created.getId() <= 0) {
        return Error::StorageError;
    }
    currentSession = created;
    return *currentSession;
}

Result<SessionInput> AppModel::clickLostButton() {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!currentSession) {
        return Error::NoSuchSession;
    }
    if (currentSession->isEnded() || !currentSession->lostButtonClicked()) {
        return Error::SessionEnded;
    }
    if (!db) {
        return Error::StorageError;
    }
    SessionInput input = currentSession->getInputs().back();
    db->saveInput(currentSession->getId(), currentUser->getId(), input);
    return input;
}

Result<SessionInput> AppModel::askQuestion(const std::string& rawText) {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!currentSession) {
        return Error::NoSuchSession;
    }
    if (currentSession->isEnded()) {
        return Error::SessionEnded;
    }
    const std::string text = Validation::trim(rawText);
    if (Error error = Validation::checkQuestion(text); error != Error::None) {
        return error;  // EmptyQuestion or QuestionTooLong
    }
    if (!currentSession->askQuestion(text)) {
        return Error::SessionEnded;
    }
    if (!db) {
        return Error::StorageError;
    }
    SessionInput input = currentSession->getInputs().back();
    db->saveInput(currentSession->getId(), currentUser->getId(), input);
    return input;
}

Result<Session> AppModel::endSession() {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!currentSession) {
        return Error::NoSuchSession;
    }
    if (!db) {
        return Error::StorageError;
    }
    currentSession->endSession();
    db->saveSessionEnd(currentSession->getId(), currentSession->getEndTime());
    Session ended = *currentSession;
    currentSession.reset();
    return ended;
}

bool AppModel::hasRunningSession() const {
    return currentSession.has_value();
}

const Session* AppModel::getCurrentSession() const {
    return currentSession ? &*currentSession : nullptr;
}

// ---- History ----

Result<std::vector<Session>> AppModel::getPastSessions() const {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!db) {
        return Error::StorageError;
    }
    return db->listSessionsForUser(currentUser->getId());
}
