# Specifications

How the code is organized. What users can do and every rule (U1, S9, ...) is in [REQUIREMENTS.md](REQUIREMENTS.md); the exact messages and the page interface are in [CONTRACT.md](CONTRACT.md).

## Architecture

PerplexPing is two programs that talk over a WebSocket:

```
 app (one per person)                                   perplexping_server (one per class)
 ┌──────────────────────────────────────────┐           ┌──────────────────────────────────────────┐
 │ View      MainWindow + pages, ViewText    │           │ PerplexServer   sockets, routing, pushes  │
 │   │ slot calls      ▲ signals + getters   │           │   │ one AppModel per connection           │
 │ Controller AppController                  │  JSON     │ AppModel   every rule (Permissions,       │
 │   │                 ▲                     │ ════════▶ │   │        Validation, Visibility)        │
 │ ServerClient (WebSocket)                  │ ◀════════ │ Database   SQLite, no rules               │
 │ PanoptoClient ──HTTPS──▶ SCU Panopto      │ reply/push│                                           │
 └──────────────────────────────────────────┘           └──────────────────────────────────────────┘
        shared by both: data classes, Protocol, Error, Validation, Filters (server/)
```

- **MVC.** The model (data classes + AppModel + Database) runs on the server. The app holds the view and the controller. The app never touches the database.
- **Rules live in one place.** `Permissions` answers yes/no for "may this person do this", `Validation` checks every length and format, `Visibility` removes what a person may not see before anything is sent. `AppModel` calls them in this order: logged in → find the thing → Permissions → Validation → save.
- **The controller holds no rules and builds no text.** It sends a request, and only changes what it shows after the server confirms. All user-facing sentences are in `ViewText` (view layer).
- **Live updates are pushed.** After any change, the server replies to the sender and pushes the change to every other connection that can see it, each copy redacted for its recipient.
- **One copy of the shared classes.** The files in `server/` that both sides need are compiled into both programs (`SHARED_SOURCES` in `CMakeLists.txt`), so the app and the server can't disagree about a session or an error code.

## A request, end to end

A student presses **I'm Lost**:

1. The page calls `AppController::pressLost()`.
2. The controller sends `{"id":12,"type":"addLost","sessionId":7}` through `ServerClient`, and ignores a second press until the reply comes.
3. `PerplexServer` decodes it and calls that connection's `AppModel::addLost(7)`.
4. `AppModel` checks: logged in (U9) → session exists and you're in its course (S6) → `Permissions::canAddInput` (student, live: I1, I2) → 5-second cooldown (I8) → `Database::addLostPing`.
5. The server replies `{"id":12,"type":"addLost","ok":true,"data":{...ping...}}` and pushes `lostAdded` to the educator's apps (students don't see each other's presses, V1).
6. The controller adds the ping to its open session, emits `notice(LostSent)` and `stateChanged()`; the page redraws from `getOpenSession()`.

A failure takes the same path back: `{"ok":false,"code":"LostTooSoon"}` → `requestFailed("addLost", Error::LostTooSoon)` → the page shows `ViewText::getErrorText(...)`.

## Session life

```
 startSession        endSession             publishSession
 ──────────▶  live  ───────────▶ in_review ───────────────▶ published
              students press Lost   educator answers,          everyone sees
              and ask; educator     removes questions,         timeline, Q&A,
              watches (read-only)   attaches the recording     video; answers
```

## Files

```
server/    model, database, server, and the shared classes
client/    the app: controller, ServerClient, Panopto, view
tests/     doctest suites (see Tests)
```

