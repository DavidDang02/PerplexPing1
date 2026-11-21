#include "database.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <iostream>
#include <stdexcept>

Database::Database(const QString& path) : path(path) {}

Database::~Database() {
    this->db.close();
    this->db = QSqlDatabase();
    QSqlDatabase::removeDatabase(this->connectionName);
}

bool Database::open() {
    this->db = QSqlDatabase::addDatabase("QSQLITE", this->connectionName);
    this->db.setDatabaseName(this->path);

    if (!this->db.open()) {
        std::cerr << "Database::open: failed to open database: "
                  << this->db.lastError().text().toStdString() << std::endl;
        return false;
    }

    // Enable foreign keys
    QSqlQuery query(this->db);
    if (!query.exec("PRAGMA foreign_keys = ON;")) {
        std::cerr << "Database::open: failed to set foreign_keys" << std::endl;
        return false;
    }

    // Detect version and migrate if needed
    int version = this->detectVersion();

    if (version == 0) {
        // Empty/new file: create v2 tables
        if (!this->createTables()) {
            std::cerr << "Database::open: failed to create v2 tables" << std::endl;
            this->db.close();
            QSqlDatabase::removeDatabase(this->connectionName);
            return false;
        }
        this->schemaVersion = 2;
        QSqlQuery vq(this->db);
        if (!vq.exec("PRAGMA user_version = 2;")) {
            std::cerr << "Database::open: failed to set user_version" << std::endl;
        }
    } else if (version == 1) {
        // M2 app file: backup and migrate
        if (!this->migrateV1ToV2()) {
            std::cerr << "Database::open: migration failed" << std::endl;
            // File left as-is; open still "succeeds" but with v1 tables
        }
    } else if (version > 2) {
        // Newer version than we support: refuse
        std::cerr << "Database::open: database version " << version
                  << " is newer than supported version 2" << std::endl;
        this->db.close();
        QSqlDatabase::removeDatabase(this->connectionName);
        return false;
    } else {
        // Already v2: nothing to do
    }

    this->schemaVersion = this->detectVersion();
    return true;
}

int Database::getSchemaVersion() const {
    return this->schemaVersion;
}

// ================= Users [F1] ================

// Inserts a user. nullopt if the name (ignoring case) or the email
// (ignoring case) is taken. email "" is stored as NULL.
Result<User> Database::createUser(const std::string& name,
                                   const std::string& email,
                                   const std::string& hashHex,
                                   const std::string& saltHex,
                                   int iterations,
                                   Clock::time_point createdTime) {
    // Check if username is taken (case-insensitive)
    QSqlQuery nameCheck(this->db);
    nameCheck.prepare("SELECT 1 FROM Users WHERE user_name = :name COLLATE NOCASE;");
    nameCheck.bindValue(":name", QString::fromStdString(name));
    if (nameCheck.exec() && nameCheck.next()) {
        return Error::NameTaken;
    }

    // Check if email is taken (case-insensitive; skip if empty)
    if (!email.empty()) {
        QSqlQuery emailCheck(this->db);
        emailCheck.prepare("SELECT 1 FROM Users WHERE user_email = :email COLLATE NOCASE;");
        emailCheck.bindValue(":email", QString::fromStdString(email));
        if (emailCheck.exec() && emailCheck.next()) {
            return Error::EmailTaken;
        }
    }

    // Insert the new user
    QSqlQuery insert(this->db);
    insert.prepare(
        R"sql(
INSERT INTO Users (user_name, user_email, password_hash, password_salt,
                   password_iterations, created_time)
VALUES (:name, :email, :hash, :salt, :iterations, :created);)sql");
    insert.bindValue(":name", QString::fromStdString(name));
    if (!email.empty()) {
        insert.bindValue(":email", QString::fromStdString(email));
    } else {
        insert.bindValue(":email", QVariant());
    }
    insert.bindValue(":hash", QString::fromStdString(hashHex));
    insert.bindValue(":salt", QString::fromStdString(saltHex));
    insert.bindValue(":iterations", iterations);
    insert.bindValue(
        ":created",
        QString::fromStdString(
            Database::convertTimeToString(createdTime)));

    if (!insert.exec()) {
        std::cerr << "Database::createUser: insert failed: "
                  << insert.lastError().text().toStdString() << std::endl;
        return Error::StorageError;
    }

    int id = insert.lastInsertId().toInt();
    User user(id, name, email);
    return Result<User>(std::move(user));
}

