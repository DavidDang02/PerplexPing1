cmake_minimum_required(VERSION 3.16)
project(PerplexPing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6::Sql gives you QSqlDatabase/QSqlQuery, backed by SQLite.
# Qt6::WebSockets gives QWebSocket/QWebSocketServer for the client/server mode.
find_package(Qt6 COMPONENTS Widgets Sql WebSockets REQUIRED)

# Collect all source files in src/ and all test files in tests/.
file(GLOB APP_SOURCES  CONFIGURE_DEPENDS src/*.cpp src/*.ui)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)

# The test binary must not include main.cpp, or you get a
# "duplicate main" link error (tests/test_main.cpp provides main()).
set(APP_SOURCES_NO_MAIN ${APP_SOURCES})
list(REMOVE_ITEM APP_SOURCES_NO_MAIN ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

# ---- GUI client (talks to the central server over WebSocket) ----
add_executable(app ${APP_SOURCES})
target_link_libraries(app PRIVATE Qt6::Widgets Qt6::Sql Qt6::WebSockets)
target_include_directories(app PRIVATE src)

# ---- Central server (owns the SQLite DB, speaks WebSocket) ----
add_executable(perplexing_server
    server/main.cpp
    server/socketserver.cpp
    src/session.cpp
    src/sessioninput.cpp
    src/user.cpp
    src/database.cpp
    src/protocol.cpp
)
target_link_libraries(perplexing_server PRIVATE Qt6::Sql Qt6::WebSockets)
target_include_directories(perplexing_server PRIVATE src server)

# ---- Tests (model + protocol + integration) ----
add_executable(test_build ${TEST_SOURCES} ${APP_SOURCES_NO_MAIN})
target_link_libraries(test_build PRIVATE Qt6::Widgets Qt6::Sql Qt6::WebSockets)
target_include_directories(test_build PRIVATE src tests)

enable_testing()
add_test(NAME test_build COMMAND test_build)