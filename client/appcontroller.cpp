#include "appcontroller.h"

#include <QJsonArray>

#include "panoptoclient.h"
#include "protocol.h"
#include "recording.h"
#include "serverclient.h"

using namespace Protocol;

namespace {

// User actions that must not be sent twice while the first is on its way
// (double-click on Lost, Enter pressed twice on a question...). Loads and
// refreshes are not in here: a second one is harmless.
bool isGuarded(const QString& type) {
    static const std::set<QString> guarded = {
        Request::kRegister,      Request::kLogin,         Request::kCreateCourse,
        Request::kJoinCourse,    Request::kStartSession,  Request::kJoinSession,
        Request::kEndSession,    Request::kPublishSession, Request::kAddLost,
        Request::kAddQuestion,   Request::kAddAnswer,     Request::kDeleteAccount,
        Request::kDeleteCourse,  Request::kCheckRecording, Request::kLinkPanoptoFolder,
        Request::kAttachRecording,
    };
    return guarded.count(type) > 0;
}

}  // namespace

AppController::AppController(ServerClient* client, PanoptoClient* panopto, QObject* parent)
    : QObject(parent), client(client), panopto(panopto) {
    connect(client, &ServerClient::pushReceived, this, &AppController::onPush);
    connect(client, &ServerClient::connectionChanged, this, &AppController::onConnectionChanged);
    connected = client->isConnected();
}

// ================= Getters =================

AppController::Page AppController::getPage() const { return page; }
bool AppController::isConnected() const { return connected; }
const User* AppController::getUser() const { return user ? &*user : nullptr; }
const std::vector<Course>& AppController::getCourses() const { return courses; }
const std::optional<CourseDetail>& AppController::getOpenCourse() const { return course; }
const std::optional<Session>& AppController::getOpenSession() const { return session; }
const std::vector<Participant>& AppController::getParticipants() const { return participants; }
bool AppController::isBusy() const { return !inFlight.empty(); }

std::vector<SessionSummary> AppController::getVisibleSessions() const {
    if (!course) {
        return {};
    }
    return Filters::filterSessions(course->sessions, sessionQuery);
}

std::vector<SessionQuestion> AppController::getVisibleQuestions() const {
    if (!session) {
        return {};
    }
    return Filters::filterQuestions(session->getQuestions(), questionQuery, unansweredOnly);
}

std::vector<Timeline::Bucket> AppController::getTimeline() const {
    if (!session) {
        return {};
    }
    return Timeline::getBuckets(*session);
}

CourseRole AppController::getRole() const {
    return course ? course->course.getRole() : CourseRole::None;
}

// ================= Plumbing =================

void AppController::request(const QString& type, const QJsonObject& fields, Handler onOk,
                            std::function<void(Error)> onFail) {
    const bool guarded = isGuarded(type);
    if (guarded && inFlight.count(type) > 0) {
        return;  // the same action is already on its way
    }
    const bool wasBusy = isBusy();
    if (guarded) {
        inFlight.insert(type);
    }
    if (!wasBusy && isBusy()) {
        emit busyChanged(true);
    }
    client->send(type, fields, [this, type, guarded, onOk = std::move(onOk),
                                onFail = std::move(onFail)](const QJsonObject& reply) {
        if (guarded) {
            inFlight.erase(type);
            if (!isBusy()) {
                emit busyChanged(false);
            }
        }
        const Error error = ServerClient::getError(reply);
        if (error != Error::None) {
            if (onFail) {
                onFail(error);
            }
            emit requestFailed(type, error);
            return;
        }
        if (onOk) {
            onOk(reply[Field::kData].toObject());
        }
    });
}

void AppController::setPage(Page newPage) {
    if (page != newPage) {
        page = newPage;
        emit pageChanged(page);
    }
    emit stateChanged();
}

void AppController::clearAll() {
    user.reset();
    courses.clear();
    course.reset();
    session.reset();
    participants.clear();
    sessionQuery.clear();
    questionQuery.clear();
    unansweredOnly = false;
    rememberedName.clear();
    rememberedPassword.clear();
}

void AppController::setCourses(const QJsonObject& data) {
    courses = coursesFromJson(data[Field::kCourses].toArray());
}

void AppController::setCourseDetail(const QJsonObject& data) {
    CourseDetail detail;
    detail.course = courseFromJson(data);
    detail.sessions = summariesFromJson(data[Field::kSessions].toArray());
    course = std::move(detail);
}

