#include "database.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>

#include <iostream>
#include <stdexcept>

Database::Database(const QString& path)
    : path(path)
    , connectionName("perplexping_" + QUuid::createUuid().toString(QUuid::WithoutBraces))
{}

Database::~Database() {
    this->db.close();
    this->db = QSqlDatabase();
    QSqlDatabase::removeDatabase(this->connectionName);
}

Result<bool> Database::open() {
    this->db = QSqlDatabase::addDatabase("QSQLITE", this->connectionName);
    this->db.setDatabaseName(this->path);
    if (!this->db.open()) {
        std::cerr << "Database::open: failed to open database: "
                  << this->db.lastError().text().toStdString() << std::endl;
        return Result<bool>(Error::StorageError);
    }

    // Enable foreign keys
    QSqlQuery query(this->db);
    if (!query.exec("PRAGMA foreign_keys = ON;")) {
        std::cerr << "Database::open: failed to set foreign_keys" << std::endl;
        return Result<bool>(Error::StorageError);
    }

    // Detect version and migrate if needed
    int version = this->detectVersion();

    if (version == 0) {
        // Empty/new file: create the v2 tables and stamp the version in one
        // transaction, so a failure never leaves a half-built file.
        QSqlQuery vq(this->db);
        if (!this->db.transaction() || !this->createTables() ||
            !vq.exec("PRAGMA user_version = 2;") || !this->db.commit()) {
            std::cerr << "Database::open: failed to create v2 tables: "
                      << this->db.lastError().text().toStdString() << std::endl;
            this->db.rollback();
            this->db.close();
            return Result<bool>(Error::StorageError);
        }
        this->schemaVersion = 2;
    } else if (version == 1) {
        // M2 app file: migrate. No migration exists yet, so this refuses
        // the file without touching it.
        if (!this->migrateV1ToV2()) {
            std::cerr << "Database::open: " << this->path.toStdString()
                      << " is an old M2 PerplexPing database. Delete or rename it "
                         "and start the server again." << std::endl;
            this->db.close();
            return Result<bool>(Error::StorageError);
        }
    } else if (version < 0) {
        // user_version 0 but tables that are not ours: not a PerplexPing file.
        std::cerr << "Database::open: " << this->path.toStdString()
                  << " is not a PerplexPing database." << std::endl;
        this->db.close();
        return Result<bool>(Error::StorageError);
    } else if (version > 2) {
        // Newer version than we support: refuse
        std::cerr << "Database::open: database version " << version
                  << " is newer than supported version 2" << std::endl;
        this->db.close();
        return Result<bool>(Error::StorageError);
    } else {
        // Already v2. Files made before Nov 27 lack Courses.panopto_folder_id
        // (added for "Find recording"); add it, keeping every row.
        if (!this->addMissingColumns()) {
            this->db.close();
            return Result<bool>(Error::StorageError);
        }
    }

    this->schemaVersion = this->detectVersion();
    return Result<bool>(true);
}

int Database::getSchemaVersion() const {
    return this->schemaVersion;
}

// ================= Users [F1] ================

Result<User> Database::createUser(const std::string& name,
                                   const std::string& email,
                                   const std::string& hashHex,
                                   const std::string& saltHex,
                                   int iterations,
                                   Clock::time_point createdTime) {
    QSqlQuery nameCheck(this->db);
    nameCheck.prepare("SELECT 1 FROM Users WHERE user_name COLLATE NOCASE = :name;");
    nameCheck.bindValue(":name", QString::fromStdString(name));
    if (nameCheck.exec() && nameCheck.next()) {
        return Result<User>(Error::NameTaken);
    }

    if (!email.empty()) {
        QSqlQuery emailCheck(this->db);
        emailCheck.prepare("SELECT 1 FROM Users WHERE user_email COLLATE NOCASE = :email;");
        emailCheck.bindValue(":email", QString::fromStdString(email));
        if (emailCheck.exec() && emailCheck.next()) {
            return Result<User>(Error::EmailTaken);
        }
    }

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
        return Result<User>(Error::StorageError);
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

Result<User> Database::findUserByName(const std::string& name) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT user_id, user_name, user_email FROM Users WHERE user_name COLLATE NOCASE = :name;)sql");
    query.bindValue(":name", QString::fromStdString(name));
    if (!query.exec() || !query.next()) {
        return Result<User>(Error::UnknownRequest);
    }
    User user(query.value(0).toInt(),
              query.value(1).toString().toStdString(),
              query.value(2).toString().toStdString());
    return Result<User>(std::move(user));
}

Result<User> Database::findUserByEmail(const std::string& email) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT user_id, user_name, user_email FROM Users WHERE user_email COLLATE NOCASE = :email;)sql");
    query.bindValue(":email", QString::fromStdString(email));
    if (!query.exec() || !query.next()) {
        return Result<User>(Error::UnknownRequest);
    }
    User user(query.value(0).toInt(),
              query.value(1).toString().toStdString(),
              query.value(2).toString().toStdString());
    return Result<User>(std::move(user));
}

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

