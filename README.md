# PerplexPing
An app that lets an educator to start sessions for their lectures, and students join those sessions.\ 
Students can press a button when they are confused by the lecture, or submit questions.\
Both students and educators can see a timeline of the recorded session afterwards, seeing where students were confused, and answer questions that they might have had.\


## Walking Skeleton
This is the walking skeleton version of the app. Users can create a session, press a button to indicate confusion, and end the session.\
After ending the session, they can then see timestamps of when they pressed the button.\
There is no connectivity to other users, and sessions are stored in memory, deleted when another session is started(or the app is closed).\

## Pre-requisites
- Qt 6 (Widgets, Sql)
- CMake 3.16+

## Setup Instructions
1. ```git clone https://github.com/SuaceFace/PerplexPing1```
2. ```cd src```
3. ```cmake --build build```

## Run
- ```./build/app```
- Press **Start Session** to begin a lecture session.
- Press **I'm lost** whenever you are confused during the session.
- Press **End Session** to finish the session.
- The window shows the session's start time and, once it ends, its end time.
- After the session ends, the window lists every press, both as time since the start and as the real (wall clock) time.


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

