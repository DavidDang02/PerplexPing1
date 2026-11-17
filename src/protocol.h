// Shared JSON protocol helpers for the PerplexPing client/server setup.
// Converts between the plain C++ models (User, Session, SessionInput) and
// QJsonObject wire formats, and wraps server responses into simple structs.

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <QJsonObject>

#include "datastore.h"
#include "session.h"
#include "sessioninput.h"
#include "user.h"

// A server response envelope:
//   {"id": <request id>, "ok": <bool>, "error": "<string>? , "data": {...}}
struct ProtocolResponse {
    int64_t id = 0;
    bool ok = false;
    std::string error;
    QJsonObject data;
};

// ---- Time helpers: UTC ISO 8601 text <-> JSON strings ----

// Stored as UTC ISO 8601 text, e.g. "2026-10-31T00:53:00Z".
std::string timeToJsonString(std::chrono::system_clock::time_point time);
std::chrono::system_clock::time_point jsonStringToTime(const std::string& text);

// ---- User ----

QJsonObject userToJson(const User& user);
bool userFromJson(const QJsonObject& obj, User& user);

// ---- SessionInput ----

QJsonObject sessionInputToJson(const SessionInput& input);
bool sessionInputFromJson(const QJsonObject& obj, SessionInput& input);

// ---- Session ----

QJsonObject sessionToJson(const Session& session);
bool sessionFromJson(const QJsonObject& obj, Session& session);

// ---- Response envelope ----

bool parseResponse(const QJsonObject& obj, ProtocolResponse& out);
bool makeResponse(int64_t id, bool ok, const std::string& error, const QJsonObject& data, QJsonObject& out);

// ---- Error string <-> domain error ----
std::string authErrorToWire(AuthError error);
std::string authErrorFromWire(const std::string& wire);
