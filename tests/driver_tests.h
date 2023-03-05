#pragma once
#include "../QtDuckDBDriver/QtDuckDBDriver.h"

#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlIndex>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTest>
#include <duckdb.hpp>

class QtDuckDBTests : public QObject {
	Q_OBJECT

private:
	void checkError(const QSqlQuery &query) {
		auto err = query.lastError();
		if (err.type() != QSqlError::NoError) {
			qDebug() << err.text();
			QVERIFY(err.type() == QSqlError::NoError);
		}
	}

	QSqlDatabase m_db;

private slots:
	void initTestCase() {
		QCoreApplication::addLibraryPath("./plugins/");
		m_db = QSqlDatabase::addDatabase("DUCKDB");
	}

	void cleanupTestCase() {
		m_db.driver()->close();
		QSqlDatabase::removeDatabase(m_db.connectionName());
	}

	void queryExecution() {
		bool ok = m_db.open();
		QVERIFY(ok);
		auto query = m_db.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);

		query = m_db.exec(R"( INSERT INTO weather VALUES ('San Francisco', 46, 50, 0.25, '1994-11-27'); )");
		checkError(query);

		query = m_db.exec(R"(SELECT COUNT(*) FROM weather)");
		checkError(query);
		while (query.next()) {
			QCOMPARE(query.value(0).toInt(), 1);
		}
	}

	void preparedExecution() {
		bool ok = m_db.open();
		QVERIFY(ok);
		QSqlQuery query(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ))",
		                m_db);
		checkError(query);

		auto prepared_query = QSqlQuery(R"(INSERT INTO weather VALUES (?, ?, ?, ?, ?))");
		prepared_query.bindValue(0, "San Francisco");
		prepared_query.bindValue(1, 46);
		prepared_query.bindValue(2, 50);
		prepared_query.bindValue(3, 0.25);
		prepared_query.bindValue(4, "1994-11-27");
		prepared_query.exec();
		checkError(prepared_query);

		prepared_query.bindValue(0, "San Francisco2");
		prepared_query.bindValue(1, 80);
		prepared_query.bindValue(2, 90);
		prepared_query.bindValue(3, 0.5);
		prepared_query.bindValue(4, "2012-12-01");
		prepared_query.exec();
		checkError(prepared_query);

		query = m_db.exec(R"(SELECT COUNT(*) FROM weather)");
		checkError(query);
		while (query.next()) {
			QCOMPARE(query.value(0).toInt(), 2);
		}
		m_db.close();
	}

	void tableEnumeration() {
		bool ok = m_db.open();
		QVERIFY(ok);
		QSqlQuery query(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ))",
		                m_db);
		checkError(query);
		query = QSqlQuery(R"(CREATE TABLE weather2 (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ))",
		                  m_db);
		checkError(query);

		auto tables = m_db.tables();
		QCOMPARE(tables.size(), 2);
		QCOMPARE(tables[0], "weather");
		QCOMPARE(tables[1], "weather2");
		m_db.close();
	}

	void handleTest() {
		bool ok = m_db.open();
		QVERIFY(ok);
		auto handle = m_db.driver()->handle().value<DuckDBConnectionHandle>();
		QCOMPARE_NE(handle.db, nullptr);
		QCOMPARE_NE(handle.connection, nullptr);
		auto result = handle.connection->SendQuery(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ))");
		if (result->HasError())
			qDebug() << result->GetError().c_str();
		QVERIFY(!result->HasError());
	}

	void transactionTest() {
		bool ok = m_db.open();
		QVERIFY(ok);
		QVERIFY(m_db.transaction());
		auto query = m_db.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);
		QVERIFY(m_db.commit());
		auto tables = m_db.tables();
		QCOMPARE(tables.size(), 1);
	}

	void rollbackTest() {
		bool ok = m_db.open();
		QVERIFY(ok);
		QVERIFY(m_db.transaction());
		auto query = m_db.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);
		QVERIFY(m_db.rollback());
		auto tables = m_db.tables();
		QCOMPARE(tables.size(), 0);
	}

	void recordTest() {
		bool ok = m_db.open();
		QVERIFY(ok);
		auto query = m_db.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);
		auto rec = m_db.driver()->record("weather");
		QVERIFY(rec.count() == 5);
		QVERIFY(rec.contains("city"));
		QVERIFY(rec.contains("temp_lo"));
		QVERIFY(rec.contains("temp_hi"));
		QVERIFY(rec.contains("prcp"));
		QVERIFY(rec.contains("date"));
	}

	void primaryIndexTest() {
		bool ok = m_db.open();
		QVERIFY(ok);
		auto query = m_db.exec(R"(CREATE TABLE weather (
			id			   INT PRIMARY KEY,
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);
		query = m_db.exec("CREATE UNIQUE INDEX city_idx ON weather(city)");
		checkError(query);
		auto idx = m_db.driver()->primaryIndex("weather");
		QCOMPARE(idx.count(), 1);
		QVERIFY(idx.contains("id"));
	}
};