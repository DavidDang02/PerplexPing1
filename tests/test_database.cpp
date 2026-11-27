#include "doctest.h"

#include "database.h"
#include "error.h"

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

using namespace std::chrono;

TEST_SUITE("Database Users [F1]") {
TEST_CASE("Create user succeeds") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("alice", "alice@scu.edu",
                                         "hash123", "salt456", 10000,
                                         system_clock::now());
    CHECK(created.isOk());
    CHECK(created.value.getId() > 0);
    CHECK(created.value.getName() == "alice");
    CHECK(created.value.getEmail() == "alice@scu.edu");
}

TEST_CASE("Create duplicate username returns NameTaken") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> first = db.createUser("bob", "bob1@scu.edu", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("bob", "bob2@scu.edu", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(!second.isOk());
    CHECK(second.error == Error::NameTaken);
}

TEST_CASE("Create duplicate email returns EmailTaken") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> first = db.createUser("carol", "carol@scu.edu", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("carol2", "carol@scu.edu", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(!second.isOk());
    CHECK(second.error == Error::EmailTaken);
}

TEST_CASE("Create user with empty email stores NULL") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("dave", "", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());
    CHECK(created.value.getEmail() == "");
}

TEST_CASE("Get user by id") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("eve", "eve@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> fetched = db.getUser(created.value.getId());
    CHECK(fetched.isOk());
    CHECK(fetched.value.getName() == "eve");
    CHECK(fetched.value.getEmail() == "eve@scu.edu");
}

TEST_CASE("Get non-existent user returns error") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> fetched = db.getUser(99999);
    CHECK(!fetched.isOk());
}

TEST_CASE("Find user by name (case-insensitive)") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("Frank", "frank@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> found = db.findUserByName("FRANK");
    CHECK(found.isOk());
    CHECK(found.value.getName() == "Frank");
}

TEST_CASE("Find user by email (case-insensitive)") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("Grace", "Grace@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<User> found = db.findUserByEmail("GRACE@SCU.EDU");
    CHECK(found.isOk());
    CHECK(found.value.getName() == "Grace");
}

TEST_CASE("Get credentials for user") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("heidi", "heidi@scu.edu", "hash99",
                                         "salt99", 15000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<Credentials> creds = db.getCredentials(created.value.getId());
    CHECK(creds.isOk());
    CHECK(creds.value.userId == created.value.getId());
    CHECK(creds.value.hashHex == "hash99");
    CHECK(creds.value.saltHex == "salt99");
    CHECK(creds.value.iterations == 15000);
}

TEST_CASE("Get credentials for non-existent user returns error") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<Credentials> fetched = db.getCredentials(99999);
    CHECK(!fetched.isOk());
}

TEST_CASE("Set password succeeds") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("ivan", "ivan@scu.edu", "oldhash",
                                         "oldsalt", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<bool> ok = db.setPassword(created.value.getId(), "newhash", "newsalts", 20000);
    CHECK(ok.isOk());
}

TEST_CASE("Set email succeeds") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> created = db.createUser("judy", "judy@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    Result<bool> ok = db.setEmail(created.value.getId(), "judy.new@scu.edu");
    CHECK(ok.isOk());

    Result<User> fetched = db.getUser(created.value.getId());
    CHECK(fetched.isOk());
    CHECK(fetched.value.getEmail() == "judy.new@scu.edu");
}

TEST_CASE("Set email fails if another user has that email") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    Result<User> first = db.createUser("kelly", "kelly@scu.edu", "h1", "s1", 10000,
                                       system_clock::now());
    CHECK(first.isOk());

    Result<User> second = db.createUser("larry", "larry@scu.edu", "h2", "s2", 10000,
                                        system_clock::now());
    CHECK(second.isOk());

    Result<bool> ok = db.setEmail(first.value.getId(), "larry@scu.edu");
    CHECK(!ok.isOk());
}

