// VIEW: every piece of text the pages show, built from model objects.
// The controller hands the pages Sessions, Users and error codes; the pages
// call these to turn them into words (M2 grading note: formatting display
// text is the view's job, not the controller's).

#pragma once

#include <QString>

#include "error.h"
#include "session.h"
#include "sessioninput.h"

namespace ViewText {

// "Nov 15, 2:15 PM" in the computer's time zone.
QString formatClockTime(Session::Clock::time_point time);

// "50 min", "<1 min", or "no end time" for a session that was never ended
// (the app was killed instead of quit).
QString formatDuration(const Session& session);

// 754 -> "12:34 in"
QString formatOffset(long long seconds);

// "12:34 in · I'm lost" or "12:34 in · Q: what is a foreign key?"
QString formatInput(const Session& session, const SessionInput& input);

// One row of the previous-sessions list:
// "Nov 15, 2:15 PM  ·  50 min  ·  Lost: 3  ·  Questions: 1"
QString formatSessionRow(const Session& session);

// "Session in progress  ·  Lost: 3  ·  Questions: 1"
QString formatLiveStatus(const Session& session);

// The sentence shown for each Error the model gives. "" for None. The one
// place error codes become words (the final project's server sends the same
// codes, so this file keeps working there).
QString getErrorText(Error error);

}  // namespace ViewText
