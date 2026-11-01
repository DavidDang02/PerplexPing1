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
		std::cout << "Error:" << this->db.lastError().text().toStdString();
		return false;
	}

	// Create tables
	QSqlQuery query;
	bool status = query.exec("CREATE TABLE IF NOT EXISTS Users (user_id INTEGER PRIMARY KEY AUTOINCREMENT, user_name VARCHAR(255) NOT NULL, user_password VARCHAR(255) NOT NULL);");
	if(!status){
		//qDebug() << "Failed to create Users table: " << query.lastError().text();
		std::cout << "Failed to create Users table: " << query.lastError().text().toStdString();
		return false;
	}

	status = query.exec("CREATE TABLE IF NOT EXISTS Sessions (session_id INTEGER PRIMARY KEY AUTOINCREMENT, host_id INTEGER NOT NULL);");
	if(!status){
		std::cout << "Failed to create Sessions table: " << query.lastError().text().toStdString();
		return false;
	}

	status = query.exec("CREATE TABLE IF NOT EXISTS Sessioninputs (input_id INTEGER PRIMARY KEY AUTOINCREMENT, session_id INTEGER NOT NULL, input_time VARCHAR(255) NOT NULL, input_type VARCHAR(255) NOT NULL, input_question VARCHAR(255) NOT NULL);");
	if(!status){
		std::cout << "Failed to create Sessioninputs table: " << query.lastError().text().toStdString();
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
		std::cout << "Failed to create user in Users table: " << query.lastError().text().toStdString();
		return std::nullopt;
	}

	query.prepare("SELECT user_id FROM Users WHERE user_name = :name;");
	query.bindValue(":name", QString::fromStdString(name));
	status = query.exec();
	if(!status){
		std::cout << "Failed to get user from Users table: " << query.lastError().text().toStdString();
		return std::nullopt;
	}

	if(query.next()) {
		int user_id = query.value(0).toInt();
		return User(user_id, std::move(name));
	} else {
		std::cout << "No vaild matches on user lookup request" << std::endl;
		return std::nullopt;
	}
}

std::optional<User> Database::login(const std::string& name, const std::string& password) {
	QSqlQuery query;
	query.prepare("SELECT user_id, user_name FROM Users WHERE user_name = :name AND user_password = :password;");
	query.bindValue(":name", QString::fromStdString(name));
	query.bindValue(":password", QString::fromStdString(password));
	bool status = query.exec();
	if(!status){
		std::cout << "Database::login SQL query failed: " << query.lastError().text().toStdString();
		return std::nullopt;
	}

	if(query.next()) {
		int user_id = query.value(0).toInt();
		std::string name = query.value(1).toString().toStdString();
		return User(user_id, std::move(name));

	} else {
		std::cout << "Database::login failed to get requested user from Users table: " << query.lastError().text().toStdString();
		return std::nullopt;
	}
}