Result<bool> Database::setPassword(int userId,
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
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::setEmail(int userId, const std::string& email) {
    QSqlQuery taken(this->db);
    taken.prepare("SELECT 1 FROM Users WHERE user_email = :email AND user_id != :id;");
    taken.bindValue(":email", QString::fromStdString(email));
    taken.bindValue(":id", userId);
    if (taken.exec() && taken.next()) {
        return Result<bool>(Error::EmailTaken);
    }

    QSqlQuery update(this->db);
    update.prepare("UPDATE Users SET user_email = :email WHERE user_id = :id;");
    update.bindValue(":email", QString::fromStdString(email));
    update.bindValue(":id", userId);
    if (!update.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(update.numRowsAffected() > 0);
}

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

Result<bool> Database::deleteUser(int userId) {
    QSqlQuery check(this->db);
    check.prepare("SELECT 1 FROM Users WHERE user_id = :id;");
    check.bindValue(":id", userId);
    if (!check.exec()) {
        return Result<bool>(Error::StorageError);
    }
    if (!check.next()) {
        return Result<bool>(false);  // no such row (header)
    }

    QSqlQuery query(this->db);
    query.prepare("DELETE FROM Users WHERE user_id = :id;");
    query.bindValue(":id", userId);
    if (!query.exec()) {
        std::cerr << "Database::deleteUser: delete failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(true);
}

// ================= Courses [F2] =================

Result<Course> Database::createCourse(const std::string& name, const std::string& joinCode,
                                bool anonymous, bool questionsShared, int educatorUserId,
                                Clock::time_point createdTime) {
    QSqlQuery codeCheck(this->db);
    codeCheck.prepare("SELECT 1 FROM Courses WHERE course_join_code = :code;");
    codeCheck.bindValue(":code", QString::fromStdString(joinCode));
    if (codeCheck.exec() && codeCheck.next()) {
        return Result<Course>(Error::NameTaken);
    }

    QSqlQuery insert(this->db);
    insert.prepare(
        R"sql(
INSERT INTO Courses (course_name, course_join_code, is_anonymous,
                     questions_shared, educator_id, created_time)
VALUES (:name, :code, :anon, :shared, :educator, :created);)sql");
    insert.bindValue(":name", QString::fromStdString(name));
    insert.bindValue(":code", QString::fromStdString(joinCode));
    insert.bindValue(":anon", anonymous ? 1 : 0);
    insert.bindValue(":shared", questionsShared ? 1 : 0);
    insert.bindValue(":educator", educatorUserId);
    insert.bindValue(":created", QString::fromStdString(convertTimeToString(createdTime)));

    if (!insert.exec()) {
        std::cerr << "Database::createCourse: insert failed: "
                  << insert.lastError().text().toStdString() << std::endl;
        return Result<Course>(Error::StorageError);
    }

    Course course;
    course.setId(insert.lastInsertId().toInt());
    course.setName(name);
    course.setJoinCode(joinCode);
    course.setAnonymous(anonymous);
    course.setQuestionsShared(questionsShared);
    course.setEducatorId(educatorUserId);
    course.setRole(CourseRole::Educator);
    // created_time is stored in the DB but not exposed on the Course model object.
    return Result<Course>(std::move(course));
}

Result<Course> Database::getCourse(int courseId, int userId) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT course_id, course_name, course_join_code, is_anonymous,
       questions_shared, educator_id, created_time, panopto_folder_id
FROM Courses WHERE course_id = :id;)sql");
    query.bindValue(":id", courseId);
    if (!query.exec() || !query.next()) {
        return Result<Course>(Error::UnknownRequest);
    }

    Course course;
    course.setId(query.value(0).toInt());
    course.setName(query.value(1).toString().toStdString());
    course.setJoinCode(query.value(2).toString().toStdString());
    course.setAnonymous(query.value(3).toInt());
    course.setQuestionsShared(query.value(4).toInt());
    course.setEducatorId(query.value(5).toInt());
        course.setPanoptoFolderId(query.value(7).toString().toStdString());

    if (course.getEducatorId() != 0 && course.getEducatorId() == userId) {
        course.setRole(CourseRole::Educator);
    } else {
        QSqlQuery mem(this->db);
        mem.prepare("SELECT 1 FROM CourseMembers WHERE course_id = :cid AND user_id = :uid;");
        mem.bindValue(":cid", course.getId());
        mem.bindValue(":uid", userId);
        if (mem.exec() && mem.next()) {
            course.setRole(CourseRole::Student);
        } else {
            course.setRole(CourseRole::None);
        }
    }

    // Collect student IDs
    std::vector<int> studentIds;
    QSqlQuery stu(this->db);
    stu.prepare("SELECT user_id FROM CourseMembers WHERE course_id = :cid ORDER BY joined_time;");
    stu.bindValue(":cid", course.getId());
    if (stu.exec()) {
        while (stu.next()) {
            studentIds.push_back(stu.value(0).toInt());
        }
    }
    course.setStudentIds(studentIds);

    // Collect session IDs
    std::vector<int> sessionIds;
    QSqlQuery ses(this->db);
    ses.prepare("SELECT session_id FROM Sessions WHERE course_id = :cid ORDER BY start_date DESC;");
    ses.bindValue(":cid", course.getId());
    if (ses.exec()) {
        while (ses.next()) {
            sessionIds.push_back(ses.value(0).toInt());
        }
    }
    course.setSessionIds(sessionIds);

    // Live session ID
    QSqlQuery live(this->db);
    live.prepare("SELECT session_id FROM Sessions WHERE course_id = :cid AND end_date IS NULL;");
    live.bindValue(":cid", course.getId());
    if (live.exec() && live.next()) {
        course.setLiveSessionId(live.value(0).toInt());
    }

    return Result<Course>(std::move(course));
}

Result<Course> Database::findCourseByJoinCode(const std::string& joinCode, int userId) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT course_id, course_name, course_join_code, is_anonymous,
       questions_shared, educator_id, created_time, panopto_folder_id
FROM Courses WHERE course_join_code = :code;)sql");
    query.bindValue(":code", QString::fromStdString(joinCode));
    if (!query.exec() || !query.next()) {
        return Result<Course>(Error::NoSuchCourseCode);
    }

    Course course;
    course.setId(query.value(0).toInt());
    course.setName(query.value(1).toString().toStdString());
    course.setJoinCode(query.value(2).toString().toStdString());
    course.setAnonymous(query.value(3).toInt());
    course.setQuestionsShared(query.value(4).toInt());
    course.setEducatorId(query.value(5).toInt());
        course.setPanoptoFolderId(query.value(7).toString().toStdString());

    if (course.getEducatorId() != 0 && course.getEducatorId() == userId) {
        course.setRole(CourseRole::Educator);
    } else {
        QSqlQuery mem(this->db);
        mem.prepare("SELECT 1 FROM CourseMembers WHERE course_id = :cid AND user_id = :uid;");
        mem.bindValue(":cid", course.getId());
        mem.bindValue(":uid", userId);
        if (mem.exec() && mem.next()) {
            course.setRole(CourseRole::Student);
        } else {
            course.setRole(CourseRole::None);
        }
    }

    // Collect student IDs
    std::vector<int> studentIds;
    QSqlQuery stu(this->db);
    stu.prepare("SELECT user_id FROM CourseMembers WHERE course_id = :cid ORDER BY joined_time;");
    stu.bindValue(":cid", course.getId());
    if (stu.exec()) {
        while (stu.next()) {
            studentIds.push_back(stu.value(0).toInt());
        }
    }
    course.setStudentIds(studentIds);

    return Result<Course>(std::move(course));
}

