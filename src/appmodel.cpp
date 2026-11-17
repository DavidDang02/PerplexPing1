#include "appmodel.h"

#include "datastore.h"

AppModel::AppModel(DataStore* store) : store(store) {}

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

Result<User> AppModel::registerUser(const std::string& rawName, const std::string& password) {
    const std::string name = trim(rawName);
    if (name.empty()) {
        return Error::EmptyName;
    }
    if (name.size() > kMaxUsernameLength) {
        return Error::NameTooLong;
    }
    if (password.empty()) {
        return Error::EmptyPassword;
    }
    if (password.size() > kMaxPasswordLength) {
        return Error::PasswordTooLong;
    }
    if (!store) {
        return Error::StorageError;
    }
    // The fields are valid, so the store only refuses a name that is taken.
    Result<User> created = store->registerUser(name, password);
    if (!created.isOk()) {
        return Error::NameTaken;
    }
    currentUser = created.value;
    currentSession.reset();
    return *currentUser;
}

Result<User> AppModel::logIn(const std::string& rawName, const std::string& password) {
    const std::string name = trim(rawName);
    if (name.empty()) {
        return Error::EmptyName;
    }
    if (password.empty()) {
        return Error::EmptyPassword;
    }
    if (!store) {
        return Error::StorageError;
    }
    Result<User> found = store->logIn(name, password);
    if (!found.isOk()) {
        return Error::WrongLogin;
    }
    currentUser = found.value;
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
    if (!store) {
        return Error::StorageError;
    }
    // The store saves a new running session (start = now) and returns it
    // with the id it picked.
    Result<Session> created = store->createSession(currentUser->getId());
    if (!created.isOk() || created.value.getId() <= 0) {
        return Error::StorageError;
    }
    currentSession = created.value;
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
    if (!store) {
        return Error::StorageError;
    }
    SessionInput input = currentSession->getInputs().back();
    Result<SessionInput> saved = store->saveInput(currentSession->getId(), currentUser->getId(), input);
    if (!saved.isOk()) {
        return Error::StorageError;
    }
    return saved;
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
    const std::string text = trim(rawText);
    if (text.empty()) {
        return Error::EmptyQuestion;
    }
    if (!currentSession->askQuestion(text)) {
        return Error::SessionEnded;
    }
    if (!store) {
        return Error::StorageError;
    }
    SessionInput input = currentSession->getInputs().back();
    Result<SessionInput> saved = store->saveInput(currentSession->getId(), currentUser->getId(), input);
    if (!saved.isOk()) {
        return Error::StorageError;
    }
    return saved;
}

Result<Session> AppModel::endSession() {
    if (!currentUser) {
        return Error::NotLoggedIn;
    }
    if (!currentSession) {
        return Error::NoSuchSession;
    }
    if (!store) {
        return Error::StorageError;
    }
    currentSession->endSession();
    store->saveSessionEnd(currentSession->getId(), currentSession->getEndTime());
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
    if (!store) {
        return Error::StorageError;
    }
    return store->listSessionsForUser(currentUser->getId());
}