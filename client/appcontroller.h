// CONTROLLER: everything the app does between the pages and the server.
//
//   page ──slot call──▶ AppController ──request──▶ ServerClient ══▶ server
//   page ◀──signal───── AppController ◀──reply / push──────────────┘
//
// Pages call the slots below when the user does something, and listen to
// the signals to know what to show. The controller:
//   * sends the matching request (CONTRACT.md 6),
//   * changes what it holds ONLY after the server says yes (so the screen
//     never shows something that didn't save),
//   * applies pushes from other users (live pings, answers, renames...),
//   * says which page should be on screen (pageChanged),
//   * reports every failure as an Error (requestFailed); the page words it
//     with ViewText::getErrorText. The controller never builds display text.
//
// It holds no widgets, so the tests drive it directly against a real
// server (tests/test_e2e.cpp). Rules are NOT checked here: the server's
// AppModel decides; the controller only stops a double-send while the
// first one is still on its way.
//
// What each page shows comes from the getters (getCourses(),
// getOpenCourse(), getOpenSession(), ...). They are always the latest
// state the server confirmed, already filtered by the search boxes.

#pragma once

#include <map>
#include <optional>
#include <set>
#include <vector>

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUrl>

#include "course.h"
#include "error.h"
#include "filters.h"
#include "session.h"
#include "user.h"

class ServerClient;
class PanoptoClient;

class AppController : public QObject {
    Q_OBJECT

public:
    // Which page should be on screen.
    enum class Page {
        Login,          // log in / create account
        Courses,        // My Courses: list, create, join by course code, join a session by code
        Course,         // one course: its sessions (search box), start a session, settings
        StudentLive,    // a live session, as a student: I'm Lost, ask, own items
        EducatorLive,   // a live session, as its educator: counts, timeline so far, participants, End
        SessionViewer,  // an ended session: review (educator) or published (everyone): timeline, Q&A, video
    };
    Q_ENUM(Page)

    // Things that worked and deserve a short confirmation on screen. The
    // page words them (ViewText::getNoticeText).
    enum class Notice {
        Registered,
        LoggedOut,
        CourseCreated,
        CourseJoined,
        CourseUpdated,
        CourseDeleted,          // you deleted it, or its educator did (you are taken home)
        SessionStarted,
        SessionJoined,
        LostSent,
        QuestionSent,
        QuestionUpdated,
        QuestionDeleted,
        SessionEndedByEducator, // students: the educator ended it; you are back on the course page
        SessionEnded,           // educator: it's now in review
        SessionPublished,
        SessionRenamed,
        SessionDeleted,         // also when the educator deleted the session you had open
        AnswerSent,
        AnswerUpdated,
        AnswerDeleted,
        RecordingAttached,
        RecordingRemoved,
        PanoptoLinked,
        PasswordChanged,
        EmailUpdated,
        AccountDeleted,         // also when the account was deleted on another device
        EducatorJoined,         // the educator joined their own live session by its code
        LoggedOutElsewhere,     // the password changed on another device while offline
        Reconnected,
    };
    Q_ENUM(Notice)

    // `panopto` may be nullptr (then Find recording reports
    // PanoptoNotConfigured). Neither is owned.
    explicit AppController(ServerClient* client, PanoptoClient* panopto = nullptr,
                           QObject* parent = nullptr);

    // ---- What the pages show (always the latest confirmed state) ----
    Page getPage() const;
    bool isConnected() const;
    const User* getUser() const;  // nullptr when logged out
    const std::vector<Course>& getCourses() const;  // My Courses, sorted by name
    // The open course page (or the course of the open session).
    const std::optional<CourseDetail>& getOpenCourse() const;
    // The open course's sessions, filtered by the session search box.
    std::vector<SessionSummary> getVisibleSessions() const;
    // The open session as this user may see it, or nullopt.
    const std::optional<Session>& getOpenSession() const;
    // The open session's questions, filtered by the question search box and
    // the "Unanswered only" toggle.
    std::vector<SessionQuestion> getVisibleQuestions() const;
    // The open session's timeline bars and the peak bar (-1 if none).
    std::vector<Timeline::Bucket> getTimeline() const;
    // The students in the open live session (educator only; names empty in
    // an anonymous course).
    const std::vector<Participant>& getParticipants() const;
    // The role in the open course (None if no course is open).
    CourseRole getRole() const;
    // The name of the open course's Panopto folder, if it was linked from
    // this app since it started ("" otherwise; the server keeps only the id).
    QString getPanoptoFolderName() const;
    bool isBusy() const;  // a request this controller sent is still waiting

public slots:
    // ---- Account ----
    void logIn(const QString& name, const QString& password);
    void registerUser(const QString& name, const QString& password, const QString& email);
    void logOut();
    void changePassword(const QString& oldPassword, const QString& newPassword);
    void updateEmail(const QString& email);
    void deleteAccount(const QString& password);  // the page confirms first

