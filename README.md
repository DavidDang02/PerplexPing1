# PerplexPing

## Walking Skeleton

The first end-to-end feature runs through every layer (view → controller → model):

- Press **Start Session** to begin a lecture session.
- Press **I'm lost** whenever you are confused during the session.
- Press **End Session** to finish the session.
- The window shows the session's start time and, once it ends, its end time.
- After the session ends, the window lists every press, both as time since the start and as the real (wall clock) time.

## User Requirements

### Mid-Lecture Session

- The professor should be able to start a lecture session.
- The professor should be able to end a lecture session.
- The student should be able to press a button during the session to signal confusion.

### After the Lecture Session

- The professor should be able to see when the button was pressed throughout the lecture.
- The student should be able to see when the button was pressed throughout the lecture.

## Specifications

- Sessions are kept in memory only. Nothing is saved to a database yet, so data disappears when the app closes.
- Each button press is recorded with a real timestamp. Presses are shown as the time since the session started, in `mm:ss in` format followed by the wall clock time (for example, `12:34 in (2:47:05 PM)`).
- The session's start and end times are shown as wall clock times.
- Presses after a session ends are ignored.
- Starting a new session replaces the previous one.

## Classes (MVC)

- **View:** `src/mainpage.ui` (Qt Designer)
  - A status label, start/end time labels, the Start Session / I'm lost / End Session buttons, and a list of press times.
- **Controller:** `MainPage` (`src/mainpage.h`, `src/mainpage.cpp`)
  - Knows: `Session*`
  - `createSession()`: creates a new session (replacing the old one) and stores it in `session`.
  - `clickButton()`: calls `buttonClicked()` on the session.
  - `endSession()`: calls `endSession()` on the session.
  - `refreshView()`: reads the session and updates the widgets.
- **Model:** `Session` (`src/session.h`, `src/session.cpp`)
  - Knows: `buttonTimestamps`, `startTime`, `endTime`
  - `Session()`: constructor; saves the session start time.
  - `endSession()`: saves the session end time; no further button presses are recorded.
  - `buttonClicked()`: records the time the button was pressed.
  - `isEnded()`, `pressCount()`, `pressOffsetsSeconds()`, `getStartTime()`, `getEndTime()`, `getButtonTimestamps()`: read-only getters for the view.

## Setup

Requires Qt 6 (Widgets, Sql) and CMake 3.16+. On macOS: `brew install qt cmake`.

**Qt Creator:** open `CMakeLists.txt`, pick a Desktop kit, then press Run.

**VS Code / terminal:**

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/app
```