Result<std::vector<Course>> Database::listCoursesForUser(int userId) {
    std::vector<Course> courses;

    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT c.course_id, c.course_name, c.course_join_code, c.is_anonymous,
       c.questions_shared, c.educator_id, c.created_time, c.panopto_folder_id
FROM Courses c
JOIN CourseMembers cm ON c.course_id = cm.course_id
WHERE cm.user_id = :uid
UNION
SELECT c.course_id, c.course_name, c.course_join_code, c.is_anonymous,
       c.questions_shared, c.educator_id, c.created_time, c.panopto_folder_id
FROM Courses c
WHERE c.educator_id = :uid
ORDER BY c.course_name COLLATE NOCASE;)sql");
    query.bindValue(":uid", userId);
    if (!query.exec()) {
        return Result<std::vector<Course>>(Error::StorageError);
    }

    while (query.next()) {
        Course course;
        course.setId(query.value(0).toInt());
        course.setName(query.value(1).toString().toStdString());
        course.setJoinCode(query.value(2).toString().toStdString());
        course.setAnonymous(query.value(3).toInt());
        course.setQuestionsShared(query.value(4).toInt());
        course.setEducatorId(query.value(5).toInt());
        course.setPanoptoFolderId(query.value(7).toString().toStdString());

        if (course.getEducatorId() != 0 && course.getEducatorId() == userId) {
            course.setRole(CourseRole::Educator);
        } else {
            course.setRole(CourseRole::Student);
        }

        // Collect student IDs
        std::vector<int> studentIds;
        QSqlQuery stu(this->db);
        stu.prepare("SELECT user_id FROM CourseMembers WHERE course_id = :cid ORDER BY joined_time;");
        stu.bindValue(":cid", course.getId());
        if (stu.exec()) {
            while (stu.next()) {
                studentIds.push_back(stu.value(0).toInt());
            }
        }
        course.setStudentIds(studentIds);

        // Collect session IDs
        std::vector<int> sessionIds;
        QSqlQuery ses(this->db);
        ses.prepare("SELECT session_id FROM Sessions WHERE course_id = :cid ORDER BY start_date DESC;");
        ses.bindValue(":cid", course.getId());
        if (ses.exec()) {
            while (ses.next()) {
                sessionIds.push_back(ses.value(0).toInt());
            }
        }
        course.setSessionIds(sessionIds);

        // Live session ID
        QSqlQuery live(this->db);
        live.prepare("SELECT session_id FROM Sessions WHERE course_id = :cid AND end_date IS NULL;");
        live.bindValue(":cid", course.getId());
        if (live.exec() && live.next()) {
            course.setLiveSessionId(live.value(0).toInt());
        }

        courses.push_back(std::move(course));
    }

    return Result<std::vector<Course>>(std::move(courses));
}

bool Database::addMissingColumns() {
    QSqlQuery info(this->db);
    if (!info.exec("PRAGMA table_info(Courses);")) {
        return false;
    }
    while (info.next()) {
        if (info.value(1).toString() == "panopto_folder_id") {
            return true;
        }
    }
    QSqlQuery alter(this->db);
    if (!alter.exec("ALTER TABLE Courses ADD COLUMN panopto_folder_id TEXT "
                    "CHECK (panopto_folder_id IS NULL OR length(panopto_folder_id) <= 64);")) {
        std::cerr << "Database::open: could not add Courses.panopto_folder_id: "
                  << alter.lastError().text().toStdString() << std::endl;
        return false;
    }
    return true;
}