    // ---- Courses ----
    void goHome();  // My Courses (refreshes the list)
    void createCourse(const QString& name, bool anonymous, bool questionsShared);
    void joinCourse(const QString& courseCode);
    void openCourse(int courseId);
    void updateCourse(const QString& name, bool anonymous, bool questionsShared);  // the open course
    void deleteCourse();  // the open course; the page confirms first
    void setSessionSearch(const QString& query);

    // ---- Sessions ----
    void startSession(const QString& name);  // in the open course
    void joinSession(const QString& sessionCode);
    void openSession(int sessionId);
    void leaveSession();  // back to the course page
    void endSession();    // the open one; the page confirms first
    void publishSession();
    void renameSession(int sessionId, const QString& name);
    void deleteSession(int sessionId);  // the page confirms first

    // ---- Live (students) ----
    void pressLost();
    void deleteLost(int pingId);
    void askQuestion(const QString& text);
    void editQuestion(int questionId, const QString& text);
    void deleteQuestion(int questionId);  // also the educator's "remove" after the session

    // ---- Answers (after the session) ----
    void addAnswer(int questionId, const QString& text);
    void editAnswer(int answerId, const QString& text);
    void deleteAnswer(int answerId);
    void setQuestionSearch(const QString& query, bool unansweredOnly);

    // ---- Recording (educator, after the session) ----
    // `startsAt` is where the session starts in the video, as typed:
    // "4:30", "1:02:05" or "" for 0:00 (Recording::parseTime).
    void attachRecording(const QString& url, const QString& startsAt);
    void removeRecording();
    // One-time setup: the educator signs in to Panopto (PanoptoClient) and
    // pastes any lecture link of the course; the controller works out its
    // folder and links it (linkPanoptoFolder).
    void linkPanoptoFolder(const QString& lectureLink);
    // "Find recording": asks Panopto for the linked folder's recordings and
    // lets the server pick the one that overlaps the session.
    void findRecording();

    // ---- Timeline ----
    // A click on the timeline at this many seconds into the session: emits
    // playRecording with the Panopto link at that moment (a little before,
    // Recording::kLeadInSeconds). Nothing if there is no recording.
    void jumpTo(long long sessionSeconds);

signals:
    void pageChanged(AppController::Page page);
    // Something on the current page changed: redraw it from the getters.
    void stateChanged();
    void participantsChanged();
    void connectionChanged(bool connected);
    void busyChanged(bool busy);
    // `request` is the Protocol::Request name that failed (so the page can
    // show the error next to the right box), e.g. kAddQuestion.
    void requestFailed(const QString& request, Error error);
    void notice(AppController::Notice notice);
    // The video player should load this link (Panopto, at a moment).
    void playRecording(const QUrl& url);

private:
    using Handler = std::function<void(const QJsonObject& data)>;

    // Sends a request; on success runs onOk(reply["data"]), on failure emits
    // requestFailed. The very same request (type and fields) is not sent
    // twice at once: a second Lost click while the first is on its way does
    // nothing, but answering two different questions quickly sends both.
    void request(const QString& type, const QJsonObject& fields, Handler onOk,
                 std::function<void(Error)> onFail = nullptr);
    void setPage(Page page);
    void onPush(const QJsonObject& push);
    void onConnectionChanged(bool connected);

    // Applying confirmed data.
    void setCourses(const QJsonObject& data);
    void setCourseDetail(const QJsonObject& data);
    void setSessionFromJson(const QJsonObject& data);  // full or details-only (keeps items)
    void showSessionPage();  // the right page for the open session and role
    void refreshCourses();
    void refreshCourse();
    void reloadSession();
    void closeOpenSession();  // tells the server, forgets the session (no page change)
    bool isOpen(int sessionId) const;  // that session is the one on screen
    void upsertIntoOpenSession(const QJsonObject& questionJson);
    // The user moved (another page, logged out) since `mark` was taken:
    // a reply to a page load from before then is dropped.
    bool hasMovedSince(int mark) const;
    void clearAll();

    ServerClient* client;
    PanoptoClient* panopto;
    Page page = Page::Login;
    bool connected = false;
    std::optional<User> user;
    std::vector<Course> courses;
    std::optional<CourseDetail> course;
    std::optional<Session> session;
    std::vector<Participant> participants;
    std::string sessionQuery;
    std::string questionQuery;
    bool unansweredOnly = false;
    std::set<QString> inFlight;
    int navigation = 0;  // +1 on every move to another page or user
    std::map<int, QString> folderNames;  // course id -> linked Panopto folder name
    // Kept in memory only (never on disk), to log back in silently after the
    // connection drops; cleared on log out.
    QString rememberedName;
    QString rememberedPassword;
};
