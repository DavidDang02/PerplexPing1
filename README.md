# PerplexPing

Students press **I'm Lost** or type a question during a lecture. Afterwards, each session shows when those happened, so you can see where the lecture lost people.

## Prerequisites

| | macOS | Windows | Linux (Ubuntu) |
|---|---|---|---|
| Qt 6 (Widgets + Sql) | `brew install qt` | [Qt Online Installer](https://www.qt.io/download-qt-installer): pick Qt 6 → **MinGW 64-bit** | `sudo apt install qt6-base-dev libqt6sql6-sqlite` |
| CMake 3.16+ | `brew install cmake` | included with Qt (Tools → CMake) | `sudo apt install cmake` |
| C++17 compiler | Xcode Command Line Tools (`xcode-select --install`) | included with Qt (Tools → MinGW) | `sudo apt install g++` |

The SQLite driver comes with Qt on macOS and Windows.

## Setup and run

**macOS**
```
git clone https://github.com/SuaceFace/PerplexPing1
cd PerplexPing1
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
./build/app
```

**Windows** (easiest: Qt Creator)
1. Open Qt Creator → **File → Open File or Project** → pick `CMakeLists.txt`.
2. Choose the **Desktop Qt 6 MinGW 64-bit** kit → **Configure Project**.
3. Pick the **app** target (bottom left) and press **Run** (green arrow).

Or from the **Qt 6 MinGW** command prompt (Start menu), replacing the Qt version with yours:
```
git clone https://github.com/SuaceFace/PerplexPing1
cd PerplexPing1
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\mingw_64
cmake --build build
build\app.exe
```

**Linux**
```
git clone https://github.com/SuaceFace/PerplexPing1
cd PerplexPing1
cmake -B build && cmake --build build
./build/app
```

## Tests
```
ctest --test-dir build --output-on-failure
```
(or run `./build/test_build`, `build\test_build.exe` on Windows). GitHub Actions runs the same build and tests on every push.

## Using the app
1. Type a username and password and press **Create Account** (or **Log In**).
2. Press **Start Session** at the start of the lecture.
3. Press **I'm Lost** when you're confused, or type a question and press **Send Question**.
4. Press **End Session**. The session appears at the top of **Previous sessions**.
5. Click a previous session to see each input and how far into the session it happened.
6. Press **Log Out** to switch users.

Data is saved in `perplexping.db` next to the app. If the app quits during a session, that session is ended and saved.

## More detail
- What users can do and the rules (limits, case): [REQUIREMENTS.md](REQUIREMENTS.md)
- How the code is organized (MVC, every class): [SPECIFICATIONS.md](SPECIFICATIONS.md)

**MVP limits:** one laptop, no networking (each user only sees their own sessions), and passwords are stored as plain text in the local file.