void AppController::setSessionFromJson(const QJsonObject& data) {
    Session fresh = sessionFromJson(data);
    // A details-only session (end, publish, rename, recording) has no item
    // keys: keep the pings and questions already on screen.
    if (!data.contains(Field::kLostPings) && session && session->getId() == fresh.getId()) {
        fresh.setLostPings(session->getLostPings());
        fresh.setQuestions(session->getQuestions());
    }
    if (!fresh.isLive()) {
        participants.clear();
    }
    session = std::move(fresh);
}

void AppController::showSessionPage() {
    if (!session) {
        setPage(course ? Page::Course : Page::Courses);
        return;
    }
    if (session->isLive()) {
        setPage(getRole() == CourseRole::Educator ? Page::EducatorLive : Page::StudentLive);
    } else {
        setPage(Page::SessionViewer);
    }
}

void AppController::refreshCourses() {
    request(Request::kListCourses, {}, [this](const QJsonObject& data) {
        setCourses(data);
        emit stateChanged();
    });
}

void AppController::refreshCourse() {
    if (!course) {
        return;
    }
    const int courseId = course->course.getId();
    request(Request::kGetCourse, {{Field::kCourseId, courseId}}, [this, courseId](const QJsonObject& data) {
        // Ignore it if the user moved to another course meanwhile.
        if (course && course->course.getId() == courseId) {
            setCourseDetail(data);
            emit stateChanged();
        }
    });
}

void AppController::reloadSession() {
    if (!session) {
        return;
    }
    const int sessionId = session->getId();
    request(Request::kOpenSession, {{Field::kSessionId, sessionId}},
        [this, sessionId](const QJsonObject& data) {
            if (session && session->getId() == sessionId) {
                setSessionFromJson(data);
                showSessionPage();
            }
        },
        [this, sessionId](Error error) {
            // E.g. a student, after the educator ended it (now in review).
            if (session && session->getId() == sessionId &&
                (error == Error::SessionNotPublished || error == Error::NoSuchSession)) {
                session.reset();
                participants.clear();
                setPage(course ? Page::Course : Page::Courses);
            }
        });
}

// ================= Account =================

void AppController::logIn(const QString& name, const QString& password) {
    request(Request::kLogin, {{Field::kName, name}, {Field::kPassword, password}},
        [this, name, password](const QJsonObject& data) {
            clearAll();
            user = userFromJson(data);
            rememberedName = name;
            rememberedPassword = password;
            setPage(Page::Courses);
            refreshCourses();
        });
}

void AppController::registerUser(const QString& name, const QString& password, const QString& email) {
    request(Request::kRegister,
        {{Field::kName, name}, {Field::kPassword, password}, {Field::kEmail, email}},
        [this, name, password](const QJsonObject& data) {
            clearAll();
            user = userFromJson(data);
            rememberedName = name;
            rememberedPassword = password;
            emit notice(Notice::Registered);
            setPage(Page::Courses);
            refreshCourses();
        });
}

void AppController::logOut() {
    // Leave the app logged out even if the server can't be reached.
    request(Request::kLogout, {}, nullptr);
    clearAll();
    emit notice(Notice::LoggedOut);
    setPage(Page::Login);
}

void AppController::changePassword(const QString& oldPassword, const QString& newPassword) {
    request(Request::kChangePassword,
        {{Field::kOldPassword, oldPassword}, {Field::kNewPassword, newPassword}},
        [this, newPassword](const QJsonObject&) {
            rememberedPassword = newPassword;
            emit notice(Notice::PasswordChanged);
        });
}

void AppController::updateEmail(const QString& email) {
    request(Request::kUpdateEmail, {{Field::kEmail, email}}, [this](const QJsonObject& data) {
        user = userFromJson(data);
        emit notice(Notice::EmailUpdated);
        emit stateChanged();
    });
}

void AppController::deleteAccount(const QString& password) {
    request(Request::kDeleteAccount, {{Field::kPassword, password}}, [this](const QJsonObject&) {
        clearAll();
        emit notice(Notice::AccountDeleted);
        setPage(Page::Login);
    });
}

// ================= Courses =================

void AppController::closeOpenSession() {
    if (!session) {
        return;
    }
    request(Request::kCloseSession, {}, nullptr);  // stop pushes; nothing to wait for
    session.reset();
    participants.clear();
    questionQuery.clear();
    unansweredOnly = false;
}

