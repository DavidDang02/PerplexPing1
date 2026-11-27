// Shared JSON protocol helpers for the PerplexPing client/server setup.

#include "protocol.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

std::string timeToJsonString(std::chrono::system_clock::time_point time) {
  auto secs =
      std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch())
          .count();
  return QDateTime::fromSecsSinceEpoch(secs)
      .toUTC()
      .toString(Qt::ISODate)
      .toStdString();
}

std::chrono::system_clock::time_point jsonStringToTime(const std::string& text) {
  QDateTime parsed = QDateTime::fromString(
      QString::fromStdString(text), Qt::ISODate);
  if (!parsed.isValid()) {
    // Fall back to epoch rather than throwing so deserialization never
    // crashes the app on an unrecognized string.
    return std::chrono::system_clock::time_point(
        std::chrono::seconds(0));
  }
  return std::chrono::system_clock::time_point(
      std::chrono::seconds(parsed.toSecsSinceEpoch()));
}

QJsonObject userToJson(const User& user) {
  QJsonObject obj;
  obj["id"] = user.getId();
  obj["name"] = QString::fromStdString(user.getName());
  return obj;
}

bool userFromJson(const QJsonObject& obj, User& user) {
  int id = obj["id"].toInt(0);
  QString name = obj["name"].toString();
  user = User(id, name.toStdString());
  return true;
}

QJsonObject sessionInputToJson(const SessionInput& input) {
  QJsonObject obj;
  obj["type"] = input.getType() == InputType::Lost ? "Lost" : "Question";
  obj["time"] = QString::fromStdString(
      timeToJsonString(input.getTime()));
  obj["text"] = QString::fromStdString(input.getQuestionText());
  return obj;
}

bool sessionInputFromJson(const QJsonObject& obj, SessionInput& input) {
  std::string type = obj["type"].toString().toStdString();
  if (type == "Lost") {
    input = SessionInput(InputType::Lost,
                         jsonStringToTime(obj["time"].toString().toStdString()));
  } else {
    input = SessionInput(InputType::Question,
                         jsonStringToTime(obj["time"].toString().toStdString()),
                         obj["text"].toString().toStdString());
  }
  return true;
}

QJsonObject sessionToJson(const Session& session) {
  QJsonObject obj;
  obj["id"] = session.getId();
  obj["userId"] = session.getUserId();
  obj["startTime"] =
      QString::fromStdString(timeToJsonString(session.getStartTime()));
  if (session.isEnded()) {
    obj["endTime"] =
        QString::fromStdString(timeToJsonString(session.getEndTime()));
  } else {
    obj["endTime"] = QJsonValue::Null;
  }
  QJsonArray inputs;
  for (const SessionInput& input : session.getInputs()) {
    inputs.append(sessionInputToJson(input));
  }
  obj["inputs"] = inputs;
  return obj;
}

bool sessionFromJson(const QJsonObject& obj, Session& session) {
  int id = obj["id"].toInt(0);
  int userId = obj["userId"].toInt(0);
  session = Session(id, userId,
                    jsonStringToTime(obj["startTime"].toString().toStdString()));

  QJsonArray inputs = obj["inputs"].toArray();
  for (const QJsonValue& val : inputs) {
    SessionInput input;
    sessionInputFromJson(val.toObject(), input);
    session.addInput(input);
  }

  QJsonValue endVal = obj["endTime"];
  if (!endVal.isNull() && !endVal.toString().isEmpty()) {
    session.endSession(jsonStringToTime(endVal.toString().toStdString()));
  }

  return true;
}

bool parseResponse(const QJsonObject& obj, ProtocolResponse& out) {
  out.id = obj["id"].toInt(0);
  out.ok = obj["ok"].toBool(false);
  out.error = obj["error"].toString().toStdString();
  out.data = obj["data"].toObject();
  return true;
}

bool makeResponse(int64_t id, bool ok, const std::string& error,
                  const QJsonObject& data, QJsonObject& out) {
  out["id"] = static_cast<int>(id);
  out["ok"] = ok;
  if (!error.empty()) {
    out["error"] = QString::fromStdString(error);
  }
  out["data"] = data;
  return true;
  }
