// End-to-end: the app's real controller stack (AppController + ServerClient)
// against a real PerplexServer over WebSockets, one fresh in-memory
// database per test case. Each case is a whole user flow with an educator
// and students on separate "apps", the way a lecture really goes. These
// run in CI with every push (ctest).
//
// Only the pages are left out: a page just calls these same slots and draws
// from the same getters.

#include "doctest.h"
#include "viewtext.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QUrlQuery>

#include <chrono>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "appcontroller.h"
#include "database.h"
#include "panoptoclient.h"
#include "passwordhasher.h"
#include "perplexserver.h"
#include "protocol.h"
#include "serverclient.h"

namespace {

using Page = AppController::Page;
using Notice = AppController::Notice;
using Clock = std::chrono::system_clock;
namespace Request = Protocol::Request;

bool waitFor(const std::function<bool()>& done, int ms = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return done();
}

void settle(int ms = 150) {
    waitFor([] { return false; }, ms);
}

// A Panopto that answers from memory: one folder, the recordings the test
// puts in it. Counts sign-ins so a test can check one was asked for.
class FakePanopto : public PanoptoClient {
public:
    bool signedIn = false;
    bool refuseSignIn = false;
    int signIns = 0;
    QString folderId = QStringLiteral("1b2c3d4e-0000-4a5b-8c9d-0123456789ab");
    std::vector<RecordingCandidate> recordings;

    bool isSignedIn() const override { return signedIn; }
    void signIn(DoneHandler done) override {
        ++signIns;
        signedIn = !refuseSignIn;
        done(signedIn ? Error::None : Error::PanoptoSignInFailed);
    }
    void findFolderOfLink(const QString& link, FolderHandler done) override {
        if (!link.contains(QStringLiteral("Viewer.aspx?id="))) {
            done(Error::PanoptoLinkInvalid, {}, {});
            return;
        }
        done(Error::None, folderId, QStringLiteral("CSCI 187"));
    }
    void listRecordings(const QString&, RecordingsHandler done) override {
        done(Error::None, recordings);
    }
};

// One running copy of the app, as one person.
struct App {
    ServerClient client;
    FakePanopto panopto;
    AppController controller;
    std::vector<Notice> notices;
    std::vector<std::pair<QString, Error>> failures;
    std::vector<QUrl> played;
    int connectionDrops = 0;

    explicit App(int port)
        : client(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port))),
          controller(&client, &panopto) {
        QObject::connect(&controller, &AppController::notice,
                         [this](Notice n) { notices.push_back(n); });
        QObject::connect(&controller, &AppController::requestFailed,
                         [this](const QString& r, Error e) { failures.push_back({r, e}); });
        QObject::connect(&controller, &AppController::playRecording,
                         [this](const QUrl& url) { played.push_back(url); });
        QObject::connect(&controller, &AppController::connectionChanged, [this](bool up) {
            if (!up) {
                ++connectionDrops;
            }
        });
        client.start();
        REQUIRE(waitFor([this] { return controller.isConnected(); }));
    }

    AppController* operator->() { return &controller; }

    bool got(Notice n) const {
        return std::find(notices.begin(), notices.end(), n) != notices.end();
    }
    // Waits for this request to fail and returns its error (None if it didn't).
    Error failure(const QString& request) {
        waitFor([&] {
            return std::any_of(failures.begin(), failures.end(),
                               [&](const auto& f) { return f.first == request; });
        }, 2000);
        for (const auto& f : failures) {
            if (f.first == request) {
                return f.second;
            }
        }
        return Error::None;
    }
    void signUp(const QString& name) {
        controller.registerUser(name, QStringLiteral("password123"), name + QStringLiteral("@scu.edu"));
        REQUIRE(waitFor([this] { return controller.getPage() == Page::Courses; }));
    }
    const Session& open() const {
        REQUIRE(controller.getOpenSession().has_value());
        return *controller.getOpenSession();
    }
};

// A server on a free port with its own in-memory database.
struct Server {
    Database db{":memory:"};
    std::unique_ptr<PerplexServer> server;
    int port = 0;