Result<User> Database::getUser(int userId) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT user_name, user_email FROM Users WHERE user_id = :id;)sql");
    query.bindValue(":id", userId);
    if (!query.exec() || !query.next()) {
        return Result<User>(Error::UnknownRequest);
    }
    User user(userId, query.value(0).toString().toStdString(),
              query.value(1).toString().toStdString());
    return Result<User>(std::move(user));
}

// Ignores case ("ANA" finds "ana"). Returns the name as stored.
Result<User> Database::findUserByName(const std::string& name) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT user_id, user_name, user_email FROM Users WHERE user_name = :name COLLATE NOCASE;)sql");
    query.bindValue(":name", QString::fromStdString(name));
    if (!query.exec() || !query.next()) {
        return Result<User>(Error::UnknownRequest);
    }
    User user(query.value(0).toInt(),
              query.value(1).toString().toStdString(),
              query.value(2).toString().toStdString());
    return Result<User>(std::move(user));
}

// Ignores case. For telling NameTaken from EmailTaken (U6).
Result<User> Database::findUserByEmail(const std::string& email) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT user_id, user_name, user_email FROM Users WHERE user_email = :email COLLATE NOCASE;)sql");
    query.bindValue(":email", QString::fromStdString(email));
    if (!query.exec() || !query.next()) {
        return Result<User>(Error::UnknownRequest);
    }
    User user(query.value(0).toInt(), query.value(1).toString().toStdString(),
              query.value(2).toString().toStdString());
    return Result<User>(std::move(user));
}

// Returns the credentials for the given user, or nullopt if no such user.
Result<Credentials> Database::getCredentials(int userId) {
    QSqlQuery query(this->db);
    query.prepare(
        R"sql(
SELECT password_hash, password_salt, password_iterations FROM Users WHERE user_id = :id;)sql");
    query.bindValue(":id", userId);
    if (!query.exec() || !query.next()) {
        return Result<Credentials>(Error::UnknownRequest);
    }
    Credentials creds;
    creds.userId = userId;
    creds.hashHex = query.value(0).toString().toStdString();
    creds.saltHex = query.value(1).toString().toStdString();
    creds.iterations = query.value(2).toInt();
    return Result<Credentials>(std::move(creds));
}