TEST_CASE("Count live sessions hosted by new user") {
    Database db(":memory:");
    CHECK(db.open().isOk());

    // No users or sessions yet
    int count = db.countLiveSessionsHostedBy(0);
    CHECK(count == 0);

    Result<User> created = db.createUser("mallory", "mallory@scu.edu", "h1", "s1", 10000,
                                         system_clock::now());
    CHECK(created.isOk());

    // No sessions hosted yet
    count = db.countLiveSessionsHostedBy(created.value.getId());
    CHECK(count == 0);
}
}
TEST_SUITE("Database items [F2-F4]") {
namespace {
// A course with an educator (prof) and two students, and one session.
struct Lecture {
    Database db{":memory:"};
    int prof = 0, ana = 0, bob = 0, courseId = 0, sessionId = 0;
    system_clock::time_point t0 = system_clock::time_point(seconds(1700000000));

    Lecture() {
        REQUIRE(db.open().isOk());
        prof = db.createUser("prof", "prof@scu.edu", "h", "s", 1, t0).value.getId();
        ana = db.createUser("ana", "ana@scu.edu", "h", "s", 1, t0).value.getId();
        bob = db.createUser("bob", "bob@scu.edu", "h", "s", 1, t0).value.getId();
        courseId = db.createCourse("CSCI 187", "K7M2QX", false, true, prof, t0).value.getId();
        REQUIRE(db.addStudent(courseId, ana, t0).isOk());
        REQUIRE(db.addStudent(courseId, bob, t0).isOk());
        sessionId = db.createSession(courseId, prof, "Lecture 1", "123456", t0).value.getId();
        REQUIRE(sessionId > 0);
    }
};
}  // namespace

TEST_CASE("Loaded pings and questions carry the author's name") {
    Lecture l;
    REQUIRE(l.db.addLostPing(l.sessionId, l.ana, l.t0 + seconds(10)).isOk());
    const auto added = l.db.addQuestion(l.sessionId, l.bob, l.t0 + seconds(20), "Why?");
    REQUIRE(added.isOk());
    CHECK(added.value.getAuthorName() == "bob");  // also on the object add* returns

    const auto session = l.db.loadSession(l.sessionId);
    REQUIRE(session.isOk());
    REQUIRE(session.value.getLostPings().size() == 1);
    CHECK(session.value.getLostPings()[0].getAuthorName() == "ana");
    REQUIRE(session.value.getQuestions().size() == 1);
    CHECK(session.value.getQuestions()[0].getAuthorName() == "bob");
}

TEST_CASE("Educator answers load first and are marked byEducator (A5)") {
    Lecture l;
    const int q = l.db.addQuestion(l.sessionId, l.ana, l.t0 + seconds(20), "Why?").value.getId();
    REQUIRE(l.db.setSessionEnd(l.sessionId, l.t0 + seconds(3600)).isOk());
    const auto student = l.db.addAnswer(q, l.bob, "student guess", l.t0 + seconds(3700));
    const auto educator = l.db.addAnswer(q, l.prof, "real answer", l.t0 + seconds(3800));
    REQUIRE(student.isOk());
    REQUIRE(educator.isOk());
    CHECK_FALSE(student.value.isByEducator());
    CHECK(educator.value.isByEducator());

    const auto question = l.db.getQuestion(q);
    REQUIRE(question.isOk());
    REQUIRE(question.value.getAnswers().size() == 2);
    CHECK(question.value.getAnswers()[0].getText() == "real answer");
    CHECK(question.value.getAnswers()[0].getAuthorName() == "prof");
    CHECK(question.value.getAnswers()[1].getAuthorName() == "bob");
}

TEST_CASE("An edited answer loads its edit time") {
    Lecture l;
    const int q = l.db.addQuestion(l.sessionId, l.ana, l.t0, "Why?").value.getId();
    const int a = l.db.addAnswer(q, l.prof, "first", l.t0 + seconds(5)).value.getId();
    REQUIRE(l.db.updateAnswerText(a, "second", l.t0 + seconds(9)).isOk());
    const auto answer = l.db.getAnswer(a);
    REQUIRE(answer.isOk());
    CHECK(answer.value.getText() == "second");
    REQUIRE(answer.value.getEditedTime().has_value());
    CHECK(*answer.value.getEditedTime() == l.t0 + seconds(9));
}

TEST_CASE("Deleting a user keeps their items as 'Deleted user' (U8)") {
    Lecture l;
    REQUIRE(l.db.addLostPing(l.sessionId, l.ana, l.t0).isOk());
    const int q = l.db.addQuestion(l.sessionId, l.ana, l.t0, "Why?").value.getId();
    REQUIRE(l.db.deleteUser(l.ana).isOk());

    const auto session = l.db.loadSession(l.sessionId);
    REQUIRE(session.isOk());
    REQUIRE(session.value.getLostPings().size() == 1);
    CHECK(session.value.getLostPings()[0].getAuthorState() == AuthorState::Deleted);
    CHECK(l.db.getQuestion(q).value.getAuthorState() == AuthorState::Deleted);
}

TEST_CASE("Ending or publishing twice is false, not an error") {
    Lecture l;
    CHECK(l.db.setSessionEnd(l.sessionId, l.t0 + seconds(60)).value);
    const auto again = l.db.setSessionEnd(l.sessionId, l.t0 + seconds(61));
    REQUIRE(again.isOk());
    CHECK_FALSE(again.value);
    CHECK(l.db.setSessionPublished(l.sessionId, l.t0 + seconds(70)).value);
    const auto republish = l.db.setSessionPublished(l.sessionId, l.t0 + seconds(71));
    REQUIRE(republish.isOk());
    CHECK_FALSE(republish.value);
}

TEST_CASE("Deleting a session removes its items (cascade)") {
    Lecture l;
    const int ping = l.db.addLostPing(l.sessionId, l.ana, l.t0).value.getId();
    const int q = l.db.addQuestion(l.sessionId, l.ana, l.t0, "Why?").value.getId();
    REQUIRE(l.db.deleteSession(l.sessionId).isOk());
    CHECK_FALSE(l.db.getLostPing(ping).isOk());
    CHECK_FALSE(l.db.getQuestion(q).isOk());
}
}