    Server() {
        REQUIRE(db.open().isOk());
        start(0);
    }
    void start(int onPort) {
        server = std::make_unique<PerplexServer>(onPort, &db, nullptr, QHostAddress::LocalHost,
                                                 PasswordHasher::kTestIterations);
        REQUIRE(server->listen());
        port = server->port();
    }
    void stop() { server.reset(); }
};

// The usual class: prof made "CSCI 187"; ana and bob joined it and are in
// a live session prof started.
struct Lecture {
    Server server;
    std::unique_ptr<App> prof, ana, bob;
    QString sessionCode;

    explicit Lecture(bool anonymous = false) {
        prof = std::make_unique<App>(server.port);
        ana = std::make_unique<App>(server.port);
        bob = std::make_unique<App>(server.port);
        prof->signUp(QStringLiteral("prof"));
        ana->signUp(QStringLiteral("ana"));
        bob->signUp(QStringLiteral("bob"));

        (*prof)->createCourse(QStringLiteral("CSCI 187"), anonymous, true);
        REQUIRE(waitFor([&] { return (*prof)->getPage() == Page::Course; }));
        const QString courseCode = QString::fromStdString((*prof)->getOpenCourse()->course.getJoinCode());
        REQUIRE(courseCode.size() == 6);

        (*prof)->startSession(QStringLiteral("Lecture 1: Recursion"));
        REQUIRE(waitFor([&] { return (*prof)->getPage() == Page::EducatorLive; }));
        sessionCode = QString::fromStdString(prof->open().getJoinCode());
        REQUIRE(sessionCode.size() == 6);

        // Students join with the session code only (it enrolls them, S5).
        (*ana)->joinSession(sessionCode);
        (*bob)->joinSession(sessionCode);
        REQUIRE(waitFor([&] { return (*ana)->getPage() == Page::StudentLive; }));
        REQUIRE(waitFor([&] { return (*bob)->getPage() == Page::StudentLive; }));
    }
};

}  // namespace

TEST_CASE("E2E: a whole lecture, from sign-up to a published timeline with a video jump") {
    Lecture l;
    App& prof = *l.prof;
    App& ana = *l.ana;
    App& bob = *l.bob;

    // The educator's live page lists who is in the room.
    REQUIRE(waitFor([&] { return prof->getParticipants().size() == 2; }));

    // Live: ana is lost, bob asks a question. The educator sees both at once.
    ana->pressLost();
    REQUIRE(waitFor([&] { return prof.open().getPressCount() == 1; }));
    CHECK(ana.got(Notice::LostSent));
    bob->askQuestion(QStringLiteral("Why does the base case stop it?"));
    REQUIRE(waitFor([&] { return prof.open().getQuestionCount() == 1; }));
    CHECK(prof->getTimeline().front().lostCount == 1);

    // Each student sees only their own items while live.
    settle();
    CHECK(ana.open().getQuestionCount() == 0);
    CHECK(ana.open().getPressCount() == 1);
    CHECK(bob.open().getPressCount() == 0);

    // End: students are sent back to the course page; the educator reviews.
    prof->endSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::SessionViewer; }));
    CHECK(prof.open().isInReview());
    CHECK(prof.open().getPressCount() == 1);  // kept from before the details-only reply
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Course; }));
    CHECK(ana.got(Notice::SessionEndedByEducator));
    const int sessionId = prof.open().getId();

    // A student can't peek while it's in review.
    ana->openSession(sessionId);
    CHECK(ana.failure(Request::kOpenSession) == Error::SessionNotPublished);

    // Review: answer, attach the recording, publish.
    const int questionId = prof.open().getQuestions().front().getId();
    prof->addAnswer(questionId, QStringLiteral("It returns without calling itself again."));
    REQUIRE(waitFor([&] { return !prof.open().getQuestions().front().getAnswers().empty(); }));
    prof->attachRecording(QStringLiteral("https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=abc"),
                          QStringLiteral("4:30"));
    REQUIRE(waitFor([&] { return prof.open().getRecording() != nullptr; }));
    CHECK(prof.open().getRecording()->getOffsetSeconds() == 270);
    prof->publishSession();
    REQUIRE(waitFor([&] { return prof.open().isPublished(); }));

    // Published: the students see the timeline, every question (shared) and
    // the answer, and a click on the timeline plays the lecture there.
    ana->openSession(sessionId);
    REQUIRE(waitFor([&] { return ana->getPage() == Page::SessionViewer; }));
    CHECK(ana.open().getPressCount() == 1);
    REQUIRE(ana.open().getQuestionCount() == 1);
    CHECK(ana.open().getQuestions().front().getAnswers().front().isByEducator());
    ana->jumpTo(60);
    REQUIRE(ana.played.size() == 1);
    const QUrlQuery query(ana.played.front());
    CHECK(query.queryItemValue(QStringLiteral("start")) == QStringLiteral("320"));  // 60 + 270 - 10

    // Bob answers too, after publish; everyone with it open sees it live.
    bob->openSession(sessionId);
    REQUIRE(waitFor([&] { return bob->getPage() == Page::SessionViewer; }));
    bob->addAnswer(questionId, QStringLiteral("Thanks, that makes sense"));
    REQUIRE(waitFor([&] { return ana.open().getQuestions().front().getAnswers().size() == 2; }));

    // The course page lists it with real counts.
    ana->leaveSession();
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Course; }));
    REQUIRE(waitFor([&] {
        const auto sessions = ana->getVisibleSessions();
        return sessions.size() == 1 && sessions.front().isPublished();
    }));
    CHECK(ana->getVisibleSessions().front().lostCount == 1);
}