void AppController::goHome() {
    closeOpenSession();
    course.reset();
    sessionQuery.clear();
    setPage(Page::Courses);
    refreshCourses();
}

void AppController::createCourse(const QString& name, bool anonymous, bool questionsShared) {
    request(Request::kCreateCourse,
        {{Field::kName, name}, {Field::kAnonymous, anonymous}, {Field::kQuestionsShared, questionsShared}},
        [this](const QJsonObject& data) {
            emit notice(Notice::CourseCreated);
            refreshCourses();
            openCourse(data[Field::kId].toInt());
        });
}

void AppController::joinCourse(const QString& courseCode) {
    request(Request::kJoinCourse, {{Field::kJoinCode, courseCode.trimmed().toUpper()}},
        [this](const QJsonObject& data) {
            emit notice(Notice::CourseJoined);
            refreshCourses();
            openCourse(data[Field::kId].toInt());
        });
}

void AppController::openCourse(int courseId) {
    closeOpenSession();
    request(Request::kGetCourse, {{Field::kCourseId, courseId}}, [this](const QJsonObject& data) {
        const bool sameCourse = course && course->course.getId() == data[Field::kId].toInt();
        if (!sameCourse) {
            sessionQuery.clear();
        }
        setCourseDetail(data);
        setPage(Page::Course);
    });
}

void AppController::updateCourse(const QString& name, bool anonymous, bool questionsShared) {
    if (!course) {
        return;
    }
    request(Request::kUpdateCourse,
        {{Field::kCourseId, course->course.getId()}, {Field::kName, name},
         {Field::kAnonymous, anonymous}, {Field::kQuestionsShared, questionsShared}},
        [this](const QJsonObject&) {
            emit notice(Notice::CourseUpdated);
            refreshCourse();
            refreshCourses();
        });
}

void AppController::deleteCourse() {
    if (!course) {
        return;
    }
    request(Request::kDeleteCourse, {{Field::kCourseId, course->course.getId()}},
        [this](const QJsonObject&) {
            course.reset();
            session.reset();
            emit notice(Notice::CourseDeleted);
            setPage(Page::Courses);
            refreshCourses();
        });
}

void AppController::setSessionSearch(const QString& query) {
    sessionQuery = query.toStdString();
    emit stateChanged();
}

// ================= Sessions =================

void AppController::startSession(const QString& name) {
    if (!course) {
        return;
    }
    request(Request::kStartSession, {{Field::kCourseId, course->course.getId()}, {Field::kName, name}},
        [this](const QJsonObject& data) {
            setSessionFromJson(data);
            participants.clear();
            emit notice(Notice::SessionStarted);
            showSessionPage();
            refreshCourse();  // now shows the live session
        });
}

void AppController::joinSession(const QString& sessionCode) {
    request(Request::kJoinSession, {{Field::kJoinCode, sessionCode.trimmed()}},
        [this](const QJsonObject& data) {
            setSessionFromJson(data);
            const int courseId = session->getCourseId();
            emit notice(Notice::SessionJoined);
            refreshCourses();  // joining by session code may have enrolled them
            // The page depends on the role in that course: load it first.
            request(Request::kGetCourse, {{Field::kCourseId, courseId}}, [this](const QJsonObject& c) {
                setCourseDetail(c);
                showSessionPage();
            });
        });
}

void AppController::openSession(int sessionId) {
    request(Request::kOpenSession, {{Field::kSessionId, sessionId}}, [this](const QJsonObject& data) {
        const int previous = session ? session->getId() : 0;
        setSessionFromJson(data);
        if (previous != session->getId()) {
            questionQuery.clear();
            unansweredOnly = false;
            participants.clear();
        }
        const int courseId = session->getCourseId();
        if (course && course->course.getId() == courseId) {
            showSessionPage();
            return;
        }
        request(Request::kGetCourse, {{Field::kCourseId, courseId}}, [this](const QJsonObject& c) {
            setCourseDetail(c);
            showSessionPage();
        });
    });
}

void AppController::leaveSession() {
    if (!session) {
        return;
    }
    closeOpenSession();
    setPage(course ? Page::Course : Page::Courses);
    refreshCourse();
}

void AppController::endSession() {
    if (!session) {
        return;
    }
    request(Request::kEndSession, {{Field::kSessionId, session->getId()}}, [this](const QJsonObject& data) {
        setSessionFromJson(data);
        emit notice(Notice::SessionEnded);
        showSessionPage();
        refreshCourse();
    });
}

