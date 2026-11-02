# PerplexPing
An app that lets an educator to start sessions for their lectures, and students join those sessions.  
  
Students can press a button when they are confused by the lecture, or submit questions.  
  
Both students and educators can see a timeline of the recorded session afterwards, seeing where students were confused, and answer questions that they might have had.  


## MVP
This is the MVP version of the app. There is still no connectivity. Sessions are assumed to be 1 User for each Session.  

Users begin by registering an account, or logging into an existing account. Either way, they must provide a username and a password. Usernames must be unique.  

After logging in, Users can see their own past Sessions, or begin a new Session. When looking at past Sessions, they can see all of their inputs during that Session. Inputs are either a single press of the "I'm Lost" button or a question that they wrote down.  

If the User begins a Session, they can press the "I'm Lost" button when they feel confused during a lecture, or write down a question to record it for later. They can end the Session whenever they like, usually at the end of the lecture that they are in.  

Users can log out. Usernames, passwords, sessions, and the inputs within each session are all stored in a persistent database. For the MVP, there is still no connectivity, and the database will be local to the device. As a result, you can have multiple users sign in with different credentials on the same device, and each of them will only be able to see information from their own past sessions, and not the sessions of others.  

## Pre-requisites
- Qt 6 (Widgets, Sql)
- CMake 3.16+

## Setup Instructions
1. ```git clone https://github.com/SuaceFace/PerplexPing1```
2. ```cd src```
3. ```cmake -B build```
4. ```cmake --build build```

## Run
1. ```./build/app```
2. Press **Start Session** to begin a lecture session.
3. Press **I'm lost** whenever you are confused during the session.
4. Press **End Session** to finish the session.
5. The window shows the session's start time and, once it ends, its end time.
6. After the session ends, the window lists every press, both as time since the start and as the real (wall clock) time.
    - `lostButtonClicked()`: logs a timestamp for each button press while active.
    - `endSession()`: locks the session and sets the end timestamp.
    - `isEnded()`, `getPressCount()`, `getStartTime()`, `getEndTime()`, `getLostButtonTimestamps()`: read-only accessors.
    - `getPressOffsetsSeconds()`: calculates the relative offset in seconds from session start for each press.
