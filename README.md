# PerplexPing
An app that lets an educator to start sessions for their lectures, and students join those sessions.  
  
Students can press a button when they are confused by the lecture, or submit questions.  
  
Both students and educators can see a timeline of the recorded session afterwards, seeing where students were confused, and answer questions that they might have had.  


## Walking Skeleton
This is the walking skeleton version of the app. Users can create a session, press a button to indicate confusion, and end the session.  

After ending the session, they can then see timestamps of when they pressed the button.  

There is no connectivity to other users, and sessions are stored in memory, deleted when another session is started(or the app is closed).  

## Pre-requisites
- Qt 6 (Widgets, Sql)
- CMake 3.16+

## Setup Instructions
1. ```git clone https://github.com/SuaceFace/PerplexPing1```
2. ```cd src```
3. ```cmake --build build```

## Run
1. ```./build/app```
2. Press **Start Session** to begin a lecture session.
3. Press **I'm lost** whenever you are confused during the session.
4. Press **End Session** to finish the session.
5. The window shows the session's start time and, once it ends, its end time.
6. After the session ends, the window lists every press, both as time since the start and as the real (wall clock) time.


## Classes (MVC)

- **View:** `MainPage` (`src/mainpage.h`, `src/mainpage.cpp`, `src/mainpage.ui`)
  - Displays session state to the user and announces UI interactions via signals. Knows nothing about the `Session` model.
  - **Widgets:** Status label, start/end time labels, "Start Session" / "I'm lost" / "End Session" buttons, and the timestamp list.
  - **Display methods (called by Controller):**
    - `setStatus(const QString&)`: updates the status label text.
    - `setStartTime(const QString&)` / `setEndTime(const QString&)`: updates the session timestamp labels.
    - `setRunning(bool)`: toggles button enabled/disabled states based on session activity.
    - `showPresses(const QStringList&)`: populates the timestamp list widget with formatted press times.
  - **Signals:** `startClicked()`, `lostClicked()`, `endClicked()`.

- **Controller:** `SessionController` (`src/sessioncontroller.h`, `src/sessioncontroller.cpp`)
  - Mediates between the `MainPage` view and the `Session` model. Has no GUI widgets of its own.
  - **Knows:** `MainPage* view` (not owned), `Session* session` (owned).
  - **Slots (handle View signals):**
    - `createSession()`: deletes any existing session, instantiates a new `Session`, and updates the view.
    - `clickButton()`: tells the active `Session` that a confusion ping was triggered.
    - `endSession()`: ends the current `Session`.
  - **Internal helpers:**
    - `refreshView()`: inspects model state, formats elapsed times ("mm:ss in") and wall clock timestamps, and calls the view's display methods.

- **Model:** `Session` (`src/session.h`, `src/session.cpp`)
  - Pure C++ representation of a single lecture session (no Qt dependencies).
  - **Knows:** `startTime`, `endTime`, `buttonTimestamps`, `ended`.
  - **Methods:**
    - `Session()`: records the start time upon creation.
    - `buttonClicked()`: logs a timestamp for each button press while active.
    - `endSession()`: locks the session and sets the end timestamp.
    - `isEnded()`, `getPressCount()`, `getStartTime()`, `getEndTime()`, `getButtonTimestamps()`: read-only accessors.
    - `getPressOffsetsSeconds()`: calculates the relative offset in seconds from session start for each press.