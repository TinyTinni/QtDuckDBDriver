#pragma once

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTest>
#include <atomic>

class TestDatabase {
public:
	TestDatabase() {
		m_name = "TESTDB_" + QString::number(s_counter.fetch_add(1));
		m_db = QSqlDatabase::addDatabase("DUCKDB", m_name);
		QVERIFY2(m_db.isValid(), qPrintable("Failed to add DUCKDB database driver"));
		m_db.setDatabaseName("");
		QVERIFY2(m_db.open(), qPrintable("Failed to open database: " + m_db.lastError().text()));
	}

	~TestDatabase() {
		if (m_db.isOpen())
			m_db.close();
		m_db = QSqlDatabase();
		QSqlDatabase::removeDatabase(m_name);
	}

	QSqlDatabase &db() { return m_db; }
	const QSqlDatabase &db() const { return m_db; }

	QSqlQuery exec(const QString &sql) {
		QSqlQuery query(m_db);
		if (!query.exec(sql)) {
			qDebug().noquote() << "Query failed:" << query.lastError().text();
			qDebug().noquote() << "SQL:" << sql;
		}
		return query;
	}

	void checkNoError(const QSqlQuery &q) {
		auto err = q.lastError();
		QVERIFY2(err.type() == QSqlError::NoError,
		         qPrintable("Unexpected error: " + err.text()));
	}

	void dropAllTables() {
		auto tables = m_db.tables(QSql::Tables);
		for (const auto &t : tables)
			exec("DROP TABLE IF EXISTS \"" + t + "\"");
		auto views = m_db.tables(QSql::Views);
		for (const auto &v : views)
			exec("DROP VIEW IF EXISTS \"" + v + "\"");
	}

	bool isOpen() const { return m_db.isOpen(); }
	void close() { m_db.close(); }
	bool open() { return m_db.open(); }

private:
	QSqlDatabase m_db;
	QString m_name;
	static inline std::atomic<int> s_counter{0};
};
