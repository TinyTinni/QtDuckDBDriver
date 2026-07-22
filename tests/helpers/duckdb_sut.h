#pragma once

#include "test_database.h"
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlQuery>
#include <memory>

class DuckDBSut {
public:
	DuckDBSut() : m_db(std::make_unique<TestDatabase>()) {}

	bool createTable(const QString &name, const QString &colType = "INTEGER") {
		auto q = m_db->exec("CREATE TABLE " + name + " (v " + colType + ")");
		return !q.lastError().isValid();
	}

	bool dropTable(const QString &name) {
		auto q = m_db->exec("DROP TABLE IF EXISTS " + name);
		return !q.lastError().isValid();
	}

	bool tableExists(const QString &name) {
		return m_db->db().tables().contains(name);
	}

	bool insertValue(const QString &table, int value) {
		QSqlQuery q(m_db->db());
		if (!q.prepare("INSERT INTO " + table + " VALUES (?)"))
			return false;
		q.bindValue(0, value);
		return q.exec();
	}

	bool deleteAll(const QString &table) {
		QSqlQuery q(m_db->db());
		return q.exec("DELETE FROM " + table);
	}

	int numRowsAffected(const QString &table, int value) {
		QSqlQuery q(m_db->db());
		if (!q.prepare("DELETE FROM " + table + " WHERE v = ?"))
			return -1;
		q.bindValue(0, value);
		q.exec();
		return q.numRowsAffected();
	}

	int queryCount(const QString &table) {
		QSqlQuery q(m_db->db());
		if (!q.prepare("SELECT COUNT(*) FROM " + table))
			return -1;
		if (!q.exec() || !q.next())
			return -1;
		return q.value(0).toInt();
	}

	bool queryItemExists(const QString &table, int value) {
		QSqlQuery q(m_db->db());
		if (!q.prepare("SELECT COUNT(*) FROM " + table + " WHERE v = ?"))
			return false;
		q.bindValue(0, value);
		if (!q.exec() || !q.next())
			return false;
		return q.value(0).toInt() > 0;
	}

	// Transaction operations
	bool beginTransaction() { return m_db->db().driver()->beginTransaction(); }
	bool commitTransaction() { return m_db->db().driver()->commitTransaction(); }
	bool rollbackTransaction() { return m_db->db().driver()->rollbackTransaction(); }

	// Connection lifecycle
	bool isOpen() const { return m_db->isOpen(); }
	void close() { m_db->close(); }
	bool reopen() { return m_db->open(); }

	// Prepared statement lifecycle
	bool prepareInsert(const QString &table) {
		m_preparedTable = table;
		m_preparedQuery = QSqlQuery(m_db->db());
		return m_preparedQuery.prepare("INSERT INTO " + table + " VALUES (?)");
	}

	bool bindAndExec(int value) {
		m_preparedQuery.bindValue(0, value);
		return m_preparedQuery.exec();
	}

	bool rebindAndExec(int value) {
		m_preparedQuery = QSqlQuery(m_db->db());
		if (!m_preparedQuery.prepare("INSERT INTO " + m_preparedTable + " VALUES (?)"))
			return false;
		m_preparedQuery.bindValue(0, value);
		return m_preparedQuery.exec();
	}

	void finalizeStmt() { m_preparedQuery = QSqlQuery(); }

	// Access
	QSqlDatabase &db() { return m_db->db(); }

private:
	std::unique_ptr<TestDatabase> m_db;
	QSqlQuery m_preparedQuery;
	QString m_preparedTable;
};