TEST_CASE("E2E: pressing Lost twice fast sends one ping; a third right after is refused") {
    Lecture l;
    App& ana = *l.ana;
    ana->pressLost();
    ana->pressLost();  // same instant: still on its way, so not sent
    REQUIRE(waitFor([&] { return ana.open().getPressCount() == 1; }));
    settle();
    CHECK(ana.open().getPressCount() == 1);
    CHECK(l.prof->open().getPressCount() == 1);
    // After the reply, a press within the cooldown is refused with a reason.
    ana->pressLost();
    CHECK(ana.failure(Request::kAddLost) == Error::LostTooSoon);
    CHECK(ana.open().getPressCount() == 1);
}

TEST_CASE("E2E: bad input is reported with the right error and changes nothing") {
    Lecture l;
    App& ana = *l.ana;
    ana->askQuestion(QStringLiteral("   "));
    CHECK(ana.failure(Request::kAddQuestion) == Error::EmptyQuestion);
    ana->askQuestion(QString(501, QChar('x')));
    REQUIRE(waitFor([&] { return ana.failures.size() == 2; }));
    CHECK(ana.failures.back().second == Error::QuestionTooLong);
    CHECK(ana.open().getQuestionCount() == 0);

    App late(l.server.port);
    late.signUp(QStringLiteral("carl"));
    late->joinSession(QStringLiteral("000000"));
    CHECK(late.failure(Request::kJoinSession) == Error::NoSuchSessionCode);
    late->joinCourse(QStringLiteral("nope"));
    CHECK(late.failure(Request::kJoinCourse) == Error::NoSuchCourseCode);

    // A student can't end the session or start one.
    ana->endSession();
    CHECK(ana.failure(Request::kEndSession) == Error::NotCourseEducator);

    // Login mistakes.
    App stranger(l.server.port);
    stranger->logIn(QStringLiteral("ana"), QStringLiteral("wrongpassword"));
    CHECK(stranger.failure(Request::kLogin) == Error::WrongLogin);
    stranger->registerUser(QStringLiteral("Ana"), QStringLiteral("password123"), QStringLiteral("x@scu.edu"));
    REQUIRE(waitFor([&] { return stranger.failures.size() == 2; }));
    CHECK(stranger.failures.back().second == Error::NameTaken);  // usernames ignore case
    CHECK(stranger->getPage() == Page::Login);
}

