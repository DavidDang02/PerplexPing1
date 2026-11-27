// The Panopto sign-in client's offline parts: reading links, reading
// Panopto's recording list, and reading settings without ever taking a
// secret. (Sign-in itself needs a real SCU account, so it is tried by hand.)

#include "doctest.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "panoptooauthclient.h"
#include "recording.h"

TEST_CASE("Panopto: the lecture id comes from a viewer or embed link") {
    const QString id = "0c1d2e3f-4a5b-6c7d-8e9f-a0b1c2d3e4f5";
    CHECK(PanoptoOAuthClient::getLectureId(
              "https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=" + id) == id);
    CHECK(PanoptoOAuthClient::getLectureId(
              "  https://scu.hosted.panopto.com/Panopto/Pages/Embed.aspx?id=" + id + "&start=30 ") == id);
    CHECK(PanoptoOAuthClient::getLectureId("https://example.com/Viewer.aspx?id=" + id).isEmpty());
    CHECK(PanoptoOAuthClient::getLectureId("https://scu.hosted.panopto.com/Viewer.aspx").isEmpty());
    CHECK(PanoptoOAuthClient::getLectureId("https://scu.hosted.panopto.com/Viewer.aspx?id=<script>").isEmpty());
    CHECK(PanoptoOAuthClient::getLectureId("not a link").isEmpty());
}

TEST_CASE("Panopto: recordings are read as local time, with a 70 min default") {
    QJsonObject withTime{{"Name", "CSCI 187 Lecture"},
                         {"StartTime", "2026-11-09T14:15:00Z"},
                         {"Duration", 4200.0},
                         {"Urls", QJsonObject{{"ViewerUrl", "https://scu.hosted.panopto.com/v?id=a"}}}};
    QJsonObject noDuration{{"Name", "No duration yet"},
                           {"StartTime", "2026-11-11T14:15:00Z"},
                           {"Urls", QJsonObject{{"ViewerUrl", "https://scu.hosted.panopto.com/v?id=b"}}}};
    QJsonObject notReady{{"Name", "Still processing"}, {"StartTime", QJsonValue()}};
    QJsonObject noUrl{{"Name", "No link"}, {"StartTime", "2026-11-11T14:15:00Z"}};
    const QJsonObject page{{"Results", QJsonArray{withTime, noDuration, notReady, noUrl}}};

    const auto found = PanoptoOAuthClient::parseRecordings(page);
    REQUIRE(found.size() == 2);
    CHECK(found[0].name == "CSCI 187 Lecture");
    CHECK(found[0].viewerUrl == "https://scu.hosted.panopto.com/v?id=a");
    CHECK(found[0].durationSeconds == 4200);
    // "14:15:00Z" means 2:15 PM on the wall clock here, not 2:15 PM in UTC.
    const QDateTime local = QDateTime::fromSecsSinceEpoch(
        std::chrono::duration_cast<std::chrono::seconds>(found[0].start.time_since_epoch()).count());
    CHECK(local.time() == QTime(14, 15));
    CHECK(local.date() == QDate(2026, 11, 9));
    CHECK(found[1].durationSeconds == Recording::kDefaultDurationSeconds);
    CHECK(PanoptoOAuthClient::parseRecordings(QJsonObject{}).empty());
}

TEST_CASE("Panopto: settings come from .env, and a secret line is never read") {
    qunsetenv("PANOPTO_HOST");
    qunsetenv("PANOPTO_CLIENT_ID");
    qunsetenv("PANOPTO_REDIRECT_URL");
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(".env");
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("# Panopto\n"
                   "PANOPTO_HOST=https://scu.hosted.panopto.com/\n"
                   "PANOPTO_CLIENT_ID = abc-123\n"
                   "PANOPTO_CLIENT_SECRET=do-not-read-me\n");
    }
    const auto settings = PanoptoOAuthClient::loadSettings(path);
    CHECK(settings.isComplete());
    CHECK(settings.host == "scu.hosted.panopto.com");
    CHECK(settings.clientId == "abc-123");
    CHECK(settings.redirect == QUrl("http://localhost:8080/callback"));

    SUBCASE("the environment wins over the file") {
        qputenv("PANOPTO_CLIENT_ID", "from-env");
        CHECK(PanoptoOAuthClient::loadSettings(path).clientId == "from-env");
        qunsetenv("PANOPTO_CLIENT_ID");
    }
    SUBCASE("no file and no environment: not set up") {
        CHECK_FALSE(PanoptoOAuthClient::loadSettings(dir.filePath("missing.env")).isComplete());
    }
}
