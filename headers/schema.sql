-- PerplexPing final schema, version 2 (PRAGMA user_version = 2).
-- Owner: Database layer. This file is the single source of truth for the tables.
-- Database::open() runs exactly these statements on a new file, and
-- migration_v1_to_v2.sql on a file made by the M2 app (current master).
--
-- Conventions (same as master): plural table names, snake_case columns,
-- times are UTC ISO 8601 TEXT to the second, e.g. "2026-11-16T22:15:03Z",
-- booleans are INTEGER 0/1. IDs are AUTOINCREMENT so an id is never reused
-- after a delete (messages refer to rows by id).
--
-- The database guards INTEGRITY (keys, NOT NULL, enums). POLICY (length
-- limits, who may do what, freeze after end) lives in the model, so old rows
-- that break today's policy can still be loaded.
--
-- Every connection must run:  PRAGMA foreign_keys = ON;
-- (SQLite ignores REFERENCES / ON DELETE without it.)

CREATE TABLE Users (
    user_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Unique ignoring case: "Ana" and "ana" are the same account (rule U2).
    user_name           TEXT    NOT NULL UNIQUE COLLATE NOCASE
                                CHECK (length(user_name) > 0),
    -- School email, required when an account is created (rule U6). NULL only
    -- for accounts imported from the M2 database, until they set one in
    -- Account settings. UNIQUE allows many NULLs in SQLite.
    user_email          TEXT    UNIQUE COLLATE NOCASE,
    -- PBKDF2-HMAC-SHA256 of the password, 32 bytes as 64 hex characters.
    password_hash       TEXT    NOT NULL,
    -- 16 random bytes as 32 hex characters, different for every user.
    password_salt       TEXT    NOT NULL,
    -- PBKDF2 rounds used for this row, so the default can be raised later.
    password_iterations INTEGER NOT NULL CHECK (password_iterations > 0),
    created_time        TEXT    NOT NULL
);

CREATE TABLE Courses (
    course_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    course_name       TEXT    NOT NULL CHECK (length(course_name) > 0),
    -- 6 characters from A-Z and 2-9 (no 0/O/1/I). Students type it once to
    -- become members. Never changes (regenerating is stretch).
    course_join_code  TEXT    NOT NULL UNIQUE,
    -- The two toggles educators set (rule C7). They are read from HERE every
    -- time something is sent, for every session of the course, live or ended.
    -- Nothing about anonymity is stored on a session or an input: flipping a
    -- toggle changes what people see from that moment on.
    -- 1 = names are hidden (rule V2).
    is_anonymous      INTEGER NOT NULL DEFAULT 0 CHECK (is_anonymous IN (0, 1)),
    -- 1 = students can see each other's questions (rule V1).
    questions_shared  INTEGER NOT NULL DEFAULT 1 CHECK (questions_shared IN (0, 1)),
    -- The course's one educator: whoever created it (rule C1). Any account
    -- can create a course; there is no "educator account". "Am I the
    -- educator here" = (educator_id = me). NULL if that account was deleted;
    -- the course then stays, read-only.
    educator_id       INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    created_time      TEXT    NOT NULL
);

-- The STUDENTS of each course (joined with the course code). The educator
-- is NOT a row here; they are Courses.educator_id. Replaces the
-- "UsersInSession" idea from the M2 ER diagram: membership is per course.
CREATE TABLE CourseMembers (
    course_id   INTEGER NOT NULL REFERENCES Courses (course_id) ON DELETE CASCADE,
    user_id     INTEGER NOT NULL REFERENCES Users (user_id)     ON DELETE CASCADE,
    joined_time TEXT    NOT NULL,
    PRIMARY KEY (course_id, user_id)
);
CREATE INDEX idx_members_user ON CourseMembers (user_id);
CREATE INDEX idx_courses_educator ON Courses (educator_id);

