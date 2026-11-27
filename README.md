# PerplexPing

Students press **I'm Lost** or type a question during a lecture. Afterwards, each session shows when those happened, so you can see where the lecture lost people.

## Prerequisites

| | macOS | Windows | Linux (Ubuntu) |
|---|---|---|---|
| Qt 6 (Widgets, Sql, WebSockets) | `brew install qt` | [Qt Online Installer](https://www.qt.io/download-qt-installer): pick Qt 6 → **MinGW 64-bit**, and under **Additional Libraries** tick **Qt WebSockets** (not ticked by default) | `sudo apt install qt6-base-dev qt6-websockets-dev libqt6sql6-sqlite` |
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
- Server: `--port 8081 --db path/to/perplexping.db`, and `--host 0.0.0.0` so laptops on the same network can join.
- App: `--host <server address> --port 8081`.

If the app says it could not connect, the server isn't running (or the host/port differ).

## Tests
```
ctest --test-dir build --output-on-failure
```
(or run `./build/server_tests`, `./build/db_tests` and `./build/test_build`; add `.exe` on Windows). GitHub Actions runs the same build and tests on every push.

## Using the app
1. Type a username and password and press **Create Account** (or **Log In**).
2. Press **Start Session** at the start of the lecture.
3. Press **I'm Lost** when you're confused, or type a question and press **Send Question**.
4. Press **End Session**. The session appears at the top of **Previous sessions**.
5. Click a previous session to see each input and how far into the session it happened.
6. Press **Log Out** to switch users.

Data is saved by the server, in `perplexping.db` next to `perplexing_server` (or the `--db` path). If the server refuses to start because of an old M2 `perplexping.db`, delete or rename that file.

## More detail
- What users can do and the rules (limits, case): [REQUIREMENTS.md](REQUIREMENTS.md)
- How the code is organized (MVC, every class): [SPECIFICATIONS.md](SPECIFICATIONS.md)

Passwords are stored only as salted PBKDF2 hashes on the server; the app never touches the database.