TEST_CASE("E2E: students edit and delete their own live items; the educator sees it") {
    Lecture l;
    App& ana = *l.ana;
    App& prof = *l.prof;
    ana->askQuestion(QStringLiteral("whta is a stack frame"));
    REQUIRE(waitFor([&] { return prof.open().getQuestionCount() == 1; }));
    const int id = ana.open().getQuestions().front().getId();
    ana->editQuestion(id, QStringLiteral("What is a stack frame?"));
    REQUIRE(waitFor([&] { return prof.open().getQuestions().front().getText() == "What is a stack frame?"; }));
    CHECK(prof.open().getQuestions().front().isEdited());
    ana->deleteQuestion(id);
    REQUIRE(waitFor([&] { return prof.open().getQuestionCount() == 0; }));

    // Bob can't touch ana's question (it doesn't exist for him).
    ana->askQuestion(QStringLiteral("Another one"));
    REQUIRE(waitFor([&] { return ana.open().getQuestionCount() == 1; }));
    (*l.bob)->deleteQuestion(ana.open().getQuestions().front().getId());
    CHECK(l.bob->failure(Request::kDeleteQuestion) == Error::NoSuchInput);
}

TEST_CASE("E2E: the same educator on a laptop and a phone stays in sync") {
    Lecture l;
    App phone(l.server.port);
    phone->logIn(QStringLiteral("prof"), QStringLiteral("password123"));
    REQUIRE(waitFor([&] { return phone->getPage() == Page::Courses; }));
    REQUIRE(waitFor([&] { return phone->getCourses().size() == 1; }));
    phone->joinSession(l.sessionCode);  // the educator opens their own live session by code
    REQUIRE(waitFor([&] { return phone->getPage() == Page::EducatorLive; }));

    (*l.ana)->pressLost();
    REQUIRE(waitFor([&] { return phone.open().getPressCount() == 1; }));

    // Ending on the laptop moves the phone to review too.
    (*l.prof)->endSession();
    REQUIRE(waitFor([&] { return phone->getPage() == Page::SessionViewer; }));
    // Ending again from the phone says it's already ended.
    phone->endSession();
    CHECK(phone.failure(Request::kEndSession) == Error::SessionEnded);
}

TEST_CASE("E2E: deleting a session or course moves everyone who had it open") {
    Lecture l;
    App& prof = *l.prof;
    App& ana = *l.ana;
    const int sessionId = prof.open().getId();

    // A live session can't be deleted (end it first).
    prof->deleteSession(sessionId);
    CHECK(prof.failure(Request::kDeleteSession) == Error::SessionNotEnded);

    prof->endSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::SessionViewer; }));
    prof->publishSession();
    REQUIRE(waitFor([&] { return prof.open().isPublished(); }));
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Course; }));
    ana->openSession(sessionId);
    REQUIRE(waitFor([&] { return ana->getPage() == Page::SessionViewer; }));

    prof->deleteSession(sessionId);
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Course; }));
    CHECK(ana.got(Notice::SessionDeleted));
    REQUIRE(waitFor([&] { return ana->getVisibleSessions().empty(); }));

    prof->deleteCourse();
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Courses; }));
    CHECK(ana.got(Notice::CourseDeleted));
    REQUIRE(waitFor([&] { return ana->getCourses().empty(); }));
}

TEST_CASE("E2E: anonymous course still names the educator's participant list") {
    Lecture l(/*anonymous=*/true);
    REQUIRE(waitFor([&] { return (*l.prof)->getParticipants().size() == 2; }));
    for (const Participant& p : (*l.prof)->getParticipants()) {
        CHECK_FALSE(p.name.empty());
        CHECK(p.userId != 0);
    }
}

