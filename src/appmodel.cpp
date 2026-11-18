#include "appmodel.h"

#include "datastore.h"
#include "validation.h"

AppModel::AppModel(DataStore* store) : store(store) {}

// ---- Accounts ----

Result<User> AppModel::registerUser(const std::string& rawName, const std::string& password) {
    const std::string name = Validation::trim(rawName);
    if (Error error = Validation::checkUsername(name); error != Error::None) {
        return error;
    }
    if (Error error = Validation::checkPassword(password); error != Error::None) {
        return error;
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
    const std::string name = Validation::trim(rawName);
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
    const std::string text = Validation::trim(rawText);
    if (Error error = Validation::checkQuestion(text); error != Error::None) {
        return error;  // EmptyQuestion or QuestionTooLong
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