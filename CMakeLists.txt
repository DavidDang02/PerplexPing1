cmake_minimum_required(VERSION 3.16)
project(PerplexPing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6::Sql gives you QSqlDatabase/QSqlQuery, backed by SQLite.
# Qt6::WebSockets gives QWebSocket/QWebSocketServer for the client/server mode.
find_package(Qt6 COMPONENTS Widgets Sql WebSockets Network REQUIRED)
# Optional: plays Panopto recordings inside the app. Without it (e.g. CI,
# which installs only qt6-base) the video player opens the browser instead.
find_package(Qt6 COMPONENTS WebEngineWidgets QUIET)

# ---- Build control options (pass -D<OPTION>=ON|OFF to cmake) ----
option(BUILD_CLIENT "Build the client (app) executable" ON)
option(BUILD_SERVER "Build the server (perplexping_server) executable" ON)
option(BUILD_TESTS  "Build the database/test executables" ON)

# Shared by the app and the server: the data classes, the rules they carry
# (Validation, Filters/Timeline), the error codes and the message format.
# One copy, so the two sides can't drift apart.
set(SHARED_SOURCES
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
    server/filters.cpp
)

# Server only: storage, the model with every permission rule, passwords.
set(SERVER_MODEL_SOURCES
    ${SHARED_SOURCES}
    server/database.cpp
    server/appmodel.cpp
    server/passwordhasher.cpp
    server/permissions.cpp
)

# The app: controller (ServerClient, AppController, Panopto) and view.
# main.cpp is listed separately so the tests can link everything else.
set(APP_SOURCES_NO_MAIN
    ${SHARED_SOURCES}
    client/serverclient.cpp
    client/appcontroller.cpp
    client/panoptoclient.h
    client/panoptooauthclient.cpp
    client/viewtext.cpp
    client/loginpage.cpp
    client/mainwindow.cpp
    client/basepage.cpp
    client/cardlist.cpp
    client/navbar.cpp
    client/timelinewidget.cpp
    client/videoplayer.cpp
    client/accountdialog.cpp
    client/coursespage.cpp
    client/coursepage.cpp
    client/studentlivepage.cpp
    client/educatorlivepage.cpp
    client/sessionviewerpage.cpp
)

# ---- GUI client ----
if(BUILD_CLIENT)
    add_executable(app client/main.cpp ${APP_SOURCES_NO_MAIN})
    target_link_libraries(app PRIVATE Qt6::Widgets Qt6::WebSockets Qt6::Network)
    target_include_directories(app PRIVATE client server)
    if(Qt6WebEngineWidgets_FOUND)
        target_link_libraries(app PRIVATE Qt6::WebEngineWidgets)
        target_compile_definitions(app PRIVATE PERPLEXPING_HAS_WEBENGINE)
    endif()
endif()

# ---- Central server ----
if(BUILD_SERVER)
    add_executable(perplexping_server
        server/server_main.cpp
        server/perplexserver.cpp
        ${SERVER_MODEL_SOURCES}
    )
    target_link_libraries(perplexping_server PRIVATE Qt6::Sql Qt6::WebSockets Qt6::Network)
    target_include_directories(perplexping_server PRIVATE server)
endif()

# ---- Tests ----
if(BUILD_TESTS)
    add_executable(db_tests
        tests/test_database.cpp
        tests/test_main.cpp
        ${SERVER_MODEL_SOURCES}
    )
    target_link_libraries(db_tests PRIVATE Qt6::Sql Qt6::Network)
    target_include_directories(db_tests PRIVATE server)

    # The app's controller and view text, driven against a real server
    # (tests/test_e2e.cpp runs whole user flows: teacher + students).
    add_executable(app_tests
        tests/test_main.cpp
        tests/test_viewtext.cpp
        tests/test_e2e.cpp
        tests/test_panopto.cpp
        ${APP_SOURCES_NO_MAIN}
        server/database.cpp
        server/appmodel.cpp
        server/passwordhasher.cpp
        server/permissions.cpp
        server/perplexserver.cpp
    )
    target_link_libraries(app_tests PRIVATE Qt6::Widgets Qt6::Sql Qt6::WebSockets Qt6::Network)
    target_include_directories(app_tests PRIVATE client server tests)

    add_executable(server_tests
        tests/test_main.cpp
        tests/test_passwordhasher.cpp
        tests/test_server_appmodel.cpp
        tests/test_server_auth.cpp
        tests/test_server_live.cpp
        tests/test_protocol.cpp
        tests/test_model_rules.cpp
        tests/test_classes.cpp
        tests/test_filters.cpp
        tests/test_session_after_end.cpp
        tests/test_session_lostButtonClicked.cpp
        tests/test_session_offsets.cpp
        tests/test_session_start_end.cpp
        tests/test_validation.cpp
        ${SERVER_MODEL_SOURCES}
        server/perplexserver.cpp
    )
    target_link_libraries(server_tests PRIVATE Qt6::Sql Qt6::WebSockets Qt6::Network)
    target_include_directories(server_tests PRIVATE server tests)

    enable_testing()
    add_test(NAME app_tests COMMAND app_tests)
    add_test(NAME db_tests COMMAND db_tests)
    add_test(NAME server_tests COMMAND server_tests)
endif()