void AppController::publishSession() {
    if (!session) {
        return;
    }
    request(Request::kPublishSession, {{Field::kSessionId, session->getId()}},
        [this](const QJsonObject& data) {
            setSessionFromJson(data);
            emit notice(Notice::SessionPublished);
            emit stateChanged();
            refreshCourse();
        });
}

void AppController::renameSession(int sessionId, const QString& name) {
    request(Request::kRenameSession, {{Field::kSessionId, sessionId}, {Field::kName, name}},
        [this, sessionId](const QJsonObject& data) {
            if (session && session->getId() == sessionId) {
                setSessionFromJson(data);
            }
            emit notice(Notice::SessionRenamed);
            emit stateChanged();
            refreshCourse();
        });
}

void AppController::deleteSession(int sessionId) {
    request(Request::kDeleteSession, {{Field::kSessionId, sessionId}}, [this, sessionId](const QJsonObject&) {
        if (session && session->getId() == sessionId) {
            session.reset();
            participants.clear();
            setPage(course ? Page::Course : Page::Courses);
        }
        emit notice(Notice::SessionDeleted);
        refreshCourse();
    });
}

// ================= Live =================

void AppController::pressLost() {
    if (!session) {
        return;
    }
    const int sessionId = session->getId();
    request(Request::kAddLost, {{Field::kSessionId, sessionId}}, [this, sessionId](const QJsonObject& data) {
        if (session && session->getId() == sessionId && !data.isEmpty()) {
            session->upsertLostPing(lostPingFromJson(data));
        }
        emit notice(Notice::LostSent);
        emit stateChanged();
    });
}

void AppController::deleteLost(int pingId) {
    request(Request::kDeleteLost, {{Field::kPingId, pingId}}, [this, pingId](const QJsonObject&) {
        if (session) {
            session->removeLostPing(pingId);
        }
        emit stateChanged();
    });
}

void AppController::askQuestion(const QString& text) {
    if (!session) {
        return;
    }
    const int sessionId = session->getId();
    request(Request::kAddQuestion, {{Field::kSessionId, sessionId}, {Field::kText, text}},
        [this, sessionId](const QJsonObject& data) {
            if (session && session->getId() == sessionId && !data.isEmpty()) {
                session->upsertQuestion(questionFromJson(data));
            }
            emit notice(Notice::QuestionSent);
            emit stateChanged();
        });
}

void AppController::editQuestion(int questionId, const QString& text) {
    request(Request::kEditQuestion, {{Field::kQuestionId, questionId}, {Field::kText, text}},
        [this](const QJsonObject& data) {
            if (session && !data.isEmpty()) {
                session->upsertQuestion(questionFromJson(data));
            }
            emit notice(Notice::QuestionUpdated);
            emit stateChanged();
        });
}

void AppController::deleteQuestion(int questionId) {
    request(Request::kDeleteQuestion, {{Field::kQuestionId, questionId}}, [this, questionId](const QJsonObject&) {
        if (session) {
            session->removeQuestion(questionId);
        }
        emit notice(Notice::QuestionDeleted);
        emit stateChanged();
    });
}

// ================= Answers =================

void AppController::addAnswer(int questionId, const QString& text) {
    request(Request::kAddAnswer, {{Field::kQuestionId, questionId}, {Field::kText, text}},
        [this](const QJsonObject& data) {
            if (session && !data.isEmpty()) {
                session->upsertQuestion(questionFromJson(data));
            }
            emit notice(Notice::AnswerSent);
            emit stateChanged();
        });
}

void AppController::editAnswer(int answerId, const QString& text) {
    request(Request::kEditAnswer, {{Field::kAnswerId, answerId}, {Field::kText, text}},
        [this](const QJsonObject& data) {
            if (session && !data.isEmpty()) {
                session->upsertQuestion(questionFromJson(data));
            }
            emit notice(Notice::AnswerUpdated);
            emit stateChanged();
        });
}

void AppController::deleteAnswer(int answerId) {
    request(Request::kDeleteAnswer, {{Field::kAnswerId, answerId}}, [this](const QJsonObject& data) {
        if (session && !data.isEmpty()) {
            session->upsertQuestion(questionFromJson(data));
        }
        emit notice(Notice::AnswerDeleted);
        emit stateChanged();
    });
}

