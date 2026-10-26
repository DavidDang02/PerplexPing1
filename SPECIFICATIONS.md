View: MainPage (src/mainpage.h, src/mainpage.cpp, src/mainpage.ui)  
---

Displays session state to the user and announces UI interactions via signals. Knows nothing about the Session model.  


Widgets: Status label, start/end time labels, "Start Session" / "I'm lost" / "End Session" buttons, and the timestamp list.  


Display methods (called by Controller):  
- setStatus(const QString&): updates the status label text.  
- setStartTime(const QString&) / setEndTime(const QString&): updates the session timestamp labels.  
- setRunning(bool): toggles button enabled/disabled states based on session activity.  
- showPresses(const QStringList&): populates the timestamp list widget with formatted press times.  


Signals: startClicked(), lostClicked(), endClicked().  

Controller: SessionController (src/sessioncontroller.h, src/sessioncontroller.cpp)  
---

Mediates between the MainPage view and the Session model. Has no GUI widgets of its own.  


Knows: MainPage* view (not owned), Session* session (owned).  


Slots (handle View signals):
- createSession(): deletes any existing session, instantiates a new Session, and updates the view.
- clickLostButton(): tells the active Session that a confusion ping was triggered.  
- endSession(): ends the current Session.  


Internal helpers:
- refreshView(): inspects model state, formats elapsed times ("mm:ss in") and wall clock timestamps, and calls the view's display methods.
  
Model: Session (src/session.h, src/session.cpp)  
---
Pure C++ representation of a single lecture session (no Qt dependencies).  


Knows: startTime, endTime, buttonTimestamps, ended.  


Methods:
- Session(): records the start time upon creation.
- buttonClicked(): logs a timestamp for each button press while active.
- endSession(): locks the session and sets the end timestamp.
- isEnded(), getPressCount(), getStartTime(), getEndTime(), getLostButtonTimestamps(): read-only accessors.
- getPressOffsetsSeconds(): calculates the relative offset in seconds from session start for each press.