TEST_CASE("E2E: anonymous course hides who asked from classmates, not the educator") {
    Lecture l(/*anonymous=*/true);
    App& prof = *l.prof;
    App& bob = *l.bob;
    (*l.ana)->askQuestion(QStringLiteral("Why does the base case stop it?"));
    REQUIRE(waitFor([&] { return prof.open().getQuestionCount() == 1; }));
    CHECK(prof.open().getQuestions().front().getAuthorState() == AuthorState::Known);
    CHECK(prof.open().getQuestions().front().getAuthorName() == "ana");

    prof->endSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::SessionViewer; }));
    const int sessionId = prof.open().getId();
    prof->publishSession();
    REQUIRE(waitFor([&] { return prof.open().isPublished(); }));
    CHECK(prof.open().getQuestions().front().getAuthorName() == "ana");

    REQUIRE(waitFor([&] { return bob->getPage() == Page::Course; }));
    bob->openSession(sessionId);
    REQUIRE(waitFor([&] { return bob->getPage() == Page::SessionViewer; }));
    REQUIRE(bob.open().getQuestionCount() == 1);
    CHECK(bob.open().getQuestions().front().getAuthorState() == AuthorState::Hidden);
    CHECK(bob.open().getQuestions().front().getAuthorName().empty());
}

TEST_CASE("E2E: search boxes filter sessions and questions without asking the server") {
    Lecture l;
    App& prof = *l.prof;
    (*l.ana)->askQuestion(QStringLiteral("What is memoization?"));
    (*l.bob)->askQuestion(QStringLiteral("Is recursion slower than loops?"));
    REQUIRE(waitFor([&] { return prof.open().getQuestionCount() == 2; }));
    prof->endSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::SessionViewer; }));

    prof->setQuestionSearch(QStringLiteral("memo"), false);
    REQUIRE(prof->getVisibleQuestions().size() == 1);
    CHECK(prof->getVisibleQuestions().front().getText() == "What is memoization?");
    const int memo = prof->getVisibleQuestions().front().getId();
    prof->addAnswer(memo, QStringLiteral("Caching results of calls."));
    REQUIRE(waitFor([&] { return !prof.open().findQuestion(memo)->getAnswers().empty(); }));
    prof->setQuestionSearch(QString(), /*unansweredOnly=*/true);
    REQUIRE(prof->getVisibleQuestions().size() == 1);
    CHECK(prof->getVisibleQuestions().front().getText() == "Is recursion slower than loops?");

    // Session list search on the course page.
    prof->leaveSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::Course; }));
    REQUIRE(waitFor([&] { return prof->getVisibleSessions().size() == 1; }));
    prof->setSessionSearch(QStringLiteral("graphs"));
    CHECK(prof->getVisibleSessions().empty());
    prof->setSessionSearch(QStringLiteral("recursion"));
    CHECK(prof->getVisibleSessions().size() == 1);
}

TEST_CASE("E2E: Find recording through Panopto, with the one-time folder link") {
    Lecture l;
    App& prof = *l.prof;
    prof->endSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::SessionViewer; }));

    // Before the folder is linked.
    prof->findRecording();
    CHECK(prof.failure(Request::kCheckRecording) == Error::PanoptoFolderNotLinked);

    // Link it from a lecture link: signs in first.
    prof->linkPanoptoFolder(QStringLiteral("not a link"));
    CHECK(prof.failure(Request::kLinkPanoptoFolder) == Error::PanoptoLinkInvalid);
    CHECK(prof.panopto.signIns == 1);
    prof->linkPanoptoFolder(QStringLiteral("https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=f0bc"));
    REQUIRE(waitFor([&] { return prof->getOpenCourse()->course.hasPanoptoFolder(); }));
    CHECK(prof.got(Notice::PanoptoLinked));

    // Panopto still processing: nothing overlaps yet.
    prof->findRecording();
    CHECK(waitFor([&] { return prof.failures.back().second == Error::RecordingNotReady; }));

    // Now it's there: recording started 5 minutes before the session.
    RecordingCandidate lecture;
    lecture.viewerUrl = "https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=lec1";
    lecture.name = "CSCI 187";
    lecture.start = prof.open().getStartTime() - std::chrono::minutes(5);
    lecture.durationSeconds = 70 * 60;
    prof.panopto.recordings = {lecture};
    prof->findRecording();
    REQUIRE(waitFor([&] { return prof.open().getRecording() != nullptr; }));
    CHECK(prof.open().getRecording()->getOffsetSeconds() == 300);
    CHECK(prof.open().getRecording()->getSource() == RecordingSource::Panopto);
    CHECK(prof.panopto.signIns == 1);  // stayed signed in

    // A student never gets the course's folder id.
    CHECK_FALSE((*l.ana)->getOpenCourse()->course.hasPanoptoFolder());
}