Result<bool> Database::setPanoptoFolder(int courseId, const std::string& folderId) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE Courses SET panopto_folder_id = :folder WHERE course_id = :id;");
    // Empty unlinks it (NULL).
    query.bindValue(":folder", folderId.empty() ? QVariant() : QVariant(QString::fromStdString(folderId)));
    query.bindValue(":id", courseId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::updateCourse(int courseId, const std::string& name, bool anonymous,
                             bool questionsShared) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE Courses SET course_name = :name, is_anonymous = :anon, questions_shared = :shared WHERE course_id = :id;");
    query.bindValue(":name", QString::fromStdString(name));
    query.bindValue(":anon", anonymous ? 1 : 0);
    query.bindValue(":shared", questionsShared ? 1 : 0);
    query.bindValue(":id", courseId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::deleteCourse(int courseId) {
    QSqlQuery check(this->db);
    check.prepare("SELECT 1 FROM Courses WHERE course_id = :id;");
    check.bindValue(":id", courseId);
    if (!check.exec()) {
        return Result<bool>(Error::StorageError);
    }
    if (!check.next()) {
        return Result<bool>(false);  // no such row (header)
    }

    QSqlQuery query(this->db);
    query.prepare("DELETE FROM Courses WHERE course_id = :id;");
    query.bindValue(":id", courseId);
    if (!query.exec()) {
        std::cerr << "Database::deleteCourse: delete failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(true);
}

Result<bool> Database::addStudent(int courseId, int userId, Clock::time_point joinedTime) {
    QSqlQuery check(this->db);
    check.prepare("SELECT 1 FROM CourseMembers WHERE course_id = :cid AND user_id = :uid;");
    check.bindValue(":cid", courseId);
    check.bindValue(":uid", userId);
    if (check.exec() && check.next()) {
        return Result<bool>(false);  // already a member
    }

    QSqlQuery query(this->db);
    query.prepare("INSERT INTO CourseMembers (course_id, user_id, joined_time) VALUES (:cid, :uid, :time);");
    query.bindValue(":cid", courseId);
    query.bindValue(":uid", userId);
    query.bindValue(":time", QString::fromStdString(convertTimeToString(joinedTime)));
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(true);
}

CourseRole Database::getRole(int courseId, int userId) {
    QSqlQuery query(this->db);
    query.prepare("SELECT educator_id FROM Courses WHERE course_id = :id;");
    query.bindValue(":id", courseId);
    if (!query.exec() || !query.next()) {
        return CourseRole::None;
    }
    // NULL (the educator deleted their account) is nobody, not user 0.
    if (!query.value(0).isNull() && query.value(0).toInt() == userId) {
        return CourseRole::Educator;
    }
    query.prepare("SELECT 1 FROM CourseMembers WHERE course_id = :cid AND user_id = :uid;");
    query.bindValue(":cid", courseId);
    query.bindValue(":uid", userId);
    if (query.exec() && query.next()) {
        return CourseRole::Student;
    }
    return CourseRole::None;
}

// ================= Sessions [F2] =================

Result<Session> Database::createSession(int courseId, int hostId, const std::string& name,
                                  const std::string& joinCode,
                                  Clock::time_point startTime) {
    QSqlQuery liveCheck(this->db);
    liveCheck.prepare("SELECT 1 FROM Sessions WHERE course_id = :cid AND end_date IS NULL;");
    liveCheck.bindValue(":cid", courseId);
    if (liveCheck.exec() && liveCheck.next()) {
        return Result<Session>(Error::CourseHasLiveSession);
    }

    QSqlQuery codeCheck(this->db);
    codeCheck.prepare("SELECT 1 FROM Sessions WHERE join_code = :code AND end_date IS NULL;");
    codeCheck.bindValue(":code", QString::fromStdString(joinCode));
    if (codeCheck.exec() && codeCheck.next()) {
        return Result<Session>(Error::CourseHasLiveSession);
    }

    QSqlQuery insert(this->db);
    insert.prepare(
        R"sql(
INSERT INTO Sessions (course_id, host_id, session_name, join_code, status, start_date)
VALUES (:cid, :host, :name, :code, 'live', :start);)sql");
    insert.bindValue(":cid", courseId);
    insert.bindValue(":host", hostId);
    insert.bindValue(":name", QString::fromStdString(name));
    insert.bindValue(":code", QString::fromStdString(joinCode));
    insert.bindValue(":start", QString::fromStdString(convertTimeToString(startTime)));

    if (!insert.exec()) {
        std::cerr << "Database::createSession: insert failed: "
                  << insert.lastError().text().toStdString() << std::endl;
        return Result<Session>(Error::StorageError);
    }

    int id = insert.lastInsertId().toInt();
    Session session(id, hostId, startTime);
    session.setCourseId(courseId);
    session.setName(name);
    session.setJoinCode(joinCode);
    // status is Live by default from constructor
    return Result<Session>(std::move(session));
}

Result<Session> Database::getSession(int sessionId) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT session_id, course_id, host_id, session_name, join_code, status,
       start_date, end_date, published_at, recording_url, recording_offset, recording_source
FROM Sessions WHERE session_id = :id;)sql");
    query.bindValue(":id", sessionId);
    if (!query.exec() || !query.next()) {
        return Result<Session>(Error::NoSuchSession);
    }
    return Result<Session>(parseSessionFromQuery(query));
}

Result<Session> Database::findLiveSessionByJoinCode(const std::string& joinCode) {
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT session_id, course_id, host_id, session_name, join_code, status,
       start_date, end_date, published_at, recording_url, recording_offset, recording_source
FROM Sessions WHERE join_code = :code AND end_date IS NULL;)sql");
    query.bindValue(":code", QString::fromStdString(joinCode));
    if (!query.exec() || !query.next()) {
        return Result<Session>(Error::NoSuchSessionCode);
    }
    return Result<Session>(parseSessionFromQuery(query));
}

// ---- Item rows (pings, questions, answers) ----
// Every item query selects these columns so one parser reads them all.
// The LEFT JOIN keeps items whose author was deleted (user_id NULL, U8).
namespace {
const char* const kPingColumns = R"sql(
SELECT p.ping_id, p.session_id, p.user_id, p.ping_time, u.user_name
FROM SessionLostPings p LEFT JOIN Users u ON u.user_id = p.user_id )sql";

const char* const kQuestionColumns = R"sql(
SELECT q.question_id, q.session_id, q.user_id, q.question_time, q.question_text,
       q.edited_time, u.user_name
FROM SessionQuestions q LEFT JOIN Users u ON u.user_id = q.user_id )sql";

