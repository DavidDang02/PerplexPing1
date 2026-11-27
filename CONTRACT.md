# PerplexPing Final: Contract

The shared interface every layer codes against. If your code matches this file and the headers in `server/`, it will plug into everyone else's. Anything not listed here (private helpers, widget layout, how a page looks) is up to whoever owns that part.

Built on top of master (the M2 prototype). Keep the M2 tests passing.

## 1. Layers and what each owns

| Layer | Owns | Writes these files |
|---|---|---|
| Database | `schema.sql`, `Database` class, migration from the M2 file | `database.cpp`, `tests/test_database.cpp` |
| Model | `User`, `Course`, `Session`, `LostPing`, `SessionQuestion`, `Answer`, `AppModel` (all the rules) | `*.cpp` for those, `tests/test_appmodel.cpp` |
| Server | `PerplexServer`: owns the one SQLite file, one `AppModel` per connection, broadcasts changes | `perplexserver.h/.cpp`, `server_main.cpp` |
| Controller | `ServerClient` (WebSocket in the app), `AppController` (page slot → request, reply/push → getters + signals), `PanoptoClient` / `PanoptoOAuthClient` | `client/*.h/.cpp`, `tests/test_e2e.cpp` |
| View | `MainWindow`, the pages in section 7, `ViewText` | `client/` page files |