CREATE TABLE Sessions (
    session_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id        INTEGER NOT NULL REFERENCES Courses (course_id) ON DELETE CASCADE,
    -- The educator who started it. NULL if that account was deleted later.
    host_id          INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    session_name     TEXT    NOT NULL CHECK (length(session_name) > 0),
    -- 6 digits as TEXT (keeps leading zeros). NULL for sessions imported
    -- from the M2 database.
    join_code        TEXT,
    -- The session's life (rule S11): 'live' -> 'in_review' -> 'published'.
    --   live:      students send inputs; set when the session is created
    --   in_review: the educator clicked End; only the course's educators can
    --              open it (attach the recording, moderate, answer)
    --   published: the educator clicked Publish; students can open it
    -- It only ever moves forward.
    status           TEXT    NOT NULL DEFAULT 'live'
                             CHECK (status IN ('live', 'in_review', 'published')),
    start_date       TEXT    NOT NULL,
    -- NULL while the session is live. Set once at End, never cleared.
    -- Inputs are frozen for students from this moment (rule I6).
    end_date         TEXT,
    -- NULL until the educator publishes. Set once, never cleared.
    published_at     TEXT,
    -- The lecture recording this session lines up with. All three are NULL
    -- until one is attached. recording_offset = seconds into the VIDEO at
    -- which the SESSION started (negative if the session started first).
    recording_url    TEXT,
    recording_offset INTEGER,
    recording_source TEXT CHECK (recording_source IN ('manual', 'panopto')),
    CHECK ((status = 'live') = (end_date IS NULL)),
    CHECK ((status = 'published') = (published_at IS NOT NULL)),
    CHECK ((recording_url IS NULL) = (recording_offset IS NULL)),
    CHECK ((recording_url IS NULL) = (recording_source IS NULL))
);
-- A join code only has to be unique among sessions that are live right now.
CREATE UNIQUE INDEX idx_sessions_live_code ON Sessions (join_code) WHERE end_date IS NULL;
-- A course has at most one live session (rule S3).
CREATE UNIQUE INDEX idx_sessions_one_live ON Sessions (course_id) WHERE end_date IS NULL;
CREATE INDEX idx_sessions_course ON Sessions (course_id, start_date);
-- Note: only 'live' sessions have end_date NULL (first CHECK above), so
-- "WHERE end_date IS NULL" in the two indexes means "live".

-- One "I'm Lost" press: who and when, nothing else. (M2 kept presses and
-- questions in one Sessioninputs table; split Nov 11 because a press has no
-- text, no edits and no answers.)
CREATE TABLE SessionLostPings (
    ping_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL REFERENCES Sessions (session_id) ON DELETE CASCADE,
    -- The student who pressed it. ALWAYS stored, even while the course is
    -- anonymous (the server needs it for "delete your own"); the server
    -- strips it before sending when the course setting says so (rule V2).
    -- NULL if that account was deleted later: shown as "Deleted user" (U8).
    user_id    INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    -- Stamped by the server's clock, never by the app that sent it.
    ping_time  TEXT    NOT NULL
);
CREATE INDEX idx_pings_session ON SessionLostPings (session_id, ping_time, ping_id);

-- One question a student typed during the session.
CREATE TABLE SessionQuestions (
    question_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id    INTEGER NOT NULL REFERENCES Sessions (session_id) ON DELETE CASCADE,
    -- Same rules as SessionLostPings.user_id.
    user_id       INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    question_time TEXT    NOT NULL,
    question_text TEXT    NOT NULL CHECK (length(question_text) > 0),
    -- When the student last edited the text; NULL if never.
    edited_time   TEXT
);
CREATE INDEX idx_questions_session ON SessionQuestions (session_id, question_time, question_id);

-- Many answers per question, written after the session has ended: by
-- educators during review, by everyone once it is published (rule A1).
-- Whether an answer is "by an educator" (pinned on top, rule A5) is NOT
-- stored: it is worked out when the answer is loaded
-- (Answers.user_id = the course's Courses.educator_id).
CREATE TABLE Answers (
    answer_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    question_id INTEGER NOT NULL REFERENCES SessionQuestions (question_id) ON DELETE CASCADE,
    -- NULL if that account was deleted later ("Deleted user").
    user_id     INTEGER REFERENCES Users (user_id) ON DELETE SET NULL,
    answer_text TEXT    NOT NULL CHECK (length(answer_text) > 0),
    answer_time TEXT    NOT NULL,
    -- When the author last edited the text; NULL if never (rule A8).
    edited_time TEXT
);
CREATE INDEX idx_answers_question ON Answers (question_id, answer_time, answer_id);

PRAGMA user_version = 2;