// by_edu: the answer's author is the course's educator (A5). False when
// either id is NULL (deleted account), so a deleted student is never "educator".
const char* const kAnswerColumns = R"sql(
SELECT a.answer_id, a.question_id, a.user_id, a.answer_text, a.answer_time,
       a.edited_time, u.user_name,
       (a.user_id IS NOT NULL AND a.user_id = c.educator_id) AS by_edu
FROM Answers a
LEFT JOIN Users u ON u.user_id = a.user_id
JOIN SessionQuestions q ON q.question_id = a.question_id
JOIN Sessions s ON s.session_id = q.session_id
JOIN Courses c ON c.course_id = s.course_id )sql";

// author: column of user_id, name: column of user_name.
template <typename Item>
void readAuthorColumns(QSqlQuery& query, int idColumn, int nameColumn, Item& item) {
    if (query.value(idColumn).isNull()) {
        item.setAuthorDeleted();
    } else {
        item.setAuthor(query.value(idColumn).toInt(),
                       query.value(nameColumn).toString().toStdString());
    }
}
}  // namespace

LostPing Database::parsePing(QSqlQuery& query) {
    LostPing ping(convertStringToTime(query.value(3).toString().toStdString()));
    ping.setId(query.value(0).toInt());
    ping.setSessionId(query.value(1).toInt());
    readAuthorColumns(query, 2, 4, ping);
    return ping;
}

Answer Database::parseAnswer(QSqlQuery& query) {
    Answer a(query.value(0).toInt(), query.value(1).toInt(),
             convertStringToTime(query.value(4).toString().toStdString()),
             query.value(3).toString().toStdString());
    if (!query.value(5).isNull()) {
        a.setEditedTime(convertStringToTime(query.value(5).toString().toStdString()));
    }
    readAuthorColumns(query, 2, 6, a);
    a.setByEducator(query.value(7).toInt() == 1);
    return a;
}

std::vector<Answer> Database::loadAnswers(int questionId) {
    std::vector<Answer> answers;
    QSqlQuery query(this->db);
    query.prepare(QString(kAnswerColumns) + "WHERE a.question_id = :qid;");
    query.bindValue(":qid", questionId);
    if (query.exec()) {
        while (query.next()) {
            answers.push_back(parseAnswer(query));
        }
    }
    return answers;
}

SessionQuestion Database::parseQuestion(QSqlQuery& query) {
    SessionQuestion q(convertStringToTime(query.value(3).toString().toStdString()),
                      query.value(4).toString().toStdString());
    q.setId(query.value(0).toInt());
    q.setSessionId(query.value(1).toInt());
    if (!query.value(5).isNull()) {
        q.setEditedTime(convertStringToTime(query.value(5).toString().toStdString()));
    }
    readAuthorColumns(query, 2, 6, q);
    q.setAnswers(loadAnswers(q.getId()));  // setAnswers sorts: educator first (A5)
    return q;
}

Result<Session> Database::loadSession(int sessionId) {
    Result<Session> sessionRes = getSession(sessionId);
    if (!sessionRes.isOk()) {
        return sessionRes;
    }
    Session session = sessionRes.value;

    Result<std::vector<LostPing>> pings = listLostPings(sessionId);
    Result<std::vector<SessionQuestion>> questions = listQuestions(sessionId);
    if (!pings.isOk() || !questions.isOk()) {
        return Result<Session>(Error::StorageError);
    }
    session.setLostPings(std::move(pings.value));
    session.setQuestions(std::move(questions.value));
    return Result<Session>(std::move(session));
}

// Forward declaration for the session-status text parser, defined at the bottom
// of this file and used by listSessionsForCourse above it.
static SessionStatus statusFromText(const std::string& text);

Result<std::vector<SessionSummary>> Database::listSessionsForCourse(int courseId) {
    std::vector<SessionSummary> summaries;
    QSqlQuery query(this->db);
    query.prepare(R"sql(
SELECT session_id, session_name, status, start_date, end_date
FROM Sessions WHERE course_id = :cid ORDER BY start_date DESC;)sql");
    query.bindValue(":cid", courseId);
    if (!query.exec()) {
        return Result<std::vector<SessionSummary>>(Error::StorageError);
    }
    while (query.next()) {
        SessionSummary s;
        s.id = query.value(0).toInt();
        s.name = query.value(1).toString().toStdString();
        s.status = statusFromText(query.value(2).toString().toStdString());
        s.startTime = convertStringToTime(query.value(3).toString().toStdString());
        if (!query.value(4).isNull()) {
            s.endTime = convertStringToTime(query.value(4).toString().toStdString());
        }

        QSqlQuery c1(this->db);
        c1.prepare("SELECT COUNT(*) FROM SessionLostPings WHERE session_id = :sid;");
        c1.bindValue(":sid", s.id);
        if (c1.exec() && c1.next()) s.lostCount = c1.value(0).toInt();

        QSqlQuery c2(this->db);
        c2.prepare("SELECT COUNT(*) FROM SessionQuestions WHERE session_id = :sid;");
        c2.bindValue(":sid", s.id);
        if (c2.exec() && c2.next()) s.questionCount = c2.value(0).toInt();

        QSqlQuery c3(this->db);
        c3.prepare("SELECT 1 FROM Sessions WHERE session_id = :sid AND recording_url IS NOT NULL;");
        c3.bindValue(":sid", s.id);
        if (c3.exec() && c3.next()) s.hasRecording = true;

        summaries.push_back(std::move(s));
    }
    return Result<std::vector<SessionSummary>>(std::move(summaries));
}

