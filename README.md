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

## Requirements
What a user can do in the MVP (full list in [REQUIREMENTS.md](REQUIREMENTS.md)):
- Create an account with a username and password, and log in
- Start a session, press **I'm Lost**, submit typed questions, and end the session
- See their own previous sessions, newest first, with each one's start time, length, and number of lost presses and questions
- Click a previous session to see every input in it, as time since the start ("03:12 in · I'm lost")
- Log out

Not in the MVP: joining another user's session, separate educator and student roles, and networking.

## Specifications
The app follows MVC. The views (LoginPage, MainPage, SessionPage) only show things and send signals. AppController reacts to those signals and asks the model to do the work. AppModel applies the rules and is the only class that talks to Database, which saves everything in a local SQLite file. Full class-by-class details are in [SPECIFICATIONS.md](SPECIFICATIONS.md).

**Known limitation:** passwords are stored as plain text in the local database. That is acceptable for this offline prototype but would need hashing before real use.

## Pre-requisites
- Qt 6 (Widgets, Sql, and the SQLite driver)
- CMake 3.16+
- A C++17 compiler

## Setup Instructions
1. ```git clone https://github.com/SuaceFace/PerplexPing1```
2. ```cd PerplexPing1```
3. ```cmake -B build```
   (on macOS with Homebrew Qt, you may need ```cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"```)
4. ```cmake --build build```

## Run
1. ```./build/app```
2. Type a username and password and press **Create Account** (or **Log In** if you already have one).
3. Press **Start Session** to begin a lecture session.
4. Press **I'm Lost** whenever you are confused, or type a question and press **Submit**.
5. Press **End Session** to finish. You go back to the main page, where the new session is at the top of **Previous sessions**.
6. Click any previous session to see its start and end times and every input.
7. Press **Log Out** to switch users.

The data is saved in `perplexping.db` next to the app, so it is still there the next time you run it. If the app is closed during a session, that session is ended and saved.

## Tests
```ctest --test-dir build --output-on-failure``` (or run ```./build/test_build```). GitHub Actions runs the same build and tests on every push.