Shared by everyone (in `server/`, compiled into both the app and the server; don't change without telling the team): `schema.sql`, `database.h`, `appmodel.h`, `protocol.h`, `error.h`, `validation.h`, and the data classes `user.h`, `course.h`, `session.h`, `lostping.h`, `sessionquestion.h`, `answer.h`, `recording.h`.

## 2. The flow

```
 View page ──signal──▶ AppController ──JSON──▶ ServerClient ══WebSocket══▶ PerplexServer
     ▲                      │                                                  │
     └──show...()───────────┘◀── reply / broadcast ◀══════════════════════════ AppModel ──▶ Database ──▶ SQLite
```

- The app never touches SQLite. Only the server does.
- The server runs one `AppModel` per connected app, so `AppModel` knows who is logged in.
- After any change, the server sends the reply to the sender and a broadcast to everyone else who can see it (same session or course).
- The view never decides rules. It shows what the controller hands it and emits signals. Empty-input checks happen in the model (M2 grading note).
- Display text is built in the view (`viewtext.h` inside the view layer), not the controller (M2 grading note).

## 3. Rules the model enforces

Accounts
- Sign up: username, password, school email. Every account is the same kind (no educator toggle). Passwords are hashed (PBKDF2 with salt). Never store or log the plain password.
- Limits: username 3–20, password 8–64, question 500, answer 1000, course/session names 60. Usernames are case-insensitive.
- Users can change password, change email, delete their account.

Courses
- Anyone can create a course. The creator is that course's one educator (`Courses.educator_id`). The same account can be a student in other courses.
- "Am I the educator here" is worked out each time (`educator_id == me`) and checked in the model, never just hidden in the view, and never saved on the user.
- Stretch (not planned): several educators/TAs per course.
- Each course has a 6-character course code. Students join with it.
- Two course toggles, set by the educator: **anonymous** (server hides names) and **questions shared** (after a session is published, students see everyone's questions, not only their own).

Sessions: `live → in_review → published` (every rule, numbered, is in REQUIREMENTS.md)
- An educator starts a session in their course. It gets a 6-digit session code. One live session per course.
- **Live:** students press Lost and ask questions. A student sees only their own items and can edit or delete them. The educator's live view is read-only (counts and participants). Nobody answers during the session.
- **End:** everything locks for students. Session moves to `in_review`. Students see a placeholder row for it.
- **In review (educator only):** attach the recording (paste a link, or auto-match from Panopto), answer questions, remove questions, then Publish.
- **Published:** students see the timeline and answers. Questions shown follow the course's "questions shared" toggle.
- A session that is never published is fine. No un-publish.
- Answers: many per question, educator answers sort first. Students can answer after publish.
- Educators can rename or delete a session, and delete items only after the session ends.

## 4. Database

Tables (full SQL in `server/schema.sql`, `PRAGMA user_version = 2`):

| Table | Key columns |
|---|---|
| Users | user_id, user_name (unique, nocase), user_email (unique), password_hash, password_salt, password_iterations, created_time |
| Courses | course_id, course_name, course_join_code (unique), is_anonymous, questions_shared, educator_id → Users, panopto_folder_id, created_time |
| CourseMembers (students only) | (course_id, user_id), joined_time |
| Sessions | session_id, course_id, host_id, session_name, join_code, status, start_date, end_date, published_at, recording_url, recording_offset, recording_source |
| SessionLostPings | ping_id, session_id, user_id, ping_time |
| SessionQuestions | question_id, session_id, user_id, question_time, question_text, edited_time |
| Answers | answer_id, question_id, user_id, answer_text, answer_time, edited_time |

Relationships: User 1:N Courses as educator (educator_id). Users M:N Courses as students through CourseMembers. Course 1:N Sessions. Session 1:N SessionLostPings. Session 1:N SessionQuestions. SessionQuestion 1:N Answers. User 1:N LostPings, Questions, Answers, hosted Sessions. (M2's single SessionInputs table was split on Nov 11: a Lost press has no text, edits or answers, so an Answer can only point at a question.)

`Database` methods (signatures in `server/database.h`):
- Users: `createUser`, `getUser`, `findUserByName`, `findUserByEmail`, `getCredentials`, `setPassword`, `setEmail`, `deleteUser`
- Courses: `createCourse`, `getCourse`, `findCourseByJoinCode`, `listCoursesForUser`, `updateCourse`, `deleteCourse`, `addStudent`, `getRole`
- Sessions: `createSession`, `getSession`, `findLiveSessionByJoinCode`, `loadSession`, `listSessionsForCourse`, `setSessionEnd`, `setSessionPublished`, `renameSession`, `deleteSession`, `setRecording`, `clearRecording`
- Lost pings: `addLostPing`, `getLostPing`, `listLostPings`, `getLastLostTime`, `deleteLostPing`
- Questions: `addQuestion`, `getQuestion`, `listQuestions`, `updateQuestionText`, `deleteQuestion`
- Answers: `addAnswer`, `getAnswer`, `updateAnswerText`, `deleteAnswer`

Rules for `database.cpp`:
1. Each `Database` object opens its own connection with a unique connection name.
2. `PRAGMA foreign_keys = ON` on open.
3. Prepared statements with bound values only. Never build SQL by gluing strings.
4. One statement per `exec`. Multi-step writes go in a transaction.
5. Never log password hashes or salts.
6. An M2 file (v1) is refused on open and left as it was (delete or rename it). A v2 file missing a newer column gets it added.
7. **Tests use a fresh temp file per test case** (M2 grading note: shared `test.db` failed when run alone).

## 5. AppModel

Every call returns `Result<T>` (a value or an `Error` from `error.h`). Signatures in `server/appmodel.h`.

| Area | Calls |
|---|---|
| Accounts | `registerUser(name, password, email)`, `logIn`, `logOut`, `changePassword`, `updateEmail`, `deleteAccount` |
| Courses | `createCourse(name, anonymous, questionsShared)`, `joinCourse(code)`, `getCourses`, `getCourse`, `updateCourse`, `getRole`, `isCourseEducator` |
| Sessions | `startSession(courseId, name)`, `joinSession(code)`, `openSession`, `endSession`, `publishSession`, `renameSession`, `deleteSession` |
| Lost pings | `addLost`, `deleteLost` |
| Questions | `addQuestion`, `editQuestion`, `deleteQuestion` |
| Answers | `addAnswer`, `editAnswer`, `deleteAnswer` |
| Recording | `attachRecording(sessionId, url, offset, source)`, `removeRecording`, `matchRecording(sessionId, candidates)` |

Stretch (only if time): `deleteCourse`.

`AppModel` takes a clock in its constructor so tests can fake time.

## 6. Messages (app ⇄ server)

One JSON object per WebSocket message, always with `"type"`. Use the constants in `server/protocol.h`, never a typed string. Ids are integers, times are ms since 1970 UTC (always whole seconds), missing optional fields are left out.

**Replies.** Each request maps one-to-one to an `AppModel` call of the same name. The sender always gets exactly one reply:

```
{"id": <copied from the request, if sent>, "type": "<the request's type>", "ok": true,  "data": {...}}
{"id": ...,                                "type": "<the request's type>", "ok": false, "error": "<Error name>", "code": "<Error name>"}
```

The app matches a reply to its request by `id` (or by `type` if it sends one request at a time). Text that is not one JSON object, or is over `kMaxMessageBytes`, gets `{"type":"error","ok":false,"code":"BadRequest"}`. A missing or wrongly typed field is `BadRequest` before any rule runs.

**Pushes.** Everyone else who may see the change gets a push. Pushes never have `"ok"`, which is how the app tells them from replies. Each recipient's copy goes through `Visibility` for that recipient.

**Session JSON comes in two forms.** Full (`openSession`, `joinSession`, `startSession`): details, recording, the pings and questions this viewer may see, with answers and counts. Details only (end, publish, rename, recording changes, the `sessionEnded` push): no `lostPings`, `questions`, `lostCount` or `questionCount` keys at all, so the app keeps the items it already has.

| Request | `data` in the reply | Push to others |
|---|---|---|
| register, login | user | – |
| logout | – | – |
| changePassword, updateEmail, deleteAccount | – / user / – | deleteAccount: `loggedOut` to the user's other apps |
| listCourses, getCourse | `{courses}` / course + `sessions` (summaries) | – |
| createCourse, joinCourse, updateCourse, deleteCourse | course / course / course / `{courseId}` | `courseChanged` / `courseDeleted` (course members) |
| startSession | full session | `courseChanged` |
| joinSession, openSession | full session | `participants` (educator) |
| closeSession (leave) | – | `participants` |
| endSession | details | `sessionEnded` (room), `courseChanged` |
| publishSession | details | `courseChanged` |
| renameSession, deleteSession | details / `{sessionId, courseId}` | `sessionRenamed` / `sessionDeleted` (course) |
| addLost, deleteLost | ping / `{pingId}` | `lostAdded` / `lostDeleted`, only to whoever may see it (educator gets every ping) |
| addQuestion, editQuestion, deleteQuestion | question / question / `{questionId}` | `questionAdded` / `questionUpdated` / `questionDeleted`, only to whoever may see it |
| addAnswer, editAnswer, deleteAnswer | the whole question with its answers | `answerAdded` / `answerUpdated` / `answerDeleted` (whole question) |
| attachRecording, removeRecording, checkRecording | details (with recording) | – |
| linkPanoptoFolder | course | `courseChanged` |

On connect the server sends `hello` with the protocol version.

## 7. View ⇄ controller (for the pages)

The controller is done: `client/appcontroller.h` has one **slot** per user action, one **getter** per thing a page shows, and the **signals** below. A page never talks to `ServerClient`, the server or Panopto, and never decides a rule. `tests/test_e2e.cpp` drives every slot below against a real server, so it doubles as an example of each flow.

**How every page works**

1. The user does something → the page calls a controller slot (e.g. `controller->askQuestion(box->text())`). Send empty boxes too: the server rejects them and the page shows the error (M2 note: feedback on empty submit).
2. `stateChanged()` → redraw the current page from the getters. Don't keep your own copy of courses/sessions.
3. `requestFailed(request, error)` → show `ViewText::getErrorText(error)`; `request` (e.g. `Protocol::Request::kAddQuestion`) says next to which box.
4. `notice(n)` → show `ViewText::getNoticeText(n)` briefly (MainWindow already does this in the status line).
5. `busyChanged(bool)` → optionally disable buttons while waiting.
6. **Destructive actions must ask first** (rubric): `deleteAccount`, `deleteCourse`, `endSession`, `deleteSession`, `deleteQuestion`, `deleteAnswer`, `deleteLost`, `removeRecording`. Use a `QMessageBox::question` before calling the slot.

`MainWindow` already switches pages on `pageChanged(page)` and shows the "Can't reach the server" banner on `connectionChanged(false)`. Each page below replaces a placeholder in `MainWindow`.

| `Page` | Shows (getters) | Calls (slots) |
|---|---|---|
| `Login` (done: `LoginPage`) | – | `logIn(name, pw)`, `registerUser(name, pw, email)` |
| `Courses` (My Courses) | `getUser()`, `getCourses()` (each `Course`: name, `getRole()`, code if educator) | `openCourse(id)`, `createCourse(name, anonymous, shared)`, `joinCourse(code)`, `joinSession(code)`, `logOut()`, account: `changePassword(old, new)`, `updateEmail(email)`, `deleteAccount(pw)` |
| `Course` | `getOpenCourse()` (course + its toggles, code, `hasPanoptoFolder()`), `getVisibleSessions()` (rows: `ViewText::formatSessionRow`), `getRole()` | `goHome()`, `openSession(id)`, `setSessionSearch(text)`; educator: `startSession(name)`, `renameSession(id, name)`, `deleteSession(id)`, `updateCourse(name, anon, shared)`, `deleteCourse()` |
| `StudentLive` | `getOpenSession()` (status line: `ViewText::formatLiveStatus`; only your own pings and questions) | `pressLost()`, `askQuestion(text)`, `editQuestion(id, text)`, `deleteQuestion(id)`, `deleteLost(id)`, `leaveSession()` |
| `EducatorLive` | `getOpenSession()` (code, counts), `getTimeline()` so far, `getParticipants()` (+ `participantsChanged()`) | `endSession()`, `leaveSession()` |
| `SessionViewer` (review for the educator, published for everyone) | `getOpenSession()`, `getTimeline()` + `Timeline::getPeakIndex(...)` (bars), `getVisibleQuestions()` (each with its answers; authors via `ViewText::formatAuthor`), `getRole()` | `jumpTo(seconds)` (timeline or question click), `setQuestionSearch(text, unansweredOnly)`, `addAnswer(qid, text)`, `editAnswer(id, text)`, `deleteAnswer(id)`, `leaveSession()`; educator: `deleteQuestion(id)`, `publishSession()`, `attachRecording(url, startsAt)`, `removeRecording()`, `linkPanoptoFolder(lectureLink)` (setup card, when `!hasPanoptoFolder()`), `findRecording()` |

**Video:** `playRecording(url)` means "play this Panopto link" (it already starts 10 s before the moment). The player is a `QWebEngineView` loading the full viewer page, as in the Nov 20 spike (`deliverables/Final/video-spike/`). Needs `Qt6::WebEngineWidgets`.

**Setup card for Find recording** (educator, course has no folder yet): step 1 "Sign in to Panopto" happens by itself when they press step 2; step 2 is a box "Paste any lecture link from this course's Panopto" + **Link**, which calls `linkPanoptoFolder(link)`; on `notice(PanoptoLinked)` show the folder as linked. After that the review page just shows **Find recording**. Keep the paste-link + "Session starts at" boxes as the fallback.

## 8. Tests each layer writes (doctest)

- Database: every method, fresh DB per test (`:memory:`), foreign keys cascade on delete, old files.
- Model: each rule in REQUIREMENTS.md by id (`tests/test_model_rules.cpp`, `test_server_appmodel.cpp`).
- Server: every message round-trips `toJson → fromJson`; several fake apps see the right pushes, redacted per recipient (`test_server_live.cpp`).
- Controller: whole user flows against a real server (`tests/test_e2e.cpp`).
- View: `ViewText` has a sentence for every error and notice (`test_viewtext.cpp`); pages are checked by hand (README).

## 9. Decisions made (Nov 10 – Nov 26)

1. Anonymous hides students' names from the educator too.
2. Recordings: auto-match from the course's Panopto folder is the main flow; the educator links the folder once by pasting a lecture link; paste link + offset is the fallback. No timer: the educator presses Find recording.
3. Users edit and delete their own answers; the educator can also delete any answer (moderation).
4. A session code also enrolls you in the course.
5. Deleting the account that created a course keeps the course; its items show "Deleted user".
6. Email must end in `@scu.edu` and is required.
7. Timeline: click a bar or a question to play that moment.
8. I'm Lost presses never show a name, even in a named course (only counts matter).
9. A live session can't be deleted (end it first). An old M2 database file is refused, not migrated.
10. Times in messages are ms since 1970, always whole seconds. Failed replies keep `type`, `ok`, `error` and `code`.
