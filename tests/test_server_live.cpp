// Server tests with several apps connected at once: the requests that go
// through AppModel and the live pushes (CONTRACT.md 6, "Broadcast to
// others"; section 8: "two fake clients see the right broadcasts").
// Each test case has its own server and its own in-memory database.

#include "doctest.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QUrl>
#include <QWebSocket>

#include <memory>
#include <optional>
#include <string>

#include "database.h"
#include "error.h"
#include "passwordhasher.h"
#include "perplexserver.h"
#include "protocol.h"

namespace {

using namespace Protocol;

// One connected app. Replies carry "ok"; pushes from other people's
// actions do not, so the two are kept apart.
class FakeApp {
public:
    explicit FakeApp(int port) {
        QObject::connect(&socket, &QWebSocket::textMessageReceived,
                         [this](const QString& text) {
                             const QJsonObject message = decode(text);
                             if (message.contains(Field::kOk)) {
                                 replies.append(message);
                             } else {
                                 pushes.append(message);
                             }
                         });
        socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
        REQUIRE(waitFor([this] { return socket.state() == QAbstractSocket::ConnectedState; }));
        REQUIRE(waitFor([this] { return takePush(Reply::kHello).has_value(); }));
    }

    bool isConnected() const { return socket.state() == QAbstractSocket::ConnectedState; }

    // Sends a request and waits for its reply.
    QJsonObject send(const QString& type, QJsonObject fields = {}) {
        socket.sendTextMessage(encode(makeMessage(type, fields)));
        waitFor([this] { return !replies.isEmpty(); });
        return replies.isEmpty() ? QJsonObject{} : replies.takeFirst();
    }

    // Waits up to a second for a push of this type.
    std::optional<QJsonObject> waitPush(const QString& type) {
        std::optional<QJsonObject> found;
        waitFor([&] { return (found = takePush(type)).has_value(); });
        return found;
    }

    // True if no push of this type is on its way. The server handles one
    // message at a time, in order (N2), so once a reply to a request sent
    // NOW comes back, every push from earlier actions has already arrived:
    // no guessing with a timer.
    bool getsNo(const QString& type) {
        REQUIRE(!sendRaw(QStringLiteral(R"({"type":"flush"})")).isEmpty());
        return !takePush(type).has_value();
    }

    void login(const QString& name) {
        QJsonObject fields;
        fields[Field::kName] = name;
        fields[Field::kPassword] = QStringLiteral("password123");
        fields[Field::kEmail] = name + QStringLiteral("@scu.edu");
        REQUIRE(send(Request::kRegister, fields)[Field::kOk].toBool());
    }

    void close() { socket.close(); }

    // Sends raw text and waits for whatever comes back.
    QJsonObject sendRaw(const QString& text) {
        socket.sendTextMessage(text);
        waitFor([this] { return !replies.isEmpty(); });
        return replies.isEmpty() ? QJsonObject{} : replies.takeFirst();
    }

private:
    std::optional<QJsonObject> takePush(const QString& type) {
        for (int i = 0; i < pushes.size(); ++i) {
            if (pushes[i][Field::kType].toString() == type) {
                return pushes.takeAt(i);
            }
        }
        return std::nullopt;
    }

    // True once `done` holds; false if it didn't within `ms`.
    template <typename Condition>
    static bool waitFor(Condition done, int ms = 3000) {
        QElapsedTimer timer;
        timer.start();
        bool ok = false;
        while (!(ok = done()) && timer.elapsed() < ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        return ok;
    }

    QWebSocket socket;
    QList<QJsonObject> replies;
    QList<QJsonObject> pushes;
};

// The error in a failed reply ({"ok":false,"error":"<code>"}).
Error errorOf(const QJsonObject& reply) {
    return getErrorFromCode(reply[Reply::kError].toString().toStdString());
}

QJsonObject fields(std::initializer_list<std::pair<QString, QJsonValue>> list) {
    QJsonObject object;
    for (const auto& [key, value] : list) {
        object[key] = value;
    }
    return object;
}

// An educator with a live session and two students in it.
struct Room {
    Database db{":memory:"};
    std::unique_ptr<PerplexServer> server;
    std::unique_ptr<FakeApp> prof, ana, bob;
    int courseId = 0;
    int sessionId = 0;

