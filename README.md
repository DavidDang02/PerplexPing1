# PerplexPing

Students press **I'm Lost** or ask a question during a lecture, without raising a hand. The instructor sees, live and afterwards on a timeline, where the lecture lost people, answers the questions, and lines the timeline up with the Panopto recording: clicking a spike plays that moment of the lecture.

## Prerequisites

| | macOS | Windows | Linux (Ubuntu) |
|---|---|---|---|
| Qt 6 (Widgets, Sql, WebSockets, Network) | `brew install qt` | [Qt Online Installer](https://www.qt.io/download-qt-installer): pick Qt 6 → **MinGW 64-bit**, and under **Additional Libraries** tick **Qt WebSockets** (not ticked by default) | `sudo apt install qt6-base-dev qt6-websockets-dev libqt6sql6-sqlite` |
| CMake 3.16+ | `brew install cmake` | included with Qt (Tools → CMake) | `sudo apt install cmake` |
| C++17 compiler | Xcode Command Line Tools (`xcode-select --install`) | included with Qt (Tools → MinGW) | `sudo apt install g++` |

The SQLite driver comes with Qt on macOS and Windows.

PerplexPing has two programs: **`perplexing_server`** (owns the database, one per class) and **`app`** (one per person). Start the server first, then any number of apps.

## Setup and run

**macOS**
```
git clone https://github.com/SuaceFace/PerplexPing1
cd PerplexPing1
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
./build/perplexing_server        # terminal 1: leave it running
./build/app                      # terminal 2 (and 3, 4... for more users)
```

**Windows** (easiest: Qt Creator)
1. Open Qt Creator → **File → Open File or Project** → pick `CMakeLists.txt`.
2. Choose the **Desktop Qt 6 MinGW 64-bit** kit → **Configure Project**.
3. Pick the **perplexing_server** target (bottom left) and press **Run** (green arrow). Leave it running.
4. Pick the **app** target and press **Run** again. Run it more than once for more users.

Or from the **Qt 6 MinGW** command prompt (Start menu), replacing the Qt version with yours:
```
git clone https://github.com/SuaceFace/PerplexPing1
cd PerplexPing1
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\mingw_64
cmake --build build
start build\perplexing_server.exe
build\app.exe
```

**Linux**
```
git clone https://github.com/SuaceFace/PerplexPing1
cd PerplexPing1
cmake -B build && cmake --build build
./build/perplexing_server &
./build/app
```

**Options.** Both programs default to `localhost` port 8081.
- Server: `--port 8081 --db path/to/perplexping.db`, and `--host 0.0.0.0` so laptops and phones on the same network can join.
- App: `--host <server address> --port 8081`.

If the app shows **Can't reach the server**, the server isn't running (or the host/port differ). The app keeps trying every 2 seconds and logs you back in by itself when it's back.

## Panopto (optional, educators)

Without this the app works fully; only **Find recording** says Panopto isn't set up, and pasting a recording link still works.

1. You need the client id of a Panopto API client (made in Panopto under your user settings → API Clients) whose redirect URL is `http://localhost:8080/callback`.
2. Create a file named `.env` in the folder you start the app from (it is git-ignored, never commit it):
   ```
   PANOPTO_HOST=santaclarauniversity.hosted.panopto.com
   PANOPTO_CLIENT_ID=<your client id>
   ```
   (or set the same names as environment variables). The app reads only these two (and an optional `PANOPTO_REDIRECT_URL`). It needs **no client secret**: sign-in uses OAuth 2 with PKCE in your browser, so the app never sees your SCU password and the server never sees any Panopto token.
3. In a course, open an ended session → **Link Panopto folder** → sign in → paste any lecture link of the course. From then on, **Find recording** lines up each lecture's recording by itself (Panopto may take a while after class to post it; press again later if it isn't there yet).

## Tests
```
ctest --test-dir build --output-on-failure
```
or run `./build/db_tests`, `./build/server_tests` and `./build/app_tests` (add `.exe` on Windows). On Linux without a screen, set `QT_QPA_PLATFORM=offscreen`. `app_tests` includes end-to-end tests: the real app controller and server, with an educator and two students going through whole lectures. GitHub Actions runs the same build and tests on every push.

## Using the app
**Everyone:** create an account (username, password, SCU email) or log in.

**Educator:**
1. **Create course**; share its 6-character course code with the class.
2. In the course, **Start session**; put the 6-digit session code on the board.
3. Watch the counts and the timeline live; press **End session** at the end.
4. In review: answer questions, remove any you don't want shown, **Find recording** (or paste the link), then **Publish**.

**Student:**
1. **Join course** with the course code (or join the live session with its code).
2. During the lecture press **I'm Lost** or type a question. You can edit or delete your own until the session ends.
3. Once published: open the session, click the timeline or a question to watch that moment, read and add answers. Search questions, or show only unanswered ones.

Data is saved by the server in `perplexping.db` (or the `--db` path). If the server refuses to start because of an old M2 `perplexping.db`, delete or rename that file.

## More detail
- What users can do and the rules (limits, case): [REQUIREMENTS.md](REQUIREMENTS.md)
- How the code is organized (MVC, every class, Panopto, tests): [SPECIFICATIONS.md](SPECIFICATIONS.md)
- Messages between app and server, and the page interface: [CONTRACT.md](CONTRACT.md)

Passwords are stored only as salted PBKDF2 hashes on the server; the app never touches the database. API keys and tokens never go in the repo or to the server.
