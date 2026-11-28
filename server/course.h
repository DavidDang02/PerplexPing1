// MODEL: a course (e.g. "CSCI 187"). Has ONE educator (whoever created it),
// its students, its sessions, and the two toggles the educator sets.
// Plain C++ (no Qt).
//
// A Course object is always loaded FOR one user, so it also carries that
// user's role in it. The role is WORKED OUT (educatorId == that user ->
// Educator; a CourseMembers row -> Student), never stored.
//
// Owner: Model layer.

#pragma once

#include <string>
#include <vector>

#include "session.h"

// What a user is in ONE course. Every permission check asks this. Anyone
// can create a course and be its educator, and be a student in someone
// else's course with the same account.
enum class CourseRole {
    None,     // not a member: the course does not exist as far as they can tell (C5)
    Student,  // has a CourseMembers row
    Educator, // Courses.educator_id: created this course
};

class Course {
public:
    Course();  // id 0
    Course(int id, std::string name);

    int getId() const;
    void setId(int id);
    const std::string& getName() const;
    void setName(const std::string& name);

    // 6 characters, e.g. "K7M2QX". Empty when the viewer is not an educator (C8).
    const std::string& getJoinCode() const;
    void setJoinCode(const std::string& code);

    // The two toggles (C7). They apply to EVERY session of the course, live
    // or ended, from the moment they are changed: Visibility reads them from
    // the course each time something is sent. Nothing is stored per session
    // or per input.
    // true = names are hidden (V2). Default false.
    bool isAnonymous() const;
    void setAnonymous(bool anonymous);
    // true = students see each other's questions (V1). Default true.
    bool areQuestionsShared() const;
    void setQuestionsShared(bool shared);

    // The course's one educator (its creator). 0 if that account was deleted.
    int getEducatorId() const;
    void setEducatorId(int userId);
    // When the educator created the course (Courses.created_time).
    Session::Clock::time_point getCreatedTime() const;
    void setCreatedTime(Session::Clock::time_point time);

    // The educator's username, shown to students on the course page.
    // Empty if that account was deleted.
    const std::string& getEducatorName() const;
    void setEducatorName(const std::string& name);
    // True if userId is this course's educator (userId != 0).
    bool isEducator(int userId) const;

    // Ids of the students (CourseMembers rows, in the order they joined) and of the
    // sessions (newest first, like the course page). Reloaded from the database each time; the tables are the
    // source of truth, these lists are a copy.
    const std::vector<int>& getStudentIds() const;
    void setStudentIds(std::vector<int> ids);
    const std::vector<int>& getSessionIds() const;
    void setSessionIds(std::vector<int> ids);

    // The role of the user this object was loaded for (worked out, see top).
    CourseRole getRole() const;
    void setRole(CourseRole role);

    // Id of the course's live session, 0 if none is live (S3: at most one).
    int getLiveSessionId() const;
    void setLiveSessionId(int sessionId);

    // The Panopto folder that holds this course's lecture recordings (its
    // id, a GUID), so "Find recording" knows where to look (R6). Empty until
    // the educator links one.
    const std::string& getPanoptoFolderId() const;
    void setPanoptoFolderId(const std::string& folderId);
    bool hasPanoptoFolder() const;

private:
    int id = 0;
    std::string name;
    std::string joinCode;
    bool anonymous = false;
    bool questionsShared = true;
    int educatorId = 0;
    std::string educatorName;
    Session::Clock::time_point createdTime{};
    std::vector<int> studentIds;
    std::vector<int> sessionIds;
    CourseRole role = CourseRole::None;
    int liveSessionId = 0;
    std::string panoptoFolderId;
};

// A course page: the course and one line per session, newest first.
// Just data.
struct CourseDetail {
    Course course;
    std::vector<SessionSummary> sessions;
};