TEST_CASE("E2E: the server going away mid-lecture, then coming back") {
    Lecture l;
    App& ana = *l.ana;
    const int sessionId = ana.open().getId();

    const int port = l.server.port;
    l.server.stop();
    REQUIRE(waitFor([&] { return !ana->isConnected(); }));
    // Requests while it's down fail with a clear error instead of hanging.
    ana->askQuestion(QStringLiteral("still there?"));
    CHECK(ana.failure(Request::kAddQuestion) == Error::ServerUnreachable);

    // Back up on the same port: the app logs back in by itself and reopens
    // the session it had open.
    l.server.start(port);
    REQUIRE(waitFor([&] { return ana->isConnected(); }, 8000));
    REQUIRE(waitFor([&] { return ana.got(Notice::Reconnected); }, 4000));
    REQUIRE(waitFor([&] { return ana->getOpenSession() && ana.open().getId() == sessionId; }));
    CHECK(ana->getPage() == Page::StudentLive);
    ana->pressLost();
    REQUIRE(waitFor([&] { return ana.open().getPressCount() == 1; }));
}

TEST_CASE("E2E: deleting your account logs out your other devices") {
    Server server;
    App laptop(server.port);
    laptop.signUp(QStringLiteral("dana"));
    App phone(server.port);
    phone->logIn(QStringLiteral("dana"), QStringLiteral("password123"));
    REQUIRE(waitFor([&] { return phone->getPage() == Page::Courses; }));

    laptop->deleteAccount(QStringLiteral("wrongpassword"));
    CHECK(laptop.failure(Request::kDeleteAccount) == Error::WrongPassword);
    laptop->deleteAccount(QStringLiteral("password123"));
    REQUIRE(waitFor([&] { return laptop->getPage() == Page::Login; }));
    REQUIRE(waitFor([&] { return phone->getPage() == Page::Login; }));
    CHECK(phone.got(Notice::AccountDeleted));
    CHECK(phone->getUser() == nullptr);
}

TEST_CASE("E2E: a session that ended while a student was offline doesn't strand them") {
    Lecture l;
    App& ana = *l.ana;
    const int sessionId = ana.open().getId();
    const int port = l.server.port;
    l.server.stop();
    REQUIRE(waitFor([&] { return !ana->isConnected(); }));
    // The educator ended it meanwhile (on the server's side).
    REQUIRE(l.server.db.setSessionEnd(sessionId, Clock::now()).value);
    l.server.start(port);
    REQUIRE(waitFor([&] { return ana.got(Notice::Reconnected); }, 8000));
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Course; }));
    CHECK_FALSE(ana->getOpenSession().has_value());
    CHECK(ana.got(Notice::SessionEndedByEducator));
}

TEST_CASE("E2E: a password changed while offline asks to log in again") {
    Server server;
    App phone(server.port);
    phone.signUp(QStringLiteral("dana"));
    settle();  // let the course list that follows sign-up arrive before the server stops
    const int port = server.port;
    server.stop();
    REQUIRE(waitFor([&] { return !phone->isConnected(); }));
    const int userId = phone->getUser()->getId();
    const std::string salt = PasswordHasher::generateSaltHex();
    REQUIRE(server.db.setPassword(userId, PasswordHasher::hashPassword("newpassword1", salt, 10), salt, 10).value);
    server.start(port);
    REQUIRE(waitFor([&] { return phone->getPage() == Page::Login; }, 8000));
    CHECK(phone.got(Notice::LoggedOutElsewhere));
    CHECK(phone.failures.empty());  // no "wrong username or password" for nothing typed
}