### Shared (compiled into both programs)
| File | What it is |
|---|---|
| `error.h/.cpp` | `Error`: every reason a request can fail, and its network code ("QuestionTooLong"). `Result<T>`: a value or an error. |
| `user.h/.cpp` | `User` (id, name, email; no educator flag), `AuthorState` (shown / hidden / deleted), `Participant`. |
| `course.h/.cpp` | `Course` (name, code, toggles, educator, Panopto folder, the viewer's role), `CourseRole`, `CourseDetail` (a course with its session list). |
| `session.h/.cpp` | `Session` (status, code, times, recording, its Lost pings and questions in time order) and `SessionSummary` (one row of a course's list). Refuses new items after the end. |
| `lostping.h/.cpp`, `sessionquestion.h/.cpp`, `answer.h/.cpp` | One press; one question with its answers; one answer. |
| `recording.h/.cpp` | `Recording`: session seconds → video seconds, the link at a moment (10 s lead-in), `parseTime("4:30")`, and `findOverlapping` for auto-match. |
| `validation.h/.cpp` | Every limit and format check (U1, U3, U6, C2, S2, I3, A3, R2, R3, R6). |
| `protocol.h/.cpp` | Message names and fields, and model ⇄ JSON for every class. A session has a full form (with items) and a details-only form. |
| `filters.h/.cpp` | `Filters` (search sessions and questions: every word, any case, answers included) and `Timeline` (one-minute buckets, the peak minute). |

### Server only
| File | What it is |
|---|---|
| `perplexserver.h/.cpp` | `PerplexServer`: the WebSocket server. One `AppModel` per connection, routes each request, sends one reply, pushes changes to the right connections, sends the participant list to the educator, logs out every device of a deleted account. |
| `appmodel.h/.cpp` | `AppModel`: every action (accounts, courses, sessions, inputs, answers, recordings). Knows who is logged in on its connection. Takes a clock, so tests fake time. |
| `permissions.h/.cpp` | `Permissions` (yes/no per action) and `Visibility` (removes names, codes and other students' items per viewer). |
| `database.h/.cpp`, `schema.sql` | `Database`: SQLite through Qt SQL. Prepared statements only, foreign keys on, a transaction for multi-step writes, adds missing columns on open, refuses an old M2 file. |
| `passwordhasher.h/.cpp` | Salted PBKDF2-HMAC-SHA256. |
| `joincodes.h` | Random course and session codes. |
| `server_main.cpp` | `perplexping_server --host --port --db`. |

### App only
| File | What it is |
|---|---|
| `serverclient.h/.cpp` | `ServerClient`: the one WebSocket. `send(type, fields, onReply)` never blocks; the reply handler runs exactly once (a `ServerUnreachable` reply if there's no connection, it drops, or 10 s pass). Reconnects every 2 s. |
| `appcontroller.h/.cpp` | `AppController`: one slot per user action, one getter per thing a page shows, signals for page changes, redraws, errors, notices and video jumps. Applies pushes, logs back in after a reconnect, filters by the search boxes. |
| `panoptoclient.h` | `PanoptoClient`: what the controller needs from Panopto (sign in, folder of a lecture link, a folder's recordings). An interface, so tests use a fake. |
| `panoptooauthclient.h/.cpp` | `PanoptoOAuthClient`: the real one. OAuth 2 authorization code + PKCE in the browser, a tiny local server catches the redirect, no client secret, token in memory only. Reads `PANOPTO_HOST` and `PANOPTO_CLIENT_ID` from the environment or `.env`. |
| `viewtext.h/.cpp` | `ViewText`: every sentence the pages show (times, durations, offsets, status, roles, authors, notices, one sentence per error). |
| `mainwindow.h/.cpp` | `MainWindow`: switches pages on `pageChanged`, shows the "Can't reach the server" banner and the notice line. |
| `loginpage.h/.cpp/.ui` | `LoginPage`: username, password, email; Log In and Create Account. |
| `main.cpp` | `app --host --port`: builds ServerClient, PanoptoOAuthClient (if set up), AppController and MainWindow. |

## Database

`server/schema.sql`, `PRAGMA user_version = 2`.

| Table | Key columns |
|---|---|
| Users | user_id, user_name (unique, nocase), user_email (unique, nocase), password_hash, password_salt, password_iterations, created_time |
| Courses | course_id, course_name, course_join_code (unique), is_anonymous, questions_shared, educator_id → Users, panopto_folder_id, created_time |
| CourseMembers (students) | (course_id, user_id), joined_time |
| Sessions | session_id, course_id, host_id, session_name, join_code, status, start_date, end_date, published_at, recording_url, recording_offset, recording_source |
| SessionLostPings | ping_id, session_id, user_id, ping_time |
| SessionQuestions | question_id, session_id, user_id, question_time, question_text, edited_time |
| Answers | answer_id, question_id, user_id, answer_text, answer_time, edited_time |

User 1:N Courses (as educator). User M:N Courses (as student, through CourseMembers). Course 1:N Sessions. Session 1:N SessionLostPings and 1:N SessionQuestions. SessionQuestion 1:N Answers. Deleting a course or session deletes everything under it; deleting a user keeps their items with no author ("Deleted user").

Times are stored as UTC ISO 8601 text to the second.

## Panopto (external system)

The recording side runs **in the app**, with the educator's own SCU sign-in, so the server never holds Panopto credentials or tokens (R7).

1. **Sign in** (`PanoptoOAuthClient::signIn`): opens SCU's Panopto sign-in page in the browser with a PKCE challenge; the app's local catcher on `localhost:8080/callback` gets the one-time code and trades it (with the PKCE verifier, no secret) for a token kept in memory.
2. **Link the folder** (`AppController::linkPanoptoFolder`): the educator pastes any lecture link of the course; `GET /Panopto/api/v1/sessions/{id}` gives its folder; the server saves the folder id on the course (`linkPanoptoFolder`). This works with a student-level Panopto account; folder search would need a creator account.
3. **Find recording** (`AppController::findRecording`): `GET /Panopto/api/v1/folders/{id}/sessions` lists recordings; the app sends them to the server (`matchRecording`), which picks the one that overlaps the session (`Recording::findOverlapping`) and saves the link and offset. Nothing runs on a timer; if the recording isn't posted yet the educator gets `RecordingNotReady` and presses again later.
4. **Play a moment** (`AppController::jumpTo`): emits `playRecording(link at that moment)`; the view loads it in its video player.

## Tests

`ctest --test-dir build` runs three programs. GitHub Actions runs them on every push.

| Program | What it covers |
|---|---|
| `db_tests` | Every Database method on a fresh in-memory database per test; foreign-key cascades; adding a missing column to an older file; refusing an M2 file. |
| `server_tests` | Validation limits, every data class and its JSON, Permissions/Visibility, every AppModel rule by id, the time maths, search and timeline, and the real server over sockets with several fake apps (live pushes, redaction per recipient, logout everywhere). |
| `app_tests` | ViewText (a sentence for every error and notice), the Panopto client's offline parts, and **end-to-end flows**: the real AppController + ServerClient against a real server, with an educator and two students (whole lecture to published timeline and video jump, double press, bad input, edit/delete, laptop + phone in sync, deletions moving everyone, anonymity, search, Find recording with a fake Panopto, server down and back, account deleted on another device). |
