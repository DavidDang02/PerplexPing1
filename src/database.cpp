#include "database.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

Database::Database(const QString& path){
	this->connectionName = path;
}

Database::~Database() {
	this->db.close();
	this->db = QSqlDatabase();  // destroy the reference first
	QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

bool Database::open() {
	this->db = QSqlDatabase::addDatabase("QSQLITE");

	this->db.setDatabaseName(this->connectionName);

	if (!this->db.open()) {
		std::cout << "Error:" << this->db.lastError().text().toStdString() << std::endl;
		return false;
	}

	// Create tables
	QSqlQuery query;
	bool status = query.exec("CREATE TABLE IF NOT EXISTS Users (user_id INTEGER PRIMARY KEY, user_name VARCHAR(255) NOT NULL UNIQUE, user_password VARCHAR(255) NOT NULL);");
	if(!status){
		//qDebug() << "Failed to create Users table: " << query.lastError().text();
		std::cout << "Failed to create Users table: " << query.lastError().text().toStdString() << std::endl;
		return false;
	}

	status = query.exec("CREATE TABLE IF NOT EXISTS Sessions (session_id INTEGER PRIMARY KEY, host_id INTEGER NOT NULL, start_date VARCHAR(255) NOT NULL, end_date VARCHAR(255));");
	if(!status){
		std::cout << "Failed to create Sessions table: " << query.lastError().text().toStdString() << std::endl;
		return false;
	}

	status = query.exec("CREATE TABLE IF NOT EXISTS Sessioninputs (input_id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL, session_id INTEGER NOT NULL, input_time VARCHAR(255) NOT NULL, input_type VARCHAR(255) NOT NULL, input_question VARCHAR(255));");
	if(!status){
		std::cout << "Failed to create Sessioninputs table: " << query.lastError().text().toStdString() << std::endl;
		return false;
	}

	return true;
}

std::optional<User> Database::registerUser(const std::string& name, const std::string& password){
	if(name.empty() || password.empty()){
		return std::nullopt;
	}
	QSqlQuery query;
	query.prepare("INSERT INTO Users (user_name, user_password) VALUES (:name, :password);");
	query.bindValue(":name", QString::fromStdString(name));
	query.bindValue(":password", QString::fromStdString(password));
	bool status = query.exec();
	if(!status){
		// Fails when the name is already taken (UNIQUE) or the insert errors.
		return std::nullopt;
	}
	return User(query.lastInsertId().toInt(), name);
}

std::optional<User> Database::login(const std::string& name, const std::string& password) {
	QSqlQuery query;
	query.prepare("SELECT user_id, user_name, user_password FROM Users WHERE user_name = :name AND user_password = :password;");
	query.bindValue(":name", QString::fromStdString(name));
	query.bindValue(":password", QString::fromStdString(password));
	bool status = query.exec();
	if(!status){
		std::cout << "Database::login SQL query failed: " << query.lastError().text().toStdString() << std::endl;
		return std::nullopt;
	}

	if(query.next()) {
		return User(query.value(0).toInt(), name);
	}
	return std::nullopt;  // no such user, or the password is wrong
}

Session Database::createSession(int userId) {
	QSqlQuery query;

	Session current_session(userId);
	std::string time = convertTimeToString(current_session.getStartTime());
	
	query.prepare("INSERT INTO Sessions (host_id, start_date) VALUES(:user_id, :startDate);");
	query.bindValue(":user_id", userId);
	query.bindValue(":startDate", QString::fromStdString(time));
	bool status = query.exec();
	if(!status){
		std::cout << "Database::createSession SQL query failed: " << query.lastError().text().toStdString() << std::endl;
		return Session(-1, -1);
	}
		int sessionId = query.lastInsertId().toInt();
		current_session.setId(sessionId);
		return current_session;
}


void Database::saveInput(int sessionId, int userId, SessionInput& input){

	QSqlQuery query;
	query.prepare("INSERT INTO Sessioninputs (session_id, user_id, input_time, input_type, input_question) VALUES(:sessionid, :userid, :inputtime, :inputtype, :inputquestion);");
	query.bindValue(":sessionid", sessionId);
	query.bindValue(":userid", userId);
	
	std::string time = convertTimeToString(input.getTime());

	query.bindValue(":inputtime", QString::fromStdString(time));
	if(input.getType() == InputType::Lost){
		query.bindValue(":inputtype", "Lost");
		query.bindValue(":inputquestion", QVariant());  // a real NULL, not the text "NULL"
	} else {
		query.bindValue(":inputtype", "Question");
		query.bindValue(":inputquestion", QString::fromStdString(input.getQuestionText()));
	}
	
	bool status = query.exec();
	if(!status){
		std::cout << "Database::saveInput Failed to insert input: " << query.lastError().text().toStdString() << std::endl;
	}
		input.setId(query.lastInsertId().toInt());
		return;
}

void Database::saveSessionEnd(int sessionId, Session::Clock::time_point endTime) {

	QSqlQuery query;
	query.prepare("UPDATE Sessions SET end_date = :endDate WHERE session_id = :sessionId;");

	std::string time = convertTimeToString(endTime);

	query.bindValue(":endDate", QString::fromStdString(time));
	query.bindValue(":sessionId", sessionId);
	if(!query.exec()){
		std::cout << "Database::saveSessionEnd could not update end_date column on given session_id: " << query.lastError().text().toStdString() << std::endl;
	}

}

// Every session the user started, newest first, each with its inputs.
std::vector<Session> Database::listSessionsForUser(int userId){
	std::vector<Session> sessions;

	QSqlQuery query;
	query.prepare("SELECT session_id FROM Sessions WHERE host_id = :userId ORDER BY session_id DESC;");
	query.bindValue(":userId", userId);
	if(!query.exec()){
		std::cout << "Database::listSessionsForUser query failed: " << query.lastError().text().toStdString() << std::endl;
		return sessions;
	}

	// Collect the ids first, then load each session with loadSession().
	std::vector<int> ids;
	while(query.next()){
		ids.push_back(query.value(0).toInt());
	}
	for(int id : ids){
		std::optional<Session> session = loadSession(id);
		if(session){
			sessions.push_back(*session);
		}
	}
	return sessions;
} //end function



// Rebuilds one session and its inputs (in the order they were saved).
// nullopt if there is no session with this id.
std::optional<Session> Database::loadSession(int sessionId){
	QSqlQuery query;
	query.prepare("SELECT host_id, start_date, end_date FROM Sessions WHERE session_id = :sessionId;");
	query.bindValue(":sessionId", sessionId);
	if(!query.exec()){
		std::cout << "Database::loadSession query failed: " << query.lastError().text().toStdString() << std::endl;
		return std::nullopt;
	}
	if(!query.next()){
		return std::nullopt;  // no such session
	}
	int host_id = query.value(0).toInt();
	std::string start_time = query.value(1).toString().toStdString();
	std::string end_time = query.value(2).toString().toStdString();  // "" while running

	Session session(sessionId, host_id, convertStringToTime(start_time));

	QSqlQuery inputs;
	inputs.prepare("SELECT input_time, input_type, input_question FROM Sessioninputs WHERE session_id = :sessionId ORDER BY input_id;");
	inputs.bindValue(":sessionId", sessionId);
	if(!inputs.exec()){
		std::cout << "Database::loadSession inputs query failed: " << inputs.lastError().text().toStdString() << std::endl;
		return std::nullopt;
	}
	while(inputs.next()){
		std::string input_time = inputs.value(0).toString().toStdString();
		if(inputs.value(1).toString() == "Question"){
			std::string question = inputs.value(2).toString().toStdString();
			session.addInput(SessionInput(InputType::Question, convertStringToTime(input_time), question));
		} else {
			session.addInput(SessionInput(InputType::Lost, convertStringToTime(input_time)));
		}
	}

	// End it last: an ended session refuses new inputs.
	if(end_time != ""){
		session.endSession(convertStringToTime(end_time));
	}
	return session;
} //end function

std::string Database::convertTimeToString(std::chrono::system_clock::time_point time){

	auto t = std::chrono::system_clock::to_time_t(time);
	std::ostringstream s;
	s << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
	return s.str();

}

std::chrono::system_clock::time_point Database::convertStringToTime(std::string time){

	std::tm tm = {};
	std::istringstream ss(time);
	// Parse string into tm struct
	ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	if (ss.fail()) {
		std::cout << "String that threw the error is: " << time << std::endl;
		throw std::runtime_error("Failed to parse date/time string");
	}
	// Convert to time_t (local time) then to time_point
	tm.tm_isdst = -1;  // let mktime work out daylight saving time, or times shift by an hour
	std::time_t t = std::mktime(&tm);
	 return std::chrono::system_clock::from_time_t(t);

}