TEST_CASE("E2E: answering two questions quickly sends both answers") {
    Lecture l;
    App& prof = *l.prof;
    App& ana = *l.ana;
    ana->askQuestion(QStringLiteral("first?"));
    ana->askQuestion(QStringLiteral("second?"));
    REQUIRE(waitFor([&] { return prof.open().getQuestions().size() == 2; }));
    prof->endSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::SessionViewer; }));
    const auto questions = prof.open().getQuestions();
    prof->addAnswer(questions[0].getId(), QStringLiteral("one"));
    prof->addAnswer(questions[1].getId(), QStringLiteral("two"));
    REQUIRE(waitFor([&] {
        const auto now = prof.open().getQuestions();
        return now.size() == 2 && !now[0].getAnswers().empty() && !now[1].getAnswers().empty();
    }));
}

TEST_CASE("E2E: pressing End then Back keeps the educator on the course page") {
    Lecture l;
    App& prof = *l.prof;
    prof->endSession();
    prof->leaveSession();
    REQUIRE(waitFor([&] { return prof.got(Notice::SessionEnded); }));
    settle();
    CHECK(prof->getPage() == Page::Course);
    CHECK_FALSE(prof->getOpenSession().has_value());
}

TEST_CASE("E2E: an educator joining their own session by code isn't told to press Lost") {
    Lecture l;
    App& prof = *l.prof;
    prof->goHome();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::Courses; }));
    prof->joinSession(l.sessionCode);
    REQUIRE(waitFor([&] { return prof->getPage() == Page::EducatorLive; }));
    CHECK(prof.got(Notice::EducatorJoined));
    CHECK_FALSE(prof.got(Notice::SessionJoined));
}

namespace {
using Kind = AppNotification::Kind;
int countKind(const AppController& c, Kind kind) {
    const auto& all = c.getNotifications();
    return static_cast<int>(std::count_if(all.begin(), all.end(),
                                          [&](const AppNotification& n) { return n.kind == kind; }));
}
}  // namespace

TEST_CASE("E2E: the bell tells the educator who joined the course and the live session") {
    Lecture l;
    App& prof = *l.prof;
    // ana and bob each joined the course (by session code) and came into the room.
    REQUIRE(waitFor([&] { return prof->getNotifications().size() == 4; }));
    CHECK(countKind(prof.controller, Kind::StudentJoinedCourse) == 2);
    CHECK(countKind(prof.controller, Kind::StudentJoinedSession) == 2);
    CHECK(prof->getUnreadCount() == 4);
    // ana and bob joined at the same moment, so the order between them varies.
    std::multiset<std::string> names;
    for (const AppNotification& n : prof->getNotifications()) {
        names.insert(n.studentName);
        CHECK(n.courseName == "CSCI 187");
        if (n.kind == Kind::StudentJoinedSession) {
            CHECK(n.sessionName == "Lecture 1: Recursion");
        }
    }
    CHECK(names == std::multiset<std::string>{"ana", "ana", "bob", "bob"});
    // Students hear nothing about each other.
    CHECK((*l.ana)->getNotifications().empty());

    prof->markNotificationsRead();
    CHECK(prof->getUnreadCount() == 0);
    CHECK(prof->getNotifications().size() == 4);

    // Leaving and coming back: the people already there aren't news.
    prof->leaveSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::Course; }));
    prof->joinSession(l.sessionCode);
    REQUIRE(waitFor([&] { return prof->getPage() == Page::EducatorLive; }));
    REQUIRE(waitFor([&] { return prof->getParticipants().size() == 2; }));
    settle(300);
    CHECK(prof->getNotifications().size() == 4);

    // A student leaving and coming back is news again.
    (*l.ana)->leaveSession();
    REQUIRE(waitFor([&] { return prof->getParticipants().size() == 1; }));
    (*l.ana)->joinSession(l.sessionCode);
    REQUIRE(waitFor([&] { return prof->getNotifications().size() == 5; }));
    CHECK(prof->getNotifications().front().studentName == "ana");
    CHECK(prof->getNotifications().front().kind == Kind::StudentJoinedSession);

    // Clicking "ana joined CSCI 187" opens the course.
    prof->endSession();
    REQUIRE(waitFor([&] { return prof->getPage() == Page::SessionViewer; }));
    const auto& all = prof->getNotifications();
    const auto joined = std::find_if(all.begin(), all.end(),
                                     [](const AppNotification& n) { return n.kind == Kind::StudentJoinedCourse; });
    REQUIRE(joined != all.end());
    prof->openNotification(static_cast<int>(joined - all.begin()));
    CHECK(waitFor([&] { return prof->getPage() == Page::Course; }));

    prof->clearNotifications();
    CHECK(prof->getNotifications().empty());
}

