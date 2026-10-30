# PerplexPing M2 Part 1: MVC contract (Oct 30)

This is what everyone codes against. It's built on David's `MVP-View` branch (c514c2f) and the team decision of **one user per session, no joining, no networking**.

`src/user.h`, `src/sessioninput.h` and `src/database.h` are on this branch. The new **`session.h` is in `docs/contract/session.h`**. Whoever writes `session.cpp` moves it into `src/` in the same commit, so the build never breaks.

## Who does what

| Layer | Files | Owner | Status |
|---|---|---|---|
| Model | `user.h/.cpp`, `sessioninput.h/.cpp`, `session.h/.cpp` + tests | Matt / Crawford (split by file) | Headers final; write the .cpp + tests |
| Model (storage) | `database.h/.cpp` + tests on `":memory:"` | Matt / Crawford | Header final; write the .cpp + tests |
| View | `loginpage`, `mainpage`, `sessionpage` | David | Done, plus 3 small additions (below) |
| Controller | `appcontroller.h/.cpp`, `main.cpp` | Rahul | Written; replaces `sessioncontroller.*` |

## The flow (what the app does)

```
LoginPage ──Log In / Create Account──▶ MainPage ──Start Session──▶ SessionPage
    ▲                                    │  ▲                         │
    └──────────── Log Out ───────────────┘  └──────── End Session ────┘
MainPage: list of your past sessions; click one to see its start/end and every input ("03:12 in · I'm lost", "05:40 in · Q: ...")
SessionPage: I'm Lost, question box + Submit, End Session
```

## Model contract

- **User**: just `getId()` and `getName()`. The password never leaves Database.
- **SessionInput**: `InputType::Lost` or `InputType::Question`, a time, and question text (empty for Lost).
- **Session**:
  - `addInput()` returns **false once ended**. That's the rule, and it's the model's job.
  - `lostButtonClicked()` and `askQuestion(text)` are shortcuts. `askQuestion("")` returns false.
  - `endSession()` / `endSession(when)`: ending twice does nothing. The `when` version is for loading from the DB.
  - `getInputs()`, `getPressCount()` (Lost only), `getQuestionCount()`, `getOffsetSeconds(input)`.
  - Kept from M1 so old tests still pass: `Session()`, `getLostButtonTimestamps()` (now returns a copy, Lost only), `getPressOffsetsSeconds()`.
- **Database**: `open`, `registerUser`, `login`, `createSession`, `saveInput`, `saveSessionEnd`, `loadSession`, `listSessionsForUser` (newest first). The exact tables are in the comment at the top of `database.h`.

### Tables (match the team ER, minus joining)
- User(**userID**, name UNIQUE, passwordHash, salt)
- Session(**sessionID**, userID → User, startTime, endTime NULL while running)
- SessionInputs(**inputID**, sessionID → Session, timestamp, isQuestion 0/1, questionText NULL for Lost)

UsersInSession is gone. With one user per session, "who's in it" is just Session.userID. Keep UsersInSession on the ER diagram as the future design for joining.

### Tests the model owners should write (each person their own file)
- Session: an input after end is rejected. `askQuestion("")` is rejected. Counts split Lost vs Question. Offsets are measured from start. `endSession(when)` keeps `when`.
- Database (`":memory:"`): register then login works. Wrong password fails. A duplicate name fails. An empty name or password fails. createSession → saveInput ×N → saveSessionEnd → loadSession gives back the same inputs in order and `isEnded()`. listSessionsForUser is newest first and only that user's sessions.

### Rules for whoever writes database.cpp
- Use prepared statements with `?` and `addBindValue`, never string-building. A name like `O'Brien` would otherwise break the SQL.
- Give each Database its own connection name (`QSqlDatabase::addDatabase("QSQLITE", uniqueName)`), or two Databases in tests will clash.
- Hash = SHA-256 of (salt + password) with `QCryptographicHash`, and a random salt per user (e.g. `QUuid`).
- `loadSession` must add inputs **before** calling `endSession(when)`, because Session rejects inputs once ended.

## View additions for David (3 small things)
1. **LoginPage**: a "Create Account" button (`registerButton`) and a `registerClicked()` signal.
2. **LoginPage**: `showError(const QString&)`, which shows a warning box. It's used for a wrong password or a taken name.
3. **SessionPage**: `setStatus(const QString&)`, which sets the existing `statusLabel`.

MainPage needs nothing new. The controller uses `setStatus`, `setStartTime`, `setEndTime`, `showPresses` (now every input, not just presses), `showSessions`, `setRunning` and the `sessionSelected(int)` signal.
Optional: rename `username()`/`password()` to `getUsername()`/`getPassword()` (the team's get rule). If he does, change the two calls in `appcontroller.cpp`.