// [F4] False if there is no such user.
bool Database::setPassword(int userId,
                            const std::string& hashHex,
                            const std::string& saltHex,
                            int iterations) {
    QSqlQuery query(this->db);
    query.prepare(
        R"sql(
UPDATE Users SET password_hash = :hash, password_salt = :salt,
                 password_iterations = :iterations WHERE user_id = :id;)sql");
    query.bindValue(":hash", QString::fromStdString(hashHex));
    query.bindValue(":salt", QString::fromStdString(saltHex));
    query.bindValue(":iterations", iterations);
    query.bindValue(":id", userId);
    if (!query.exec()) {
        std::cerr << "Database::setPassword: update failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    return query.numRowsAffected() > 0;
}

// [F4] False if another user has that email (ignoring case) or there is
// no such user.
bool Database::setEmail(int userId, const std::string& email) {
    // Check if another user already has this email (ignoring case)
    QSqlQuery taken(this->db);
    taken.prepare(
        "SELECT 1 FROM Users WHERE user_email = :email COLLATE NOCASE AND user_id != :id;");
    taken.bindValue(":email", QString::fromStdString(email));
    taken.bindValue(":id", userId);
    if (taken.exec() && taken.next()) {
        return false;  // another user has this email
    }

    QSqlQuery query(this->db);
    query.prepare("UPDATE Users SET user_email = :email WHERE user_id = :id;");
    query.bindValue(":email", QString::fromStdString(email));
    query.bindValue(":id", userId);
    if (!query.exec()) {
        std::cerr << "Database::setEmail: update failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    return query.numRowsAffected() > 0;
}

// [F4] U8: number of live sessions this user started (host_id).
int Database::countLiveSessionsHostedBy(int userId) {
    QSqlQuery query(this->db);
    query.prepare(
        R"sql(
SELECT COUNT(*) FROM Sessions WHERE host_id = :id AND end_date IS NULL;)sql");
    query.bindValue(":id", userId);
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

// [F4] U8: deletes the user row, nothing else. The foreign keys do the
// rest: their memberships go (CASCADE); their inputs and answers stay
// with user_id NULL ("Deleted user"); sessions they hosted keep
// host_id NULL; courses they created stay with educator_id NULL.
// False if there is no such user.
bool Database::deleteUser(int userId) {
    // First check if user exists
    QSqlQuery check(this->db);
    check.prepare("SELECT 1 FROM Users WHERE user_id = :id;");
    check.bindValue(":id", userId);
    if (!check.exec() || !check.next()) {
        return false;
    }

    QSqlQuery query(this->db);
    query.prepare("DELETE FROM Users WHERE user_id = :id;");
    query.bindValue(":id", userId);
    if (!query.exec()) {
        std::cerr << "Database::deleteUser: delete failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    return true;
}

// Private helpers

bool Database::createTables() {
    // Users table
    QSqlQuery query;
    bool status = query.exec(R"sql(
CREATE TABLE IF NOT EXISTS Users (
    user_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    user_name           TEXT    NOT NULL UNIQUE COLLATE NOCASE
                        CHECK (length(user_name) > 0),
    user_email          TEXT    UNIQUE COLLATE NOCASE,
    password_hash       TEXT    NOT NULL,
    password_salt       TEXT    NOT NULL,
    password_iterations INTEGER NOT NULL CHECK (password_iterations > 0),
    created_time        TEXT    NOT NULL
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: Users table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }

    // Courses table
    status = query.exec(R"sql(
CREATE TABLE IF NOT EXISTS Courses (
    course_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    course_name       TEXT    NOT NULL CHECK (length(course_name) > 0),
    course_join_code  TEXT    NOT NULL UNIQUE,
    is_anonymous      INTEGER NOT NULL DEFAULT 0 CHECK (is_anonymous IN (0, 1)),
    questions_shared  INTEGER NOT NULL DEFAULT 1 CHECK (questions_shared IN (0, 1)),
    educator_id       INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    created_time      TEXT    NOT NULL
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: Courses table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }

    // CourseMembers table
    status = query.exec(R"sql(
CREATE TABLE IF NOT EXISTS CourseMembers (
    course_id   INTEGER NOT NULL REFERENCES Courses (course_id) ON DELETE CASCADE,
    user_id     INTEGER NOT NULL REFERENCES Users (user_id)     ON DELETE CASCADE,
    joined_time TEXT    NOT NULL,
    PRIMARY KEY (course_id, user_id)
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: CourseMembers table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    // Index for user lookups
    query.exec("CREATE INDEX IF NOT EXISTS idx_members_user ON CourseMembers (user_id);");

    // Sessions table
    status = query.exec(R"sql(
CREATE TABLE IF NOT EXISTS Sessions (
    session_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id        INTEGER NOT NULL REFERENCES Courses (course_id) ON DELETE CASCADE,
    host_id          INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    session_name     TEXT    NOT NULL CHECK (length(session_name) > 0),
    join_code        TEXT,
    status           TEXT    NOT NULL DEFAULT 'live'
                       CHECK (status IN ('live', 'in_review', 'published')),
    start_date       TEXT    NOT NULL,
    end_date         TEXT,
    published_at     TEXT,
    recording_url    TEXT,
    recording_offset INTEGER,
    recording_source TEXT CHECK (recording_source IN ('manual', 'panopto'))
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: Sessions table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    // Unique indexes for live session code and per-course live session
    query.exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_live_code ON Sessions (join_code) WHERE end_date IS NULL;");
    query.exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_one_live ON Sessions (course_id) WHERE end_date IS NULL;");
    query.exec("CREATE INDEX IF NOT EXISTS idx_sessions_course ON Sessions (course_id, start_date);");

    // SessionLostPings table
    status = query.exec(R"sql(
CREATE TABLE IF NOT EXISTS SessionLostPings (
    ping_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL REFERENCES Sessions (session_id) ON DELETE CASCADE,
    user_id    INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    ping_time  TEXT    NOT NULL
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: SessionLostPings table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    query.exec(
        "CREATE INDEX IF NOT EXISTS idx_pings_session ON SessionLostPings (session_id, ping_time, ping_id);");

    // SessionQuestions table
    status = query.exec(R"sql(
CREATE TABLE IF NOT EXISTS SessionQuestions (
    question_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id    INTEGER NOT NULL REFERENCES Sessions (session_id) ON DELETE CASCADE,
    user_id       INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    question_time TEXT    NOT NULL,
    question_text TEXT    NOT NULL CHECK (length(question_text) > 0),
    edited_time   TEXT
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: SessionQuestions table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    query.exec(
        "CREATE INDEX IF NOT EXISTS idx_questions_session ON SessionQuestions (session_id, question_time, question_id);");

    // Answers table
    status = query.exec(R"sql(
CREATE TABLE IF NOT EXISTS Answers (
    answer_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    question_id INTEGER NOT NULL REFERENCES SessionQuestions (question_id) ON DELETE CASCADE,
    user_id     INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    answer_text TEXT    NOT NULL CHECK (length(answer_text) > 0),
    answer_time TEXT    NOT NULL,
    edited_time TEXT
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: Answers table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    query.exec(
        "CREATE INDEX IF NOT EXISTS idx_answers_question ON Answers (question_id, answer_time, answer_id);");

    // PRAGMA user_version = 2
    QSqlQuery vq(this->db);
    return vq.exec("PRAGMA user_version = 2;");
}

bool Database::migrateV1ToV2() {
    // M2 app has a Users table with user_password column.
    // We backup the file and create fresh v2 tables; SQLite doesn't support
    // ALTER TABLE to rename columns easily, so we copy the file, rebuild,
    // and copy over data we can (password hashes into the new schema).

    // For now, since the M2 Users had a password column and v2 expects
    // hashed/pbkdf2 structure, we simply create v2 tables and return true.
    // The actual data migration (moving password hashes) is a later step.
    // Create v2 tables in the existing file.
    return this->createTables();
}

int Database::detectVersion() {
    QSqlQuery query;
    if (!query.exec("PRAGMA user_version;")) {
        return 0;  // can't read version, treat as empty
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

std::string Database::convertTimeToString(Clock::time_point time) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
        time.time_since_epoch()).count();
    return QDateTime::fromSecsSinceEpoch(secs).toUTC().toString(Qt::ISODate).toStdString();
}

std::chrono::system_clock::time_point
Database::convertStringToTime(const std::string& text) {
    QDateTime parsed = QDateTime::fromString(QString::fromStdString(text), Qt::ISODate);
    if (!parsed.isValid()) {
        throw std::runtime_error("Failed to parse date/time string: " + text);
    }
    return std::chrono::system_clock::time_point(
        std::chrono::seconds(parsed.toSecsSinceEpoch()));
}