TEST_CASE("E2E: the bell tells students when a session is published, and opens it") {
    Lecture l;
    App& prof = *l.prof;
    App& ana = *l.ana;
    prof->endSession();
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Course; }));
    settle(200);
    CHECK(ana->getNotifications().empty());  // ending isn't publishing

    REQUIRE(waitFor([&] { return prof->getPage() == Page::SessionViewer; }));
    prof->publishSession();
    REQUIRE(waitFor([&] { return ana->getNotifications().size() == 1; }));
    const AppNotification& n = ana->getNotifications().front();
    CHECK(n.kind == Kind::SessionPublished);
    CHECK(n.sessionName == "Lecture 1: Recursion");
    CHECK(n.courseName == "CSCI 187");
    CHECK(n.sessionId == prof.open().getId());
    CHECK(ana->getUnreadCount() == 1);
    CHECK(countKind(prof.controller, Kind::SessionPublished) == 0);

    ana->goHome();
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Courses; }));
    ana->openNotification(0);
    CHECK(waitFor([&] { return ana->getPage() == Page::SessionViewer; }));
    CHECK(ana->getUnreadCount() == 0);

    // A new login starts with an empty bell.
    ana->logOut();
    REQUIRE(waitFor([&] { return ana->getPage() == Page::Login; }));
    CHECK(ana->getNotifications().empty());
}

TEST_CASE("E2E: in an anonymous course the bell still names who joined (the educator sees names)") {
    Lecture l(/*anonymous=*/true);
    REQUIRE(waitFor([&] { return (*l.prof)->getNotifications().size() == 4; }));
    std::multiset<std::string> names;
    for (const AppNotification& n : (*l.prof)->getNotifications()) {
        names.insert(n.studentName);
    }
    CHECK(names == std::multiset<std::string>{"ana", "ana", "bob", "bob"});
}

TEST_CASE("E2E: students see whose course it is (course name - educator)") {
    Lecture l;
    // ana's open course (she joined by session code) and her My Courses list.
    REQUIRE((*l.ana)->getOpenCourse());
    CHECK((*l.ana)->getOpenCourse()->course.getEducatorName() == "prof");
    CHECK(ViewText::formatCourseTitle((*l.ana)->getOpenCourse()->course) == "CSCI 187 - prof");
    (*l.ana)->goHome();
    REQUIRE(waitFor([&] { return (*l.ana)->getCourses().size() == 1; }));
    CHECK((*l.ana)->getCourses().front().getEducatorName() == "prof");
    // When it was created: just now, as far as both of them can tell.
    const auto age = std::chrono::system_clock::now() - (*l.ana)->getOpenCourse()->course.getCreatedTime();
    CHECK(age >= std::chrono::seconds(-2));
    CHECK(age < std::chrono::minutes(5));
    CHECK((*l.prof)->getOpenCourse()->course.getCreatedTime() ==
          (*l.ana)->getOpenCourse()->course.getCreatedTime());
    CHECK(ViewText::formatCourseCreated((*l.ana)->getOpenCourse()->course)
              .startsWith("Course created on "));
    // The educator's own page shows just the course name.
    CHECK(ViewText::formatCourseTitle((*l.prof)->getOpenCourse()->course) == "CSCI 187");
}
