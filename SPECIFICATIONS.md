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
- showError(const QString&) / clearError(): shows or hides the red error message.
- clearFields(): empties both fields and hides the error (used after logging out).
- Signals: loginClicked(), registerClicked() ("Create Account").


View: MainPage (src/mainpage.h, .cpp, .ui)
---
The window after logging in. Shows the user's previous sessions and the details of the selected one. Knows nothing about Session.

- setStatus(const QString&): "Logged in as ana".
- showSessions(const QStringList&): fills the Previous sessions list, one row per session.
- setStartTime / setEndTime(const QString&), showPresses(const QStringList&): fill the Session details panel.
- setRunning(bool): enables or disables Start Session and Log Out.
- Signals: startClicked(), logoutClicked(), sessionSelected(int row).


View: SessionPage (src/sessionpage.h, .cpp, .ui)
---
The window while a session is running: I'm Lost, a question box with Submit, and End Session. Knows nothing about Session.

- setStatus(const QString&): "Session in progress · Lost: 3 · Questions: 1".
- clearQuestion(): empties the question box.
- Signals: lostClicked(), questionSubmitted(const QString&), endClicked().


Controller: AppController (src/appcontroller.h, .cpp)
---
Connects every page signal to a slot, asks AppModel to do the work, then tells the pages what to show and which page is on screen. Has no widgets and never touches the database.

- logIn(), registerUser(): on success go to MainPage. On failure show the reason from AppModel ("Enter a username.", "Enter a password.", "That username is already taken.", "Wrong username or password.").
- logOut(): back to LoginPage with empty fields.
- startSession(), clickLostButton(), submitQuestion(text), endSession(): pass the action to AppModel and refresh SessionPage. Ending goes back to MainPage.
- selectSession(row): shows that session's start time, end time and inputs.
- Formatting: a list row is "Nov 3, 5:53 PM · 12 min · Lost: 4 · Questions: 2". An input is "03:12 in · I'm lost" or "05:40 in · Q: what is a foreign key?". A session that was never ended shows "no end time".
- When the app quits during a session, it ends and saves that session.


Model: AppModel (src/appmodel.h, .cpp)
---
Remembers who is logged in and which session is running, applies the rules, and is the only class that calls Database.

- registerUser(name, password), logIn(name, password): return AuthError::None, EmptyName, EmptyPassword, NameTaken or WrongLogin. The name is trimmed first. Login never says whether the name or the password was wrong.
- logOut(), isLoggedIn(), getCurrentUser().
- startSession(), clickLostButton(), askQuestion(text), endSession(): change the running Session and save the change. Each returns false if it can't happen (logged out, no running session, already ended, empty question).
- getPastSessions(): the logged-in user's sessions, newest first.


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
- registerUser(name, password): nullopt if the name or password is empty, or the name is taken. login(name, password): nullopt if there is no match.
- createSession(userId): inserts a running session and returns it with its new id.
- saveInput(sessionId, userId, input), saveSessionEnd(sessionId, endTime).
- loadSession(sessionId): one session with its inputs in order, or nullopt if it doesn't exist.
- listSessionsForUser(userId): that user's sessions, newest first, each with its inputs.

Tables:
- Users (user_id, user_name UNIQUE, user_password)
- Sessions (session_id, host_id, start_date, end_date NULL while running)
- Sessioninputs (input_id, user_id, session_id, input_time, input_type "Lost" or "Question", input_question NULL for Lost)

Times are stored as "YYYY-MM-DD HH:MM:SS" text in local time. Passwords are stored as plain text (a known limitation of this offline prototype).


Tests (tests/)
---
doctest, built into test_build and run by GitHub Actions on every push: Session rules (start/end, offsets, nothing recorded after end, press and question counts), SessionInput, Database (register/login, sessions, inputs, loading), and AppModel (login and register errors, saving and reloading sessions, each user only sees their own sessions, newest first).
