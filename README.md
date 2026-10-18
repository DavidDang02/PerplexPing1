# MyProject

A Qt desktop app template for CSCI 187 team projects. Each team uses this
repo as their starting point.

## One-time setup

1. Install [VS Code](https://code.visualstudio.com/), [CMake](https://cmake.org/download/),
   and [Qt](https://www.qt.io/download-qt-installer) (Widgets module, version 5).
2. In VS Code, sign in to GitHub using the account icon in the bottom-left
   corner of the window. **Do not create a Personal Access Token** — the
   account icon sign-in flow is all you need, and PATs are unnecessary for
   this course and easy to leak.
3. On GitHub, click **Use this template** on this repository to create your
   team's own copy.
4. Clone your team's new repository and open it in VS Code.

## Building and running locally

These are the exact same three commands CI runs (once it's turned on — see
below), so if they pass locally they'll pass in CI too:

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