void AppController::setQuestionSearch(const QString& query, bool unanswered) {
    questionQuery = query.toStdString();
    unansweredOnly = unanswered;
    emit stateChanged();
}

// ================= Recording =================

void AppController::attachRecording(const QString& url, const QString& startsAt) {
    if (!session) {
        return;
    }
    long long offset = 0;
    const std::string typed = startsAt.trimmed().toStdString();
    if (!typed.empty() && !Recording::parseTime(typed, offset)) {
        emit requestFailed(Request::kAttachRecording, Error::RecordingOffsetInvalid);
        return;
    }
    request(Request::kAttachRecording,
        {{Field::kSessionId, session->getId()}, {Field::kUrl, url},
         {Field::kOffsetSeconds, offset}, {Field::kSource, toText(RecordingSource::Manual)}},
        [this](const QJsonObject& data) {
            setSessionFromJson(data);
            emit notice(Notice::RecordingAttached);
            emit stateChanged();
        });
}

void AppController::removeRecording() {
    if (!session) {
        return;
    }
    request(Request::kRemoveRecording, {{Field::kSessionId, session->getId()}}, [this](const QJsonObject& data) {
        setSessionFromJson(data);
        emit notice(Notice::RecordingRemoved);
        emit stateChanged();
    });
}

void AppController::linkPanoptoFolder(const QString& lectureLink) {
    if (!course) {
        return;
    }
    if (!panopto) {
        emit requestFailed(Request::kLinkPanoptoFolder, Error::PanoptoNotConfigured);
        return;
    }
    const int courseId = course->course.getId();
    auto findAndLink = [this, courseId, lectureLink] {
        panopto->findFolderOfLink(lectureLink, [this, courseId](Error error, const QString& folderId,
                                                                 const QString&) {
            if (error != Error::None) {
                emit requestFailed(Request::kLinkPanoptoFolder, error);
                return;
            }
            request(Request::kLinkPanoptoFolder, {{Field::kCourseId, courseId}, {Field::kFolderId, folderId}},
                [this](const QJsonObject&) {
                    emit notice(Notice::PanoptoLinked);
                    refreshCourse();
                });
        });
    };
    if (panopto->isSignedIn()) {
        findAndLink();
        return;
    }
    panopto->signIn([this, findAndLink](Error error) {
        if (error != Error::None) {
            emit requestFailed(Request::kLinkPanoptoFolder, error);
            return;
        }
        findAndLink();
    });
}

void AppController::findRecording() {
    if (!session || !course) {
        return;
    }
    if (!panopto) {
        emit requestFailed(Request::kCheckRecording, Error::PanoptoNotConfigured);
        return;
    }
    const QString folderId = QString::fromStdString(course->course.getPanoptoFolderId());
    if (folderId.isEmpty()) {
        emit requestFailed(Request::kCheckRecording, Error::PanoptoFolderNotLinked);
        return;
    }
    const int sessionId = session->getId();
    auto listAndMatch = [this, folderId, sessionId] {
        panopto->listRecordings(folderId, [this, sessionId](Error error,
                                                            const std::vector<RecordingCandidate>& found) {
            if (error != Error::None) {
                emit requestFailed(Request::kCheckRecording, error);
                return;
            }
            // Newest first; the server takes at most kMaxCandidates.
            std::vector<RecordingCandidate> candidates(found.begin(),
                found.begin() + std::min<size_t>(found.size(), kMaxCandidates));
            request(Request::kCheckRecording,
                {{Field::kSessionId, sessionId}, {Field::kCandidates, toJson(candidates)}},
                [this, sessionId](const QJsonObject& data) {
                    if (session && session->getId() == sessionId) {
                        setSessionFromJson(data);
                    }
                    emit notice(Notice::RecordingAttached);
                    emit stateChanged();
                });
        });
    };
    if (panopto->isSignedIn()) {
        listAndMatch();
        return;
    }
    panopto->signIn([this, listAndMatch](Error error) {
        if (error != Error::None) {
            emit requestFailed(Request::kCheckRecording, error);
            return;
        }
        listAndMatch();
    });
}

void AppController::jumpTo(long long sessionSeconds) {
    if (!session || !session->getRecording()) {
        return;
    }
    emit playRecording(QUrl(QString::fromStdString(session->getRecording()->getLinkAt(sessionSeconds))));
}

// ================= Pushes =================

