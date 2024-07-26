#pragma once
#include "../QtDuckDBDriver/QtDuckDBDriver.h"

#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlIndex>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTest>
#include <duckdb.hpp>
#include <memory>

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

	std::unique_ptr<QSqlDatabase> m_db;

private slots:
	void initTestCase() {
		QCoreApplication::addLibraryPath("./plugins/");
		m_db = std::make_unique<QSqlDatabase>(QSqlDatabase::addDatabase("DUCKDB"));
	}

	void cleanupTestCase() {
		m_db.reset();
		QSqlDatabase::removeDatabase("DUCKDB");
	}

	void queryExecution() {
		bool ok = m_db->open();
		QVERIFY(ok);
		QSqlQuery query(*m_db);
		query.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);

		query.exec(R"( INSERT INTO weather VALUES ('San Francisco', 46, 50, 0.25, '1994-11-27'); )");
		checkError(query);
		while (query.next()) {
			QCOMPARE(query.size(), 4);
			QCOMPARE(query.value(0).toInt(), 46);
			QCOMPARE(query.value(1).toInt(), 50);
			QCOMPARE(query.value(2).toString(), "0.25");
			QCOMPARE(query.value(3).toDate(), QDate(1994, 11, 27));
		}

		query.exec(R"(SELECT COUNT(*) FROM weather)");
		checkError(query);
		while (query.next()) {
			QCOMPARE(query.value(0).toInt(), 1);
		}
	}

	void preparedExecution() {
		bool ok = m_db->open();
		QVERIFY(ok);
		QSqlQuery query(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ))",
		                *m_db);
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

		query.exec(R"(SELECT COUNT(*) FROM weather)");
		checkError(query);
		while (query.next()) {
			QCOMPARE(query.value(0).toInt(), 2);
		}
		m_db->close();
	}

	void preparedExecutionNamedPlaceholders() {
		bool ok = m_db->open();
		QVERIFY(ok);
		QSqlQuery query(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ))",
		                *m_db);
		checkError(query);

		auto features = m_db->driver()->hasFeature(QSqlDriver::NamedPlaceholders);

		QSqlQuery prepared_query;
		bool prepared = prepared_query.prepare(
		    R"(INSERT INTO weather (city, temp_lo, temp_hi, prcp, date) VALUES (:city, :temp_lo, :temp_hi, :prcp, :date))");
		prepared_query.bindValue(":city", "San Francisco");
		prepared_query.bindValue(":temp_lo", 46);
		prepared_query.bindValue(":temp_hi", 50);
		prepared_query.bindValue(":prcp", 0.25);
		prepared_query.bindValue(":date", "1994-11-27");
		prepared_query.exec();
		checkError(prepared_query);

		prepared_query.bindValue(":city", "San Francisco2");
		prepared_query.bindValue(1, 80);
		prepared_query.bindValue(2, 90);
		prepared_query.bindValue(":prcp", 0.5);
		prepared_query.bindValue(4, "2012-12-01");
		prepared_query.exec();
		checkError(prepared_query);

		query.exec(R"(SELECT COUNT(*) FROM weather)");
		checkError(query);
		while (query.next()) {
			QCOMPARE(query.value(0).toInt(), 2);
		}
		m_db->close();
	}

	void tableEnumeration() {
		bool ok = m_db->open();
		QVERIFY(ok);
		QSqlQuery query(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ))",
		                *m_db);
		checkError(query);
		query = QSqlQuery(R"(CREATE TABLE weather2 (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ))",
		                  *m_db);
		checkError(query);

		auto tables = m_db->tables();
		QCOMPARE(tables.size(), 2);
		tables.sort();
		QCOMPARE(tables[0], "weather");
		QCOMPARE(tables[1], "weather2");
		m_db->close();
	}

	void handleTest() {
		bool ok = m_db->open();
		QVERIFY(ok);
		auto handle = m_db->driver()->handle().value<DuckDBConnectionHandle>();
		QVERIFY(handle.db);
		QVERIFY(handle.connection);
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
		bool ok = m_db->open();
		QVERIFY(ok);
		QVERIFY(m_db->transaction());
		QSqlQuery query(*m_db);
		query.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);
		QVERIFY(m_db->commit());
		auto tables = m_db->tables();
		QCOMPARE(tables.size(), 1);
	}

	void rollbackTest() {
		bool ok = m_db->open();
		QVERIFY(ok);
		QVERIFY(m_db->transaction());
		QSqlQuery query(*m_db);
		query.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);
		QVERIFY(m_db->rollback());
		auto tables = m_db->tables();
		QCOMPARE(tables.size(), 0);
	}

	void recordTest() {
		bool ok = m_db->open();
		QVERIFY(ok);
		QSqlQuery query(*m_db);
		query.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);
		auto rec = m_db->driver()->record("weather");
		QVERIFY(rec.count() == 5);
		QVERIFY(rec.contains("city"));
		QVERIFY(rec.contains("temp_lo"));
		QVERIFY(rec.contains("temp_hi"));
		QVERIFY(rec.contains("prcp"));
		QVERIFY(rec.contains("date"));
	}

	void dateTimeTest() {
		bool ok = m_db->open();
		QVERIFY(ok);
		QSqlQuery query(*m_db);
		query.exec(R"(SELECT DATETIME '1992-09-20 11:30:00.123456789')");
		checkError(query);
		QVERIFY(query.next());
		auto test = query.value(0).toDateTime();
		QCOMPARE(test.date(), QDate(1992, 9, 20));
		QCOMPARE(test.time(), QTime(11, 30, 00, 123));
		QVERIFY(query.next() == false);
	}

	void timestampTest() {
		bool ok = m_db->open();
		QVERIFY(ok);
		QSqlQuery query(*m_db);
		query.exec(R"(SELECT TIMESTAMP '1992-09-20 11:30:00.123456789')");
		checkError(query);
		QVERIFY(query.next());
		auto test = query.value(0).toDateTime();
		QCOMPARE(test.date(), QDate(1992, 9, 20));
		QCOMPARE(test.time(), QTime(11, 30, 00, 123));
		QVERIFY(query.next() == false);
	}

	void lastInsertIdTest() {
		// so far, not supported, see lastInsertIdTestWithReturning for a substitution
		QVERIFY(m_db->open());
		QVERIFY(!m_db->driver()->hasFeature(QSqlDriver::DriverFeature::LastInsertId));
		QSqlQuery query(*m_db);
		query.exec(R"(CREATE SEQUENCE seq_personid START 1;)");
		checkError(query);
		query.exec(R"(CREATE TABLE Persons (
					Personid integer primary key default nextval('seq_personid'),
					LastName varchar(255) not null,
					FirstName varchar(255),
					Age integer
					);)");
		checkError(query);
		query.exec(R"(INSERT INTO Persons (LastName, FirstName, Age) VALUES ('Doe', 'John', 99);)");
		checkError(query);
		QVERIFY(!query.lastInsertId().isValid()); // should be 1 if feature is supported
	}

	void lastInsertIdTestWithReturning() {
		QVERIFY(m_db->open());
		QSqlQuery query(*m_db);
		query.exec(R"(CREATE SEQUENCE seq_personid START 1;)");
		checkError(query);
		query.exec(R"(CREATE TABLE Persons (
					Personid integer primary key default nextval('seq_personid'),
					LastName varchar(255) not null,
					FirstName varchar(255),
					Age integer
					);)");
		checkError(query);
		query.exec(
		    R"(INSERT INTO Persons (LastName, FirstName, Age) VALUES ('Doe', 'John', 99) RETURNING (Personid);)");
		checkError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 1);
	}

	void exampleTableModel() {
		QVERIFY(m_db->open());
		QSqlQuery query(*m_db);
		query.exec("CREATE TABLE employee (Name VARCHAR, Salary INTEGER);");
		query.exec("INSERT INTO employee VALUES ('Paul', 5000);");
		query.exec("INSERT INTO employee VALUES ('Bert', 5500);");
		query.exec("INSERT INTO employee VALUES ('Tina', 6500);");
		query.exec("INSERT INTO employee VALUES ('Alice', 6500);");

		QSqlTableModel model(nullptr, *m_db);
		model.setTable("employee");
		model.select();

		QCOMPARE(model.rowCount(), 4);
		QCOMPARE(model.columnCount(), 2);
		QCOMPARE(model.data(model.index(2, 0)).toString(), "Tina");
	}

	void returnSimplePrimaryKey() {
		QVERIFY(m_db->open());
		QSqlQuery query(*m_db);
		query.exec(R"(CREATE SEQUENCE seq_personid START 1;)");
		checkError(query);
		query.exec(R"(CREATE TABLE Persons (
					Personid integer primary key default nextval('seq_personid'),
					LastName varchar(255) not null,
					FirstName varchar(255),
					Age integer
					);)");
		checkError(query);
		auto index = m_db->primaryIndex("Persons");
		auto count = index.count();
		auto fieldName = index.fieldName(0);
		QCOMPARE(count, 1);
		QCOMPARE(fieldName, "Personid");
	}

	void autoCompleterExtension() {
		QVERIFY(m_db->open());
		QSqlQuery query(*m_db);
		query.exec(R"(CREATE TABLE weather (
            city           VARCHAR,
            temp_lo        INTEGER, 
            temp_hi        INTEGER,
            prcp           REAL,
            date           DATE
        ); )");
		checkError(query);
		// action
		query.exec(R"(SELECT * FROM sql_auto_complete('SELECT ci'); )");
		checkError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toString(), "city");
	}
};