Result<bool> Database::setSessionEnd(int sessionId, Clock::time_point endTime) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE Sessions SET end_date = :end, status = 'in_review' WHERE session_id = :id AND end_date IS NULL;");
    query.bindValue(":end", QString::fromStdString(convertTimeToString(endTime)));
    query.bindValue(":id", sessionId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    // False (not an error) if nothing changed: no such session, or it was
    // already past this step, e.g. End pressed on two devices (header).
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::setSessionPublished(int sessionId, Clock::time_point publishedTime) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE Sessions SET published_at = :pub, status = 'published' WHERE session_id = :id AND status = 'in_review';");
    query.bindValue(":pub", QString::fromStdString(convertTimeToString(publishedTime)));
    query.bindValue(":id", sessionId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    // False (not an error) if nothing changed: no such session, or it was
    // already past this step, e.g. End pressed on two devices (header).
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::renameSession(int sessionId, const std::string& name) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE Sessions SET session_name = :name WHERE session_id = :id;");
    query.bindValue(":name", QString::fromStdString(name));
    query.bindValue(":id", sessionId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::deleteSession(int sessionId) {
    QSqlQuery query(this->db);
    query.prepare("DELETE FROM Sessions WHERE session_id = :id;");
    query.bindValue(":id", sessionId);
    if (!query.exec()) {
        std::cerr << "Database::deleteSession: delete failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::setRecording(int sessionId, const Recording& recording) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE Sessions SET recording_url = :url, recording_offset = :offset, recording_source = :source WHERE session_id = :id;");
    query.bindValue(":url", QString::fromStdString(recording.getViewerUrl()));
    query.bindValue(":offset", static_cast<qlonglong>(recording.getOffsetSeconds()));
    query.bindValue(":source",
                    QString::fromStdString(recording.getSource() == RecordingSource::Manual ? "manual" : "panopto"));
    query.bindValue(":id", sessionId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::clearRecording(int sessionId) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE Sessions SET recording_url = NULL, recording_offset = NULL, recording_source = NULL WHERE session_id = :id;");
    query.bindValue(":id", sessionId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

// ================= Lost pings [F2] =================

Result<LostPing> Database::addLostPing(int sessionId, int userId, Clock::time_point time) {
    QSqlQuery check(this->db);
    check.prepare("SELECT 1 FROM Sessions WHERE session_id = :sid;");
    check.bindValue(":sid", sessionId);
    if (!check.exec() || !check.next()) {
        return Result<LostPing>(Error::NoSuchSession);
    }
    check.prepare("SELECT 1 FROM Users WHERE user_id = :uid;");
    check.bindValue(":uid", userId);
    if (!check.exec() || !check.next()) {
        return Result<LostPing>(Error::NoSuchSession);
    }

    QSqlQuery insert(this->db);
    insert.prepare("INSERT INTO SessionLostPings (session_id, user_id, ping_time) VALUES (:sid, :uid, :time);");
    insert.bindValue(":sid", sessionId);
    insert.bindValue(":uid", userId);
    insert.bindValue(":time", QString::fromStdString(convertTimeToString(time)));
    if (!insert.exec()) {
        std::cerr << "Database::addLostPing: insert failed: "
                  << insert.lastError().text().toStdString() << std::endl;
        return Result<LostPing>(Error::StorageError);
    }
    // Read it back so the author's name (and byEducator) come from the
    // same parser as every other load.
    return getLostPing(insert.lastInsertId().toInt());
}

Result<LostPing> Database::getLostPing(int pingId) {
    QSqlQuery query(this->db);
    query.prepare(QString(kPingColumns) + "WHERE p.ping_id = :id;");
    query.bindValue(":id", pingId);
    if (!query.exec() || !query.next()) {
        return Result<LostPing>(Error::NoSuchSession);
    }
    return Result<LostPing>(parsePing(query));
}

Result<std::vector<LostPing>> Database::listLostPings(int sessionId) {
    std::vector<LostPing> pings;
    QSqlQuery query(this->db);
    query.prepare(QString(kPingColumns) + "WHERE p.session_id = :sid ORDER BY p.ping_time, p.ping_id;");
    query.bindValue(":sid", sessionId);
    if (!query.exec()) {
        return Result<std::vector<LostPing>>(Error::StorageError);
    }
    while (query.next()) {
        pings.push_back(parsePing(query));
    }
    return Result<std::vector<LostPing>>(std::move(pings));
}

Result<std::optional<std::chrono::system_clock::time_point>> Database::getLastLostTime(int sessionId, int userId) {
    QSqlQuery query(this->db);
    query.prepare("SELECT MAX(ping_time) FROM SessionLostPings WHERE session_id = :sid AND user_id = :uid;");
    query.bindValue(":sid", sessionId);
    query.bindValue(":uid", userId);
    if (!query.exec() || !query.next()) {
        return Result<std::optional<Clock::time_point>>(Error::StorageError);
    }
    if (query.value(0).isNull()) {
        return Result<std::optional<Clock::time_point>>(std::nullopt);
    }
    return Result<std::optional<Clock::time_point>>(convertStringToTime(query.value(0).toString().toStdString()));
}

Result<bool> Database::deleteLostPing(int pingId) {
    QSqlQuery query(this->db);
    query.prepare("DELETE FROM SessionLostPings WHERE ping_id = :id;");
    query.bindValue(":id", pingId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

// ================= Questions [F2] =================

Result<SessionQuestion> Database::addQuestion(int sessionId, int userId,
                                        Clock::time_point time, const std::string& text) {
    QSqlQuery check(this->db);
    check.prepare("SELECT 1 FROM Sessions WHERE session_id = :sid;");
    check.bindValue(":sid", sessionId);
    if (!check.exec() || !check.next()) {
        return Result<SessionQuestion>(Error::NoSuchSession);
    }
    check.prepare("SELECT 1 FROM Users WHERE user_id = :uid;");
    check.bindValue(":uid", userId);
    if (!check.exec() || !check.next()) {
        return Result<SessionQuestion>(Error::NoSuchSession);
    }

    QSqlQuery insert(this->db);
    insert.prepare("INSERT INTO SessionQuestions (session_id, user_id, question_time, question_text) VALUES (:sid, :uid, :time, :text);");
    insert.bindValue(":sid", sessionId);
    insert.bindValue(":uid", userId);
    insert.bindValue(":time", QString::fromStdString(convertTimeToString(time)));
    insert.bindValue(":text", QString::fromStdString(text));
    if (!insert.exec()) {
        std::cerr << "Database::addQuestion: insert failed: "
                  << insert.lastError().text().toStdString() << std::endl;
        return Result<SessionQuestion>(Error::StorageError);
    }
    // Read it back so the author's name (and byEducator) come from the
    // same parser as every other load.
    return getQuestion(insert.lastInsertId().toInt());
}

Result<SessionQuestion> Database::getQuestion(int questionId) {
    QSqlQuery query(this->db);
    query.prepare(QString(kQuestionColumns) + "WHERE q.question_id = :id;");
    query.bindValue(":id", questionId);
    if (!query.exec() || !query.next()) {
        return Result<SessionQuestion>(Error::NoSuchSession);
    }
    return Result<SessionQuestion>(parseQuestion(query));
}

Result<std::vector<SessionQuestion>> Database::listQuestions(int sessionId) {
    std::vector<SessionQuestion> questions;
    QSqlQuery query(this->db);
    query.prepare(QString(kQuestionColumns) + "WHERE q.session_id = :sid ORDER BY q.question_time, q.question_id;");
    query.bindValue(":sid", sessionId);
    if (!query.exec()) {
        return Result<std::vector<SessionQuestion>>(Error::StorageError);
    }
    while (query.next()) {
        questions.push_back(parseQuestion(query));
    }
    return Result<std::vector<SessionQuestion>>(std::move(questions));
}

Result<bool> Database::updateQuestionText(int questionId, const std::string& text, Clock::time_point editedTime) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE SessionQuestions SET question_text = :text, edited_time = :edited WHERE question_id = :id;");
    query.bindValue(":text", QString::fromStdString(text));
    query.bindValue(":edited", QString::fromStdString(convertTimeToString(editedTime)));
    query.bindValue(":id", questionId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::deleteQuestion(int questionId) {
    QSqlQuery query(this->db);
    query.prepare("DELETE FROM SessionQuestions WHERE question_id = :id;");
    query.bindValue(":id", questionId);
    if (!query.exec()) {
        std::cerr << "Database::deleteQuestion: delete failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

// ================= Answers [F4] =================

Result<Answer> Database::addAnswer(int questionId, int userId, const std::string& text,
                             Clock::time_point time) {
    QSqlQuery check(this->db);
    check.prepare("SELECT 1 FROM SessionQuestions WHERE question_id = :qid;");
    check.bindValue(":qid", questionId);
    if (!check.exec() || !check.next()) {
        return Result<Answer>(Error::NoSuchSession);
    }
    check.prepare("SELECT 1 FROM Users WHERE user_id = :uid;");
    check.bindValue(":uid", userId);
    if (!check.exec() || !check.next()) {
        return Result<Answer>(Error::NoSuchSession);
    }

    QSqlQuery insert(this->db);
    insert.prepare("INSERT INTO Answers (question_id, user_id, answer_text, answer_time) VALUES (:qid, :uid, :text, :time);");
    insert.bindValue(":qid", questionId);
    insert.bindValue(":uid", userId);
    insert.bindValue(":text", QString::fromStdString(text));
    insert.bindValue(":time", QString::fromStdString(convertTimeToString(time)));
    if (!insert.exec()) {
        std::cerr << "Database::addAnswer: insert failed: "
                  << insert.lastError().text().toStdString() << std::endl;
        return Result<Answer>(Error::StorageError);
    }
    // Read it back so the author's name (and byEducator) come from the
    // same parser as every other load.
    return getAnswer(insert.lastInsertId().toInt());
}

Result<Answer> Database::getAnswer(int answerId) {
    QSqlQuery query(this->db);
    query.prepare(QString(kAnswerColumns) + "WHERE a.answer_id = :id;");
    query.bindValue(":id", answerId);
    if (!query.exec() || !query.next()) {
        return Result<Answer>(Error::NoSuchSession);
    }
    return Result<Answer>(parseAnswer(query));
}

Result<bool> Database::updateAnswerText(int answerId, const std::string& text, Clock::time_point editedTime) {
    QSqlQuery query(this->db);
    query.prepare("UPDATE Answers SET answer_text = :text, edited_time = :edited WHERE answer_id = :id;");
    query.bindValue(":text", QString::fromStdString(text));
    query.bindValue(":edited", QString::fromStdString(convertTimeToString(editedTime)));
    query.bindValue(":id", answerId);
    if (!query.exec()) {
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

Result<bool> Database::deleteAnswer(int answerId) {
    QSqlQuery query(this->db);
    query.prepare("DELETE FROM Answers WHERE answer_id = :id;");
    query.bindValue(":id", answerId);
    if (!query.exec()) {
        std::cerr << "Database::deleteAnswer: delete failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return Result<bool>(Error::StorageError);
    }
    return Result<bool>(query.numRowsAffected() > 0);
}

// ================= Private helpers =================

static SessionStatus statusFromText(const std::string& text) {
    if (text == "live") return SessionStatus::Live;
    if (text == "in_review") return SessionStatus::InReview;
    if (text == "published") return SessionStatus::Published;
    return SessionStatus::Live;
}

Session Database::parseSessionFromQuery(QSqlQuery& query) {
    // Columns: session_id(0), course_id(1), host_id(2), session_name(3),
    //          join_code(4), status(5), start_date(6), end_date(7),
    //          published_at(8), recording_url(9), recording_offset(10), recording_source(11)
    int id = query.value(0).toInt();
    int courseId = query.value(1).toInt();
    int hostId = query.value(2).isNull() ? 0 : query.value(2).toInt();
    std::string name = query.value(3).toString().toStdString();
    std::string joinCode = query.value(4).isNull() ? "" : query.value(4).toString().toStdString();
    std::string statusStr = query.value(5).toString().toStdString();
    Clock::time_point startTime = convertStringToTime(query.value(6).toString().toStdString());

    Session session(id, hostId, startTime);
    session.setCourseId(courseId);
    session.setName(name);
    session.setJoinCode(joinCode);

    if (statusStr == "in_review" || statusStr == "published") {
        Clock::time_point endTime;
        if (!query.value(7).isNull()) {
            endTime = convertStringToTime(query.value(7).toString().toStdString());
        }
        session.endSession(endTime);
    }
    if (statusStr == "published") {
        Clock::time_point publishedTime;
        if (!query.value(8).isNull()) {
            publishedTime = convertStringToTime(query.value(8).toString().toStdString());
        }
        session.publish(publishedTime);
    }

    // Recording
    if (!query.value(9).isNull()) {
        std::string url = query.value(9).toString().toStdString();
        long long offset = query.value(10).isNull() ? 0 : query.value(10).toLongLong();
        RecordingSource source = RecordingSource::Manual;
        if (!query.value(11).isNull() && query.value(11).toString().toStdString() == "panopto") {
            source = RecordingSource::Panopto;
        }
        session.setRecording(Recording(url, offset, source));
    }

    return session;
}

bool Database::createTables() {
    QSqlQuery query(this->db);
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

    status = query.exec(R"sql(
CREATE TABLE IF NOT EXISTS Courses (
    course_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    course_name       TEXT    NOT NULL CHECK (length(course_name) > 0),
    course_join_code  TEXT    NOT NULL UNIQUE,
    is_anonymous      INTEGER NOT NULL DEFAULT 0 CHECK (is_anonymous IN (0, 1)),
    questions_shared  INTEGER NOT NULL DEFAULT 1 CHECK (questions_shared IN (0, 1)),
    educator_id       INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    created_time      TEXT    NOT NULL,
    panopto_folder_id TEXT    CHECK (panopto_folder_id IS NULL OR length(panopto_folder_id) <= 64)
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: Courses table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }

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
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_members_user ON CourseMembers (user_id);")) {
        return false;
    }

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
    recording_source TEXT CHECK (recording_source IN ('manual', 'panopto')),
    CHECK ((status = 'live') = (end_date IS NULL)),
    CHECK ((status = 'published') = (published_at IS NOT NULL)),
    CHECK ((recording_url IS NULL) = (recording_offset IS NULL)),
    CHECK ((recording_url IS NULL) = (recording_source IS NULL))
);)sql");
    if (!status) {
        std::cerr << "Database::createTables: Sessions table failed: "
                  << query.lastError().text().toStdString() << std::endl;
        return false;
    }
    if (!query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_live_code ON Sessions (join_code) WHERE end_date IS NULL;")) {
        return false;
    }
    if (!query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_one_live ON Sessions (course_id) WHERE end_date IS NULL;")) {
        return false;
    }
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_sessions_course ON Sessions (course_id, start_date);")) {
        return false;
    }

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
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_pings_session ON SessionLostPings (session_id, ping_time, ping_id);")) {
        return false;
    }

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
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_questions_session ON SessionQuestions (session_id, question_time, question_id);")) {
        return false;
    }

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
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_answers_question ON Answers (question_id, answer_time, answer_id);")) {
        return false;
    }

    QSqlQuery vq(this->db);
    return vq.exec("PRAGMA user_version = 2;");
}

bool Database::migrateV1ToV2() {
    // Not written: CREATE TABLE IF NOT EXISTS would keep the M2 tables and
    // stamp the file v2, after which every sign-up fails. Until a real
    // migration exists, an M2 file is refused (see open()).
    return false;
}

int Database::detectVersion() {
    QSqlQuery query(this->db);
    int version = 0;
    if (query.exec("PRAGMA user_version;") && query.next()) {
        version = query.value(0).toInt();
    }
    if (version != 0) {
        return version;
    }
    QSqlQuery tables(this->db);
    if (!tables.exec("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%';") ||
        !tables.next()) {
        return 0;  // empty file
    }
    if (!tableExists("Users")) {
        return -1;  // someone else's tables
    }
    // user_version 0 but tables exist: the M2 app made it if Users has the
    // old plain-text user_password column (header).
    QSqlQuery columns(this->db);
    if (columns.exec("PRAGMA table_info(Users);")) {
        while (columns.next()) {
            if (columns.value(1).toString() == "user_password") {
                return 1;
            }
        }
    }
    return -1;  // tables, but neither ours (user_version 2) nor the M2 app's
}

bool Database::tableExists(const QString& name) {
    QSqlQuery query(this->db);
    query.prepare("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = :name;");
    query.bindValue(":name", name);
    return query.exec() && query.next();
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
        // A hand-edited or corrupt row must not crash the server: log it and
        // use the epoch instead.
        std::cerr << "Database: unreadable time \"" << text << "\"" << std::endl;
        return {};
    }
    return std::chrono::system_clock::time_point(
        std::chrono::seconds(parsed.toSecsSinceEpoch()));
}