void AppController::onPush(const QJsonObject& push) {
    const QString type = push[Field::kType].toString();
    const int sessionId = push[Field::kSessionId].toInt();
    const int courseId = push[Field::kCourseId].toInt();
    const bool isOpenSession = session && sessionId != 0 && session->getId() == sessionId;
    const bool isOpenCourse = course && courseId != 0 && course->course.getId() == courseId;

    if (type == Reply::kLostAdded && isOpenSession) {
        session->upsertLostPing(lostPingFromJson(push[Field::kLostPing].toObject()));
        emit stateChanged();
    } else if (type == Reply::kLostDeleted && isOpenSession) {
        session->removeLostPing(push[Field::kPingId].toInt());
        emit stateChanged();
    } else if ((type == Reply::kQuestionAdded || type == Reply::kQuestionUpdated ||
                type == Reply::kAnswerAdded || type == Reply::kAnswerUpdated ||
                type == Reply::kAnswerDeleted) && isOpenSession) {
        // Answer changes carry the whole question, so one upsert covers all.
        session->upsertQuestion(questionFromJson(push[Field::kQuestion].toObject()));
        emit stateChanged();
    } else if (type == Reply::kQuestionDeleted && isOpenSession) {
        session->removeQuestion(push[Field::kQuestionId].toInt());
        emit stateChanged();
    } else if (type == Reply::kSessionEnded) {
        if (isOpenSession) {
            if (getRole() == CourseRole::Educator) {
                // Ended on the educator's other device: switch to review.
                setSessionFromJson(push[Field::kSession].toObject());
                showSessionPage();
            } else {
                // Students can't open a session in review (S12).
                session.reset();
                participants.clear();
                emit notice(Notice::SessionEndedByEducator);
                setPage(course ? Page::Course : Page::Courses);
            }
        }
        if (course) {
            refreshCourse();
        }
    } else if (type == Reply::kSessionRenamed) {
        if (isOpenSession) {
            session->setName(push[Field::kName].toString().toStdString());
            emit stateChanged();
        }
        if (isOpenCourse) {
            refreshCourse();
        }
    } else if (type == Reply::kSessionDeleted) {
        if (isOpenSession) {
            session.reset();
            participants.clear();
            emit notice(Notice::SessionDeleted);
            setPage(course ? Page::Course : Page::Courses);
        }
        if (isOpenCourse) {
            refreshCourse();
        }
    } else if (type == Reply::kCourseChanged) {
        refreshCourses();
        if (isOpenCourse) {
            refreshCourse();
            // The toggles may have changed what this user may see.
            if (session) {
                reloadSession();
            }
        }
    } else if (type == Reply::kCourseDeleted) {
        refreshCourses();
        if (isOpenCourse) {
            course.reset();
            session.reset();
            participants.clear();
            emit notice(Notice::CourseDeleted);
            setPage(Page::Courses);
        }
    } else if (type == Reply::kParticipants && isOpenSession) {
        participants = participantsFromJson(push[Field::kParticipants].toArray());
        emit participantsChanged();
    } else if (type == Reply::kLoggedOut) {
        // This account was deleted on another device.
        clearAll();
        emit notice(Notice::AccountDeleted);
        setPage(Page::Login);
    }
}

void AppController::onConnectionChanged(bool isUp) {
    connected = isUp;
    emit connectionChanged(isUp);
    if (!isUp || rememberedName.isEmpty()) {
        return;
    }
    // The server forgets a connection when it drops: log back in, then
    // reload whatever was open, so a wifi blip mid-lecture costs nothing.
    const int openCourseId = course ? course->course.getId() : 0;
    const int openSessionId = session ? session->getId() : 0;
    const Page wasOn = page;
    client->send(Request::kLogin, {{Field::kName, rememberedName}, {Field::kPassword, rememberedPassword}},
        [this, openCourseId, openSessionId, wasOn](const QJsonObject& reply) {
            if (ServerClient::getError(reply) != Error::None) {
                // E.g. the password was changed elsewhere: log in again.
                clearAll();
                setPage(Page::Login);
                emit requestFailed(Request::kLogin, ServerClient::getError(reply));
                return;
            }
            user = userFromJson(reply[Field::kData].toObject());
            emit notice(Notice::Reconnected);
            refreshCourses();
            if (openSessionId != 0) {
                openSession(openSessionId);
            } else if (openCourseId != 0 && wasOn == Page::Course) {
                openCourse(openCourseId);
            }
        });
}