TEST_SUITE("Database files") {
TEST_CASE("An old M2 database file is refused, not stamped v2 and broken") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("perplexping.db");
    {
        // The M2 app's tables (plain-text user_password, no courses).
        QSqlDatabase m2 = QSqlDatabase::addDatabase("QSQLITE", "m2-file");
        m2.setDatabaseName(path);
        REQUIRE(m2.open());
        QSqlQuery q(m2);
        REQUIRE(q.exec("CREATE TABLE Users (user_id INTEGER PRIMARY KEY, user_name TEXT, user_password TEXT);"));
        REQUIRE(q.exec("INSERT INTO Users (user_name, user_password) VALUES ('old', 'pw');"));
        m2.close();
    }
    QSqlDatabase::removeDatabase("m2-file");

    {
        Database db(path);
        CHECK_FALSE(db.open().isOk());
    }
    // The file is untouched: still version 0, old row still there.
    {
        QSqlDatabase check = QSqlDatabase::addDatabase("QSQLITE", "m2-check");
        check.setDatabaseName(path);
        REQUIRE(check.open());
        QSqlQuery q(check);
        REQUIRE(q.exec("PRAGMA user_version;"));
        REQUIRE(q.next());
        CHECK(q.value(0).toInt() == 0);
        REQUIRE(q.exec("SELECT user_password FROM Users;"));
        CHECK(q.next());
        check.close();
    }
    QSqlDatabase::removeDatabase("m2-check");
}

TEST_CASE("A new file is created at v2 and opens again") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("fresh.db");
    {
        Database db(path);
        REQUIRE(db.open().isOk());
        CHECK(db.getSchemaVersion() == 2);
        REQUIRE(db.createUser("ana", "ana@scu.edu", "h", "s", 1, system_clock::now()).isOk());
    }
    Database again(path);
    REQUIRE(again.open().isOk());
    CHECK(again.findUserByName("ana").isOk());
}
}
