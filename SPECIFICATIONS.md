# Specifications

PerplexPing uses MVC. Each layer only talks to the one next to it:

```
LoginPage / MainPage / SessionPage  --signals-->  AppController  --calls-->  AppModel  --calls-->  Database (SQLite)
          (View)                                   (Controller)              (Model)             (Model: storage)
```

Page flow:

```
LoginPage --Log In / Create Account--> MainPage --Start Session--> SessionPage
    ^                                    |  ^                          |
    +------------- Log Out --------------+  +-------- End Session -----+
```

The app (`src/main.cpp`) opens `perplexping.db` next to the executable, builds the three pages, AppModel and AppController, and starts on the login page.


View: LoginPage (src/loginpage.h, .cpp, .ui)
---
The login window. Collects a username and password. Does not check them itself.

- username(), password(): the typed text.
- showError(Error) / clearError(): shows or hides the red error message. The page picks the sentence with ViewText::getErrorText.
- clearFields(): empties both fields and hides the error (used after logging out).
- Signals: loginClicked(), registerClicked() ("Create Account").


View: MainPage (src/mainpage.h, .cpp, .ui)
---
The window after logging in. Shows the user's previous sessions and the details of the selected one. Gets model objects from the controller and words them with ViewText.

- showUser(const User&): "Logged in as ana".
- showSessions(const std::vector<Session>&): fills the Previous sessions list, one row per session.
- selectSessionRow(int): highlights a row, so the list shows which session the details belong to.
- showSessionDetails(const Session&) / clearSessionDetails(): fill or blank the Session details panel (start, end, every input).
- Signals: startClicked(), logoutClicked(), sessionSelected(int row).


View: ViewText (src/viewtext.h, .cpp)
---
Every piece of text the pages show, built from model objects. Only the view uses it.

- formatClockTime, formatDuration, formatOffset: "Nov 3, 5:53 PM", "12 min" / "<1 min" / "no end time", "03:12 in".
- formatSessionRow(Session): "Nov 3, 5:53 PM · 12 min · Lost: 4 · Questions: 2".
- formatInput(Session, SessionInput): "03:12 in · I'm lost" or "05:40 in · Q: what is a foreign key?".
- formatLiveStatus(Session): "Session in progress · Lost: 3 · Questions: 1".
- getErrorText(Error): the sentence for each error code, e.g. WrongLogin is "Wrong username or password." An unexpected code shows "Something went wrong" with the code's name.


View: SessionPage (src/sessionpage.h, .cpp, .ui)
---
The window while a session is running: I'm Lost, a question box with Send Question, and End Session.

