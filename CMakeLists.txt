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

# ---- Build control options (pass -D<OPTION>=ON|OFF to cmake) ----
option(BUILD_CLIENT "Build the client (app) executable" ON)
option(BUILD_SERVER "Build the server (perplexing_server) executable" ON)
option(BUILD_TESTS  "Build the database/test executables" ON)

# Collect source files for client app.
file(GLOB APP_SOURCES CONFIGURE_DEPENDS client/*.cpp client/*.ui)

# The client app builds against the OLD client code. The database (Result<T>)
# implementation is part of the NEW server-side layer; strip it from the client
# source list so the client never links the new code.
list(REMOVE_ITEM APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/client/database.cpp)

# The test binary must not include main.cpp, or you get a
# "duplicate main" link error (tests/test_main.cpp provides main()).
set(APP_SOURCES_NO_MAIN ${APP_SOURCES})
list(REMOVE_ITEM APP_SOURCES_NO_MAIN ${CMAKE_CURRENT_SOURCE_DIR}/client/main.cpp)

# Server model sources
set(SERVER_MODEL_SOURCES
    server/database.cpp
    server/appmodel.cpp
    server/passwordhasher.cpp
    server/permissions.cpp
    server/error.cpp
    server/course.cpp
    server/session.cpp
    server/lostping.cpp
    server/sessionquestion.cpp
    server/answer.cpp
    server/recording.cpp
    server/validation.cpp
    server/user.cpp
    server/protocol.cpp
)

# ---- GUI client ----
if(BUILD_CLIENT)
    add_executable(app ${APP_SOURCES})
    target_link_libraries(app PRIVATE Qt6::Widgets Qt6::Sql Qt6::WebSockets)
    target_include_directories(app PRIVATE client)
endif()

# ---- Central server ----
if(BUILD_SERVER)
    add_executable(perplexing_server
        server/server_main.cpp
        server/perplexserver.cpp
        ${SERVER_MODEL_SOURCES}
    )
    target_link_libraries(perplexing_server PRIVATE Qt6::Sql Qt6::WebSockets)
    target_include_directories(perplexing_server PRIVATE server)
endif()

# ---- Tests ----
if(BUILD_TESTS)
    add_executable(db_tests
        tests/test_database.cpp
        tests/test_main.cpp
        ${SERVER_MODEL_SOURCES}
    )
    target_link_libraries(db_tests PRIVATE Qt6::Sql)
    target_include_directories(db_tests PRIVATE server)

    add_executable(test_build
        tests/test_main.cpp
        tests/test_appmodel.cpp
        tests/test_session_contract.cpp
        tests/test_session_after_end.cpp
        tests/test_session_lostButtonClicked.cpp
        tests/test_session_offsets.cpp
        tests/test_session_start_end.cpp
        tests/test_sessioninput.cpp
        tests/test_validation.cpp
        ${APP_SOURCES_NO_MAIN}
        client/database.cpp
    )
    target_link_libraries(test_build PRIVATE Qt6::Widgets Qt6::Sql Qt6::WebSockets)
    target_include_directories(test_build PRIVATE client tests)

    add_executable(server_tests
        tests/test_main.cpp
        tests/test_passwordhasher.cpp
        tests/test_server_appmodel.cpp
        tests/test_server_auth.cpp
        tests/test_server_live.cpp
        tests/test_validation.cpp
        ${SERVER_MODEL_SOURCES}
        server/perplexserver.cpp
    )
    target_link_libraries(server_tests PRIVATE Qt6::Sql Qt6::WebSockets)
    target_include_directories(server_tests PRIVATE server tests)

    enable_testing()
    add_test(NAME test_build COMMAND test_build)
    add_test(NAME db_tests COMMAND db_tests)
    add_test(NAME server_tests COMMAND server_tests)
endif()