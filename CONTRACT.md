# PerplexPing Final: Contract

The shared interface every layer codes against. If your code matches this file and the headers in `server/`, it will plug into everyone else's. Anything not listed here (private helpers, widget layout, how a page looks) is up to whoever owns that part.

Built on top of master (the M2 prototype). Keep the M2 tests passing.

## 1. Layers and what each owns

| Layer | Owns | Writes these files |
|---|---|---|
| Database | `schema.sql`, `Database` class, migration from the M2 file | `database.cpp`, `tests/test_database.cpp` |
| Model | `User`, `Course`, `Session`, `LostPing`, `SessionQuestion`, `Answer`, `AppModel` (all the rules) | `*.cpp` for those, `tests/test_appmodel.cpp` |
| Server | `PerplexServer`: owns the one SQLite file, one `AppModel` per connection, broadcasts changes | `perplexserver.h/.cpp`, `server_main.cpp` |
| Controller | `ServerClient` (WebSocket in the app), `AppController` (view signal → request, reply → view), `PanoptoClient` | their `.h/.cpp`, `tests/test_protocol.cpp` |
| View | `MainWindow` and the pages in section 7 | each page's `.h/.cpp` |

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

Sessions: `live → in_review → published`
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
| Courses | course_id, course_name, course_join_code (unique), is_anonymous, questions_shared, educator_id → Users, created_time |
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
6. An M2 file (v1) is backed up to `.v1.bak` and migrated on open.
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
| createCourse, joinCourse, updateCourse, deleteCourse | course / course / course / – | `courseChanged` / `courseDeleted` (course members) |
| startSession | full session | `courseChanged` |
| joinSession, openSession | full session | `participants` (educator) |
| closeSession (leave) | – | `participants` |
| endSession | details | `sessionEnded` (room), `courseChanged` |
| publishSession | details | `courseChanged` |
| renameSession, deleteSession | details | `sessionRenamed` / `sessionDeleted` (course) |
| addLost, deleteLost | ping | `lostAdded` / `lostDeleted`, only to whoever may see it (educator gets every ping) |
| addQuestion, editQuestion, deleteQuestion | question | `questionAdded` / `questionUpdated` / `questionDeleted`, only to whoever may see it |
| addAnswer, editAnswer, deleteAnswer | the whole question with its answers | `answerAdded` / `answerUpdated` / `answerDeleted` (whole question) |
| attachRecording, removeRecording, checkRecording | details (with recording) | – |
| linkPanoptoFolder | course | `courseChanged` |

On connect the server sends `hello` with the protocol version.

## 7. View: pages, signals, display methods

Each page is a `QWidget`. It emits signals and has `show...()` methods. Everything else about it is the page owner's choice. Every page also has `showError(Error)`.

| Page | Signals (out) | Display methods (in) |
|---|---|---|
| LoginPage | loginClicked, registerClicked | showMode, clearFields, showNotice, setBusy |
| NavBar | homeClicked, joinSessionRequested(code), accountSettingsClicked, logoutClicked | showUser, setJoinEnabled, clearJoinCode |
| CoursesPage (My Courses) | courseSelected, openSessionRequested, joinCourseRequested(code), createCourseRequested(name, anonymous, shared) | showCourses |
| CoursePage | backClicked, sessionSelected, startSessionRequested, renameSessionRequested, deleteSessionRequested, courseSettingsChanged | showCourse(CourseDetail) |
| StudentLivePage | lostClicked, questionSubmitted, editQuestionRequested, deleteQuestionRequested, deleteLostRequested, leaveClicked | showSession, showLostSent, clearQuestion |
| EducatorLivePage | endSessionClicked, backClicked | showSession, showParticipants |
| SessionViewerPage (review + published, includes the timeline) | backClicked, momentActivated(seconds), answerSubmitted, editAnswerRequested, deleteAnswerRequested, removeQuestionRequested, attachRecordingRequested, removeRecordingRequested, autoMatchRequested, publishRequested | showSession, selectQuestion, clearAnswerBox, setPanoptoConfigured, setAutoMatchBusy |
| AccountDialog | emailChangeRequested, passwordChangeRequested, deleteAccountRequested | showUser, showNotice, clearPasswordFields |

Every empty-able box still emits its signal when empty; the model rejects it and the page shows the error (M2 grading note: feedback on empty submit). Every button needs a clear label or tooltip.

## 8. Tests each layer writes (doctest)

- Database: every method, fresh DB per test, migration from a v1 file, foreign keys cascade on delete.
- Model: each rule in section 3 (live lock, only the course's educator can run it, questions shared toggle, anonymity hides names, limits, Lost cooldown, publish order).
- Server/Controller: every message round-trips `toJson → fromJson`; two fake clients see the right broadcasts.
- View: none required; manual check list in README.

## 9. Open decisions (settle as a team)

1. Does anonymous hide names from the educator too?
2. Which Panopto field says "processing done"?
3. Can people edit/delete their own answers? (contract assumes yes)
4. Does a session code also enroll you in the course?
5. Deleting the account that created a course keeps the course? (contract: yes, educator_id becomes NULL and the course is read-only)
6. Must email end in @scu.edu?
7. Timeline: click for details, double-click to jump in the video?
8. Are anonymous Lost markers still shown? (assumed yes)
9. In named courses, does the educator see who pressed Lost?

The long reference version (with every view and controller header written out) is in `../Contract/`. It is optional reading, not part of the contract.