- showSession(const Session&): "Session in progress · Lost: 3 · Questions: 1".
- clearQuestion(): empties the question box and hides any error.
- showQuestionError(Error): red message under the box, e.g. "Type a question before sending." Typing again hides it.
- showQuestionSent(): the empty box's hint says the question was sent.
- Sends whatever was typed; it does not check the text itself (that is the model's rule).
- Signals: lostClicked(), questionSubmitted(const QString&), endClicked().


Controller: AppController (src/appcontroller.h, .cpp)
---
Connects every page signal to a slot, asks AppModel to do the work, then hands the pages model objects to show and picks which page is on screen. Has no widgets, never touches the database, and never builds display text.

- logIn(), registerUser(): on success go to MainPage. On failure pass the Result's Error to LoginPage::showError.
- logOut(): back to LoginPage with empty fields.
- startSession(), clickLostButton(), endSession(): pass the action to AppModel and refresh SessionPage. Ending goes back to MainPage.
- submitQuestion(text): passes the text to AppModel::askQuestion. If it was saved, clears the box; otherwise passes the Result's Error to SessionPage::showQuestionError.
- selectSession(row): shows that session's details.
- When the app quits during a session, it ends and saves that session.


Model: AppModel (src/appmodel.h, .cpp)
---
Remembers who is logged in and which session is running, applies the rules, and is the only class that calls Database. Every action returns a Result: either the value it made, or the Error saying why it didn't happen.

- registerUser(name, password), logIn(name, password): return Result<User>. The name is trimmed first and ignores case. Registering checks the limits in REQUIREMENTS.md with Validation (NameTooShort, NameTooLong, NameBadCharacters, PasswordTooShort, PasswordTooLong), then NameTaken. Login only returns EmptyName, EmptyPassword or WrongLogin, so it never says whether the name or the password was wrong.
- logOut(), isLoggedIn(), getCurrentUser().
- startSession(): Result<Session>. NotLoggedIn, or CourseHasLiveSession if one is already running.
- clickLostButton(), askQuestion(text): Result<SessionInput>, saved with its id. NotLoggedIn, NoSuchSession (none running), and for questions EmptyQuestion or QuestionTooLong (over 500 characters after trimming).
- endSession(): Result<Session>, the ended session. NotLoggedIn or NoSuchSession.
- getPastSessions(): Result<std::vector<Session>>, the logged-in user's sessions, newest first.
- StorageError means the database refused a save; it should never happen.


Model: Error and Result (src/error.h, .cpp)
---
Shared with the rest of the final project contract.

- enum class Error: None plus one code per broken rule (EmptyName, WrongLogin, EmptyQuestion, NoSuchSession, ...).
- Result<T>: holds an error and a value. `return Error::EmptyQuestion;` makes a failed Result, `return input;` makes a successful one. isOk() is true when error is None. No exceptions are thrown anywhere.
- getErrorCode(Error) / getErrorFromCode(text): the code's name as text ("EMPTY_QUESTION"), for messages and later for the server.


Model: Validation (src/validation.h, .cpp)
---
Pure functions with no state, so the rules are written once and tested on their own.

- trim(text), getLength(text) (counts characters, not bytes), toUpper(text).
- checkUsername(name): EmptyName, NameTooShort, NameTooLong or NameBadCharacters, in that order. checkPassword(password): EmptyPassword, PasswordTooShort, PasswordTooLong (never trimmed).
- checkQuestion(text): EmptyQuestion or QuestionTooLong. Also checkEmail, checkCourseName, checkSessionName, checkAnswer, checkRecordingUrl, checkRecordingOffset for the final project.
- Each returns Error::None when the input is fine.


Model: Session (src/session.h, .cpp)
---
One lecture session: who started it, when it started and ended, and every input in order. Plain C++ (no Qt).

- endSession() / endSession(time): sets the end time once. Ending again does nothing.
- addInput(input), lostButtonClicked(), askQuestion(text): record an input. They return false after the session has ended, and askQuestion also returns false for empty text.
- getInputs(), getPressCount(), getQuestionCount(), isEnded(), getStartTime(), getEndTime(), getId(), getUserId().
- getOffsetSeconds(input), getPressOffsetsSeconds(), getLostButtonTimestamps(): times relative to the start, for "mm:ss in".


Model: SessionInput (src/sessioninput.h, .cpp)
---
One "I'm lost" press or one question. InputType::Lost or InputType::Question, the time, and the question text (empty for Lost).


Model: User (src/user.h, .cpp)
---
An id and a name. The password stays in Database.


Model (storage): Database (src/database.h, .cpp)
---
Saves and loads everything in a SQLite file with Qt SQL. Translates between table rows and model objects. The rules live in the model classes, not here.

- open(): opens the file and creates the tables if needed.
- registerUser(name, password): nullopt if the name or password is empty, or the name is taken ignoring case. login(name, password): nullopt if there is no match; the name ignores case and the returned User has the name as registered.
- createSession(userId): inserts a running session and returns it with its new id.
- saveInput(sessionId, userId, input), saveSessionEnd(sessionId, endTime).
- loadSession(sessionId): one session with its inputs in order, or nullopt if it doesn't exist.
- listSessionsForUser(userId): that user's sessions, newest first, each with its inputs.

Tables:
- Users (user_id, user_name UNIQUE, user_password)
- Sessions (session_id, host_id, start_date, end_date NULL while running)
- Sessioninputs (input_id, user_id, session_id, input_time, input_type "Lost" or "Question", input_question NULL for Lost)

Times are stored in UTC as ISO 8601 text, for example "2026-10-31T00:53:00Z", and shown in local time. UTC has no daylight saving, so a time loads back exactly as it was saved. Passwords are stored as plain text (a known limitation of this offline prototype).


Tests (tests/)
---
doctest, built into test_build and run by GitHub Actions on every push: Session rules (start/end, offsets, nothing recorded after end, press and question counts), SessionInput, Database (register/login, sessions, inputs, loading), Validation (each rule and its edges), and AppModel (every action fails with NotLoggedIn when logged out, login and register errors, username/password limits and case, question rules, saving and reloading sessions, each user only sees their own sessions newest first, one running session at a time).

Every Database and AppModel test opens its own empty in-memory database (":memory:"), so each test passes on its own and in any order, e.g. `./build/test_build -tc="Check Save Input"`.
