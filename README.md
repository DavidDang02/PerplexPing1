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
    - `buttonClicked()`: logs a timestamp for each button press while active.
    - `endSession()`: locks the session and sets the end timestamp.
    - `isEnded()`, `getPressCount()`, `getStartTime()`, `getEndTime()`, `getButtonTimestamps()`: read-only accessors.
    - `getPressOffsetsSeconds()`: calculates the relative offset in seconds from session start for each press.
