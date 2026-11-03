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
	bool status = query.exec("CREATE TABLE IF NOT EXISTS Users (user_id INTEGER PRIMARY KEY, user_name VARCHAR(255) NOT NULL UNIQUE, user_password VARCHAR(255) NOT NULL);");
	if(!status){
		//qDebug() << "Failed to create Users table: " << query.lastError().text();
		std::cout << "Failed to create Users table: " << query.lastError().text().toStdString() << std::endl;
		return false;
	}

	status = query.exec("CREATE TABLE IF NOT EXISTS Sessions (session_id INTEGER PRIMARY KEY, host_id INTEGER NOT NULl, start_date VARCHAR(255) NOT NULL, end_date VARCHAR(255));");
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
	return User(query.lastInsertId().toInt(), std::move(name));
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
		query.bindValue(":inputquestion", "NULL");
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

std::vector<Session> Database::listSessionsForUser(int userId){

	QSqlQuery query;
	std::vector<Session> s;
	query.prepare("SELECT * FROM Sessions LEFT JOIN Sessioninputs USING(session_id) WHERE host_id = :userId ORDER BY session_id;");
	query.bindValue(":userId", userId);
	if(!query.exec()){
		std::cout << "Database::listSessionForUser query failed: " << query.lastError().text().toStdString() << std::endl;
		return s;
	}
	QSqlRecord rec = query.record();
	int session_id_column = rec.indexOf("session_id");
	int host_id_column = rec.indexOf("host_id");
	int user_id_column = rec.indexOf("user_id");
	int start_time_column = rec.indexOf("start_date");
	int end_time_column = rec.indexOf("end_date");
	int input_id_column = rec.indexOf("input_id");
	int input_time_column = rec.indexOf("input_time");
	int input_type_column = rec.indexOf("input_type");
	int input_question_column = rec.indexOf("input_question");
	
	int index = -1;
	int prev_session_id = -1;
	int session_id;
	int host_id;
	std::string start_time;
	std::string end_time;
	int user_id;
	int input_id;
	std::string input_time;
	std::string input_type;
	std::string input_question;
	bool ok;
	bool first = true;
	int temp = 10;
	while(query.next()){
		temp = query.value(session_id_column).toInt(&ok);
		if(ok){
			if (!first){
				prev_session_id = session_id;
				session_id = temp;
			} else {
				session_id = temp;
			}
		} else{
			session_id = -1;
		}

		temp = query.value(host_id_column).toInt(&ok);
		if(ok){
			host_id = temp;
		} else {
			host_id = -1;
		}

		temp = query.value(user_id_column).toInt(&ok);
		if(ok){
			user_id = temp;
		} else  {
			user_id = -1;
		}


		temp = query.value(input_id_column).toInt(&ok);
		if(ok){
			input_id = temp;
		} else {
			input_id = -1;
		}

		start_time = query.value(start_time_column).toString().toStdString();
		input_time = query.value(input_time_column).toString().toStdString();
		input_type = query.value(input_type_column).toString().toStdString();
		input_question = query.value(input_question_column).toString().toStdString();

		if(prev_session_id == session_id){
			if(input_time != ""){
				if(input_type == "Question"){
					SessionInput i(InputType::Question, convertStringToTime(input_time), input_question);
					s[index].addInput(i);
				} else {
				
					SessionInput i(InputType::Lost, convertStringToTime(input_time), input_question);
					s[index].addInput(i);
				}
			}

		} else {
			if(first){
				s.push_back(Session(session_id, host_id, convertStringToTime(start_time)));
				index++;
				first = false;
			} else {
				end_time = query.value(end_time_column).toString().toStdString();
				if(end_time != ""){
					s[index].endSession(convertStringToTime(end_time));
				}
				s.push_back(Session(session_id, host_id, convertStringToTime(start_time)));
				index++;
			}
		}
	} //end while loop
	return s;
} //end function



std::optional<Session> Database::loadSession(int sessionId){
	QSqlQuery query;
	query.prepare("SELECT * FROM Sessions LEFT JOIN Sessioninputs USING(session_id) WHERE session_id = :sessionId ORDER BY input_id;");
	query.bindValue(":sessionId", sessionId);
	Session* session = NULL;
	if(!query.exec()){
		std::cout << "Database::loadSession query failed: " << query.lastError().text().toStdString() << std::endl;
		return std::nullopt;
	}
	QSqlRecord rec = query.record();
	int host_id_column = rec.indexOf("host_id");
	int user_id_column = rec.indexOf("user_id");
	int start_time_column = rec.indexOf("start_date");
	int end_time_column = rec.indexOf("end_date");
	int input_id_column = rec.indexOf("input_id");
	int input_time_column = rec.indexOf("input_time");
	int input_type_column = rec.indexOf("input_type");
	int input_question_column = rec.indexOf("input_question");
	
	int host_id;
	std::string start_time;
	std::string end_time;
	int user_id;
	int input_id;
	std::string input_time;
	std::string input_type;
	std::string input_question;
	bool ok;
	bool first = true;
	int temp = 10;
	while(query.next()){
		temp = query.value(host_id_column).toInt(&ok);
		if(ok){
			host_id = temp;
		} else {
			host_id = -1;
		}

		temp = query.value(user_id_column).toInt(&ok);
		if(ok){
			user_id = temp;
		} else  {
			user_id = -1;
		}

		temp = query.value(input_id_column).toInt(&ok);
		if(ok){
			input_id = temp;
		} else {
			input_id = -1;
		}

		start_time = query.value(start_time_column).toString().toStdString();
		end_time = query.value(end_time_column).toString().toStdString();
		input_time = query.value(input_time_column).toString().toStdString();
		input_type = query.value(input_type_column).toString().toStdString();
		input_question = query.value(input_question_column).toString().toStdString();

		if(first){
			session = new Session(sessionId, host_id, convertStringToTime(start_time));
			first = false;
		}
		if(input_type == "Question"){
			SessionInput i(InputType::Question, convertStringToTime(input_time), input_question);
			session->addInput(i);
		} else {
			SessionInput i(InputType::Lost, convertStringToTime(input_time), input_question);
			session->addInput(i);
		}
	} //end while loop
	if(end_time != ""){
		session->endSession(convertStringToTime(end_time));
	}
	return *session;
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
	std::time_t t = std::mktime(&tm);
	 return std::chrono::system_clock::from_time_t(t);

}