    explicit Room(bool anonymous = false) {
        REQUIRE(db.open().isOk());
        server = std::make_unique<PerplexServer>(0, &db, nullptr, QHostAddress::LocalHost,
                                                 PasswordHasher::kTestIterations);
        REQUIRE(server->listen());
        prof = std::make_unique<FakeApp>(server->port());
        ana = std::make_unique<FakeApp>(server->port());
        bob = std::make_unique<FakeApp>(server->port());
        REQUIRE(prof->isConnected());
        prof->login(QStringLiteral("prof"));
        ana->login(QStringLiteral("ana"));
        bob->login(QStringLiteral("bob"));

        const QJsonObject course = prof->send(Request::kCreateCourse,
            fields({{Field::kName, QStringLiteral("CSCI 187")},
                    {Field::kAnonymous, anonymous},
                    {Field::kQuestionsShared, true}}))[Field::kData].toObject();
        courseId = course[Field::kId].toInt();
        REQUIRE(courseId > 0);

        const QJsonObject session = prof->send(Request::kStartSession,
            fields({{Field::kCourseId, courseId},
                    {Field::kName, QStringLiteral("Lecture 1")}}))[Field::kData].toObject();
        sessionId = session[Field::kSessionId].toInt();
        const QString code = session[Field::kJoinCode].toString();
        REQUIRE(sessionId > 0);

        REQUIRE(ana->send(Request::kJoinSession, fields({{Field::kJoinCode, code}}))[Field::kOk].toBool());
        REQUIRE(bob->send(Request::kJoinSession, fields({{Field::kJoinCode, code}}))[Field::kOk].toBool());
    }
};

}  // namespace

TEST_CASE("Live push: a Lost press reaches the educator, not the other students") {
    Room room;

    const QJsonObject reply = room.ana->send(Request::kAddLost, fields({{Field::kSessionId, room.sessionId}}));
    REQUIRE(reply[Field::kOk].toBool());
    const int pingId = reply[Field::kData].toObject()[Field::kPingId].toInt();
    CHECK(pingId > 0);

    const auto pushed = room.prof->waitPush(Reply::kLostAdded);
    REQUIRE(pushed.has_value());
    const QJsonObject ping = (*pushed)[Field::kLostPing].toObject();
    CHECK((*pushed)[Field::kSessionId].toInt() == room.sessionId);
    CHECK(ping[Field::kPingId].toInt() == pingId);
    // Pings never name the student (V2).
    CHECK(ping[Field::kAuthorState].toString() == QStringLiteral("hidden"));
    CHECK(ping[Field::kAuthorId].toInt() == 0);

    // While live, a student sees only their own items.
    CHECK(room.bob->getsNo(Reply::kLostAdded));
    // The sender gets the reply, not a push as well.
    CHECK(room.ana->getsNo(Reply::kLostAdded));

    // Taking it back reaches the educator too.
    REQUIRE(room.ana->send(Request::kDeleteLost, fields({{Field::kPingId, pingId}}))[Field::kOk].toBool());
    const auto deleted = room.prof->waitPush(Reply::kLostDeleted);
    REQUIRE(deleted.has_value());
    CHECK((*deleted)[Field::kPingId].toInt() == pingId);
    CHECK(room.bob->getsNo(Reply::kLostDeleted));
}

TEST_CASE("Live push: questions reach the educator, edits and deletes follow") {
    Room room;

    const QJsonObject reply = room.ana->send(Request::kAddQuestion,
        fields({{Field::kSessionId, room.sessionId}, {Field::kText, QStringLiteral("Why O(n log n)?")}}));
    REQUIRE(reply[Field::kOk].toBool());
    const int questionId = reply[Field::kData].toObject()[Field::kQuestionId].toInt();

    const auto added = room.prof->waitPush(Reply::kQuestionAdded);
    REQUIRE(added.has_value());
    CHECK((*added)[Field::kQuestion].toObject()[Field::kText].toString() == QStringLiteral("Why O(n log n)?"));
    CHECK(room.bob->getsNo(Reply::kQuestionAdded));

    REQUIRE(room.ana->send(Request::kEditQuestion,
        fields({{Field::kQuestionId, questionId}, {Field::kText, QStringLiteral("Why n log n?")}}))[Field::kOk].toBool());
    const auto updated = room.prof->waitPush(Reply::kQuestionUpdated);
    REQUIRE(updated.has_value());
    CHECK((*updated)[Field::kQuestion].toObject()[Field::kText].toString() == QStringLiteral("Why n log n?"));

    REQUIRE(room.ana->send(Request::kDeleteQuestion, fields({{Field::kQuestionId, questionId}}))[Field::kOk].toBool());
    const auto deleted = room.prof->waitPush(Reply::kQuestionDeleted);
    REQUIRE(deleted.has_value());
    CHECK((*deleted)[Field::kQuestionId].toInt() == questionId);
    CHECK_FALSE((*deleted)[Field::kByModerator].toBool());
}

TEST_CASE("Live push: an anonymous course still shows the educator who asked") {
    Room room(/*anonymous=*/true);

    REQUIRE(room.ana->send(Request::kAddQuestion,
        fields({{Field::kSessionId, room.sessionId}, {Field::kText, QStringLiteral("Lost on slide 4")}}))[Field::kOk].toBool());
    const auto added = room.prof->waitPush(Reply::kQuestionAdded);
    REQUIRE(added.has_value());
    const QJsonObject question = (*added)[Field::kQuestion].toObject();
    CHECK(question[Field::kAuthorState].toString() != QStringLiteral("hidden"));
    CHECK(question[Field::kAuthorName].toString() == QStringLiteral("ana"));
}

TEST_CASE("Live push: the educator sees who is in the room") {
    Room room;

    // The latest list has both students (earlier ones may still be queued).
    QJsonArray participants;
    while (const auto pushed = room.prof->waitPush(Reply::kParticipants)) {
        participants = (*pushed)[Field::kParticipants].toArray();
        if (participants.size() == 2) {
            break;
        }
    }
    REQUIRE(participants.size() == 2);

    // Bob leaves: the list shrinks to Ana.
    REQUIRE(room.bob->send(Request::kCloseSession)[Field::kOk].toBool());
    const auto after = room.prof->waitPush(Reply::kParticipants);
    REQUIRE(after.has_value());
    const QJsonArray left = (*after)[Field::kParticipants].toArray();
    REQUIRE(left.size() == 1);
    CHECK(left[0].toObject()[Field::kName].toString() == QStringLiteral("ana"));

    // Students never get the list.
    CHECK(room.ana->getsNo(Reply::kParticipants));
}

TEST_CASE("Live push: ending the session locks it for the room and tells the course") {
    Room room;

    REQUIRE(room.prof->send(Request::kEndSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());

    const auto ended = room.ana->waitPush(Reply::kSessionEnded);
    REQUIRE(ended.has_value());
    const QJsonObject session = (*ended)[Field::kSession].toObject();
    CHECK(session[Field::kStatus].toString() == QStringLiteral("inReview"));
    CHECK(session[Field::kJoinCode].toString().isEmpty());  // S4
    CHECK(room.ana->waitPush(Reply::kCourseChanged).has_value());

    // Locked: a late Lost press is refused.
    const QJsonObject late = room.bob->send(Request::kAddLost, fields({{Field::kSessionId, room.sessionId}}));
    CHECK_FALSE(late[Field::kOk].toBool());
}

TEST_CASE("Server routes: publish, rename and answers go through AppModel") {
    Room room;
    const QJsonObject asked = room.ana->send(Request::kAddQuestion,
        fields({{Field::kSessionId, room.sessionId}, {Field::kText, QStringLiteral("What is a CRC card?")}}));
    const int questionId = asked[Field::kData].toObject()[Field::kQuestionId].toInt();

    // Educator-only calls are refused for a student (checked in the model).
    const QJsonObject studentRename = room.ana->send(Request::kRenameSession,
        fields({{Field::kSessionId, room.sessionId}, {Field::kName, QStringLiteral("Mine")}}));
    CHECK_FALSE(studentRename[Field::kOk].toBool());
    CHECK(errorOf(studentRename) == Error::NotCourseEducator);

    // Publishing a live session is refused; end it first.
    CHECK_FALSE(room.prof->send(Request::kPublishSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());
    REQUIRE(room.prof->send(Request::kEndSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());

    // Rename reaches the course.
    REQUIRE(room.prof->send(Request::kRenameSession,
        fields({{Field::kSessionId, room.sessionId}, {Field::kName, QStringLiteral("Lecture 1: CRC")}}))[Field::kOk].toBool());
    const auto renamed = room.ana->waitPush(Reply::kSessionRenamed);
    REQUIRE(renamed.has_value());
    CHECK((*renamed)[Field::kName].toString() == QStringLiteral("Lecture 1: CRC"));

    // Educator answers in review, edits, then deletes.
    const QJsonObject answered = room.prof->send(Request::kAddAnswer,
        fields({{Field::kQuestionId, questionId}, {Field::kText, QStringLiteral("Class, responsibilities, collaborators.")}}));
    REQUIRE(answered[Field::kOk].toBool());
    const QJsonArray answers = answered[Field::kData].toObject()[Field::kAnswers].toArray();
    REQUIRE(answers.size() == 1);
    const int answerId = answers[0].toObject()[Field::kAnswerId].toInt();
    // In review, students cannot open the session, so they get no push.
    CHECK(room.ana->getsNo(Reply::kAnswerAdded));

    REQUIRE(room.prof->send(Request::kPublishSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());

    const QJsonObject edited = room.prof->send(Request::kEditAnswer,
        fields({{Field::kAnswerId, answerId}, {Field::kText, QStringLiteral("Class, Responsibilities, Collaborators.")}}));
    REQUIRE(edited[Field::kOk].toBool());

    // Only the author may edit (A8).
    CHECK_FALSE(room.ana->send(Request::kEditAnswer,
        fields({{Field::kAnswerId, answerId}, {Field::kText, QStringLiteral("hacked")}}))[Field::kOk].toBool());

    REQUIRE(room.prof->send(Request::kDeleteAnswer, fields({{Field::kAnswerId, answerId}}))[Field::kOk].toBool());
}

TEST_CASE("Server routes: recording attach and remove") {
    Room room;
    REQUIRE(room.prof->send(Request::kEndSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());

    const QJsonObject attached = room.prof->send(Request::kAttachRecording,
        fields({{Field::kSessionId, room.sessionId},
                {Field::kUrl, QStringLiteral("https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=abc")},
                {Field::kOffsetSeconds, 90}}));
    REQUIRE(attached[Field::kOk].toBool());
    CHECK(attached[Field::kData].toObject()[Field::kHasRecording].toBool());
    CHECK(attached[Field::kData].toObject()[Field::kRecording].toObject()[Field::kOffsetSeconds].toInt() == 90);

    const QJsonObject removed = room.prof->send(Request::kRemoveRecording, fields({{Field::kSessionId, room.sessionId}}));
    REQUIRE(removed[Field::kOk].toBool());
    CHECK_FALSE(removed[Field::kData].toObject()[Field::kHasRecording].toBool());

    // A missing field is a bad request, not a crash.
    const QJsonObject bad = room.prof->send(Request::kAttachRecording, fields({{Field::kSessionId, room.sessionId}}));
    CHECK(errorOf(bad) == Error::BadRequest);
}

TEST_CASE("Server routes: course settings reach the students") {
    Room room;
    REQUIRE(room.prof->send(Request::kUpdateCourse,
        fields({{Field::kCourseId, room.courseId}, {Field::kName, QStringLiteral("CSCI 187 Fall")},
                {Field::kAnonymous, true}, {Field::kQuestionsShared, false}}))[Field::kOk].toBool());
    const auto changed = room.bob->waitPush(Reply::kCourseChanged);
    REQUIRE(changed.has_value());
    CHECK((*changed)[Field::kCourseId].toInt() == room.courseId);
}

TEST_CASE("Server routes: deleting an account logs out its other connections") {
    Database db(":memory:");
    REQUIRE(db.open().isOk());
    PerplexServer server(0, &db, nullptr, QHostAddress::LocalHost, PasswordHasher::kTestIterations);
    REQUIRE(server.listen());

    FakeApp laptop(server.port());
    FakeApp phone(server.port());
    laptop.login(QStringLiteral("cara"));
    REQUIRE(phone.send(Request::kLogin, fields({{Field::kName, QStringLiteral("cara")},
                                                {Field::kPassword, QStringLiteral("password123")}}))[Field::kOk].toBool());

    // Wrong password: nothing happens.
    CHECK_FALSE(laptop.send(Request::kDeleteAccount, fields({{Field::kPassword, QStringLiteral("nope12345")}}))[Field::kOk].toBool());

    REQUIRE(laptop.send(Request::kChangePassword,
        fields({{Field::kOldPassword, QStringLiteral("password123")},
                {Field::kNewPassword, QStringLiteral("newpassword1")}}))[Field::kOk].toBool());
    REQUIRE(laptop.send(Request::kDeleteAccount, fields({{Field::kPassword, QStringLiteral("newpassword1")}}))[Field::kOk].toBool());

    const auto loggedOut = phone.waitPush(Reply::kLoggedOut);
    REQUIRE(loggedOut.has_value());
    CHECK((*loggedOut)[Field::kReason].toString() == kReasonAccountDeleted);
    // The phone is really logged out now.
    const QJsonObject after = phone.send(Request::kListCourses);
    CHECK(errorOf(after) == Error::NotLoggedIn);
}

TEST_CASE("Server routes: the course page gets its session list") {
    Room room;
    const QJsonObject reply = room.ana->send(Request::kGetCourse, fields({{Field::kCourseId, room.courseId}}));
    REQUIRE(reply[Field::kOk].toBool());
    const QJsonObject data = reply[Field::kData].toObject();
    CHECK(data[Field::kRole].toString() == QStringLiteral("student"));
    const QJsonArray sessions = data[Field::kSessions].toArray();
    REQUIRE(sessions.size() == 1);
    CHECK(sessions[0].toObject()[Field::kSessionId].toInt() == room.sessionId);
    CHECK(sessions[0].toObject()[Field::kStatus].toString() == QStringLiteral("live"));
}

TEST_CASE("Live push: switching user on one app stops the old user's pushes") {
    Room room;
    // Ana's app logs in as a new user without logging out.
    room.ana->login(QStringLiteral("dan"));
    REQUIRE(room.prof->send(Request::kEndSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());
    CHECK(room.ana->getsNo(Reply::kSessionEnded));
    CHECK(room.bob->waitPush(Reply::kSessionEnded).has_value());
}

TEST_CASE("Server routes: malformed requests get BadRequest, never a crash or silence") {
    Room room;
    CHECK(errorOf(room.ana->sendRaw(QStringLiteral("not json"))) == Error::BadRequest);
    // A session id sent as text is a bad request, not "no such session".
    const QJsonObject textId = room.ana->send(Request::kAddLost, fields({{Field::kSessionId, QStringLiteral("7")}}));
    CHECK(errorOf(textId) == Error::BadRequest);
    // A toggle that isn't a bool is refused, not read as false.
    const QJsonObject badToggle = room.prof->send(Request::kCreateCourse,
        fields({{Field::kName, QStringLiteral("X")}, {Field::kAnonymous, QStringLiteral("yes")}}));
    CHECK(errorOf(badToggle) == Error::BadRequest);
    // Missing text on a question.
    CHECK(errorOf(room.ana->send(Request::kAddQuestion, fields({{Field::kSessionId, room.sessionId}}))) == Error::BadRequest);
    // Sign-up without an email is refused; no name@scu.edu is made up (U6).
    FakeApp carl(room.server->port());
    CHECK(errorOf(carl.send(Request::kRegister, fields({{Field::kName, QStringLiteral("carl")},
                                                       {Field::kPassword, QStringLiteral("password123")}}))) == Error::BadRequest);
    CHECK(errorOf(carl.send(Request::kRegister, fields({{Field::kName, QStringLiteral("carl")},
                                                       {Field::kPassword, QStringLiteral("password123")},
                                                       {Field::kEmail, QStringLiteral("")}}))) == Error::EmptyEmail);
    // The connection still works afterwards.
    CHECK(room.ana->send(Request::kAddLost, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());
}

TEST_CASE("Server routes: big or odd messages get a reply, and the connection stays") {
    Room room;
    // Over 64 KB: BadRequest, not a silently closed connection.
    const QString big = QStringLiteral(R"({"type":"addQuestion","text":"%1"})").arg(QString(70 * 1024, 'x'));
    CHECK(errorOf(room.ana->sendRaw(big)) == Error::BadRequest);
    CHECK(room.ana->isConnected());
    // Ids must be whole numbers that fit in an int.
    CHECK(errorOf(room.prof->send(Request::kPublishSession, fields({{Field::kSessionId, 1.5}}))) == Error::BadRequest);
    CHECK(errorOf(room.ana->send(Request::kAddLost, fields({{Field::kSessionId, 4294967297.0}}))) == Error::BadRequest);
    // An unknown type is answered with that same type.
    const QJsonObject unknown = room.ana->sendRaw(QStringLiteral(R"({"type":"fly"})"));
    CHECK(unknown[Field::kType].toString() == QStringLiteral("fly"));
    CHECK(errorOf(unknown) == Error::UnknownRequest);
}

TEST_CASE("Server routes: a course with a live session can't be deleted") {
    Room room;
    CHECK(errorOf(room.prof->send(Request::kDeleteCourse, fields({{Field::kCourseId, room.courseId}}))) ==
          Error::CourseHasLiveSession);
    REQUIRE(room.prof->send(Request::kEndSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());
    const QJsonObject deleted = room.prof->send(Request::kDeleteCourse, fields({{Field::kCourseId, room.courseId}}));
    REQUIRE(deleted[Field::kOk].toBool());
    CHECK(deleted[Field::kData].toObject()[Field::kCourseId].toInt() == room.courseId);
}

TEST_CASE("Live push: students leave the room when the session ends") {
    Room room;
    const QJsonObject asked = room.ana->send(Request::kAddQuestion,
        fields({{Field::kSessionId, room.sessionId}, {Field::kText, QStringLiteral("why?")}}));
    const int questionId = asked[Field::kData].toObject()[Field::kQuestionId].toInt();
    REQUIRE(questionId > 0);
    REQUIRE(room.prof->send(Request::kEndSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());
    REQUIRE(room.prof->send(Request::kPublishSession, fields({{Field::kSessionId, room.sessionId}}))[Field::kOk].toBool());
    REQUIRE(room.prof->send(Request::kAddAnswer,
        fields({{Field::kQuestionId, questionId}, {Field::kText, QStringLiteral("see slide 4")}}))[Field::kOk].toBool());
    // Bob went back to the course page at the end: no answer pushes for him.
    CHECK(room.bob->getsNo(Reply::kAnswerAdded));
}

TEST_CASE("Live push: an anonymous course still names the room list for the educator") {
    Room room(/*anonymous=*/true);
    QJsonArray people;
    while (const auto pushed = room.prof->waitPush(Reply::kParticipants)) {
        people = (*pushed)[Field::kParticipants].toArray();
        if (people.size() == 2) {
            break;
        }
    }
    REQUIRE(people.size() == 2);
    for (const QJsonValue& person : people) {
        CHECK_FALSE(person.toObject()[Field::kName].toString().isEmpty());
    }
}
