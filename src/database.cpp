#include "database.h"

Database::Database(const QString& path){
	this->connectionName = path;
}

Database::~Database() {
	this->db.close();
	this->db = QSqlDatabase();  // destroy the reference first
	QSqlDatabase::removeDatabase(this->connectionName);
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
	bool status = query.exec("CREATE TABLE IF NOT EXISTS Users (user_id INTEGER PRIMARY KEY AUTOINCREMENT, user_name VARCHAR(255) NOT NULL UNIQUE, user_password VARCHAR(255) NOT NULL);");
	if(!status){
		//qDebug() << "Failed to create Users table: " << query.lastError().text();
		std::cout << "Failed to create Users table: " << query.lastError().text().toStdString() << std::endl;
		return false;
	}

	status = query.exec("CREATE TABLE IF NOT EXISTS Sessions (session_id INTEGER PRIMARY KEY AUTOINCREMENT, host_id INTEGER NOT NULl, start_date VARCHAR(255) NOT NULL);");
	if(!status){
		std::cout << "Failed to create Sessions table: " << query.lastError().text().toStdString() << std::endl;
		return false;
	}

	status = query.exec("CREATE TABLE IF NOT EXISTS Sessioninputs (input_id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, session_id INTEGER NOT NULL, input_time VARCHAR(255) NOT NULL, input_type VARCHAR(255) NOT NULL, input_question VARCHAR(255));");
	if(!status){
		std::cout << "Failed to create Sessioninputs table: " << query.lastError().text().toStdString() << std::endl;
		return false;
	}

	return true;
}

std::optional<User> Database::registerUser(const std::string& name, const std::string& password){
	QSqlQuery query;
	query.prepare("INSERT INTO Users (user_name, user_password) VALUES (:name, :password);");
	query.bindValue(":name", QString::fromStdString(name));
	query.bindValue(":password", QString::fromStdString(password));
	bool status = query.exec();
	if(!status){
		std::string error_code = query.lastError().nativeErrorCode().toStdString();
		if(error_code == "2067") {
			return login(name, password);
		} else {
			return std::nullopt;
		}
	}
	return login(name, password);
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
		int db_user_id = query.value(0).toInt();
		std::string db_name = query.value(1).toString().toStdString();
		std::string db_password = query.value(2).toString().toStdString();
		return User(db_user_id, std::move(name));
	} else {
		std::cout << "Database::login failed: lookup did not match any user in database: " << query.lastError().text().toStdString() << std::endl;
		return std::nullopt;
	}
}

Session Database::createSession(int userId) {
	QSqlQuery query;

	Session current_session(userId);
	auto t = std::chrono::system_clock::to_time_t(current_session.getStartTime());
	std::ostringstream s;
	s << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
	std::string time = s.str();
	
	query.prepare("INSERT INTO Sessions (host_id, start_date) VALUES(:user_id, :startDate);");
	query.bindValue(":user_id", userId);
	query.bindValue(":startDate", QString::fromStdString(time));
	bool status = query.exec();
	if(!status){
		std::cout << "Database::createSession SQL query failed: " << query.lastError().text().toStdString() << std::endl;
		return Session(-1, -1);
	}

	query.prepare("SELECT session_id FROM Sessions WHERE host_id = :user_id;");
	query.bindValue(":user_id", userId);
	status = query.exec();
	if(!status){
		std::cout << "Database::createSession SQL query failed: " << query.lastError().text().toStdString() << std::endl;
		return Session(-1, -1);
	}

	if(query.next()){
		int sessionId = query.value(0).toInt();
		current_session.setId(sessionId);
		return current_session;
	} else {
		std::cout << "Database::createSession SQL could not find the created session" << std::endl;
		return Session(-1, -1);
	}
}


void Database::saveInput(int sessionId, SessionInput& input){

	QSqlQuery query;
	query.prepare("INSERT INTO Sessioninputs (session_id, input_time, input_type, input_question) VALUES(:sessionid, :inputtime, :inputtype, :inputquestion);");
	query.bindValue(":sessionid", sessionId);

	auto t = std::chrono::system_clock::to_time_t(input.getTime());
	std::ostringstream s;
	s << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
	std::string time = s.str();

	query.bindValue(":inputtime", QString::fromStdString(time));
	if(input.getType() == InputType::Lost){
		query.bindValue(":inputtype", "Lost");
		query.bindValue(":inputquestion", "NULL");
	} else {
		query.bindValue(":inputtype", "Question");
		query.bindValue(":inputquestion", QString::fromStdString(input.getQuestionText()));
	}
	
	bool status = query.exec();
	if(!status){
		std::cout << "Database::saveInput Failed to insert input: " << query.lastError().text().toStdString() << std::endl;
	}
	query.prepare("SELECT input_id FROM Sessioninputs WHERE session_id = :sessionid;");
	query.bindValue(":sessionid", sessionId);
	if(query.next()){
		input.setId(query.value(0).toInt());
		return;
	}

	std::cout << "Database::saveInput Failed to get input_id from Database" << query.lastError().text().toStdString() << std::endl;
	return;
}

//std::vector<Session> listSessionsForUser(int sessionId){
	//QSqlQuery query;
	//query.exec("SELECT s.session_id, i.input_time

//}

//void Database::saveSessionEnd(int sessionId, Session::Clock::time_point endTime) {

	
//}
