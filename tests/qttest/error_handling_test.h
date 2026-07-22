#pragma once

#include "../helpers/temp_database.h"
#include "../helpers/test_database.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QTest>

class ErrorHandlingTest : public QObject {
	Q_OBJECT

private slots:
	void invalidSQLSyntax() {
		TestDatabase db;
		auto query = db.exec("SELCT * FROM nonexistent");
		QVERIFY(query.lastError().type() != QSqlError::NoError);
	}

	void queryOnNonExistentTable() {
		TestDatabase db;
		auto query = db.exec("SELECT * FROM nonexistent_table");
		QVERIFY(query.lastError().type() != QSqlError::NoError);
	}

	void insertIntoNonExistentTable() {
		TestDatabase db;
		auto query = db.exec("INSERT INTO nonexistent_table VALUES (1)");
		QVERIFY(query.lastError().type() != QSqlError::NoError);
	}

	void dropNonExistentTable() {
		TestDatabase db;
		auto query = db.exec("DROP TABLE nonexistent_table");
		QVERIFY(query.lastError().type() != QSqlError::NoError);
	}

	void closedDatabaseQuery() {
		TestDatabase db;
		db.close();
		QVERIFY(!db.isOpen());
		auto query = db.exec("SELECT 1");
		QVERIFY(!query.isActive());
	}

	void insertDuplicatePrimaryKey() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER PRIMARY KEY, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");

		auto query = db.exec("INSERT INTO items VALUES (1, 'banana')");
		QVERIFY(query.lastError().type() != QSqlError::NoError);
	}

	void divideByZero() {
		TestDatabase db;
		auto query = db.exec("SELECT 1/0");
		db.checkNoError(query);
		QVERIFY(query.next());
		QVERIFY(query.value(0).toString() == "inf" || query.value(0).toDouble() == std::numeric_limits<double>::infinity());
	}

	void typeMismatch() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		auto query = db.exec("INSERT INTO items VALUES ('not_a_number', 'apple')");
		QVERIFY(query.lastError().type() != QSqlError::NoError);
	}

	void missingColumn() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		auto query = db.exec("INSERT INTO items (id, name, extra) VALUES (1, 'apple', 'x')");
		QVERIFY(query.lastError().type() != QSqlError::NoError);
	}

	void selectFromView() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		auto query = db.exec("SELECT * FROM myview");
		QVERIFY(query.lastError().type() != QSqlError::NoError);
	}

	void openFailureInvalidPath() {
		TempDatabase tmp("openfail");
		QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB", tmp.connectionName);
		db.setDatabaseName("/nonexistent/deeply/nested/path/db.duckdb");
		QVERIFY(!db.open());
		QVERIFY(db.lastError().type() != QSqlError::NoError);
	}

	void openFailureIsolation() {
		{
			TempDatabase tmp("failiso");
			QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB", tmp.connectionName);
			db.setDatabaseName("/nonexistent/path/db.duckdb");
			db.open();
			QVERIFY(!db.isOpen());
		}

		TestDatabase db2;
		QVERIFY(db2.isOpen());
		auto q = db2.exec("SELECT 1");
		QVERIFY(q.next());
		QCOMPARE(q.value(0).toInt(), 1);
	}

	void closeWithActiveQuery() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		for (int i = 0; i < 10; ++i)
			db.exec(QString("INSERT INTO items VALUES (%1, 'item_%1')").arg(i));

		QSqlQuery q(db.db());
		QVERIFY(q.exec("SELECT id, name FROM items ORDER BY id"));
		QVERIFY(q.next());
		QCOMPARE(q.value(0).toInt(), 0);

		db.close();
		QVERIFY(!db.isOpen());
		QVERIFY(!q.isActive());
		QVERIFY(!q.next());
	}

	void multipleConnectionsSameDb() {
		TempDatabase tmp("multi_conn");
		{
			QSqlDatabase db1 = QSqlDatabase::addDatabase("DUCKDB", tmp.connectionName + "_1");
			db1.setDatabaseName(tmp.path);
			QVERIFY2(db1.open(), qPrintable(db1.lastError().text()));
			QSqlQuery q1(db1);
			q1.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
			q1.exec("INSERT INTO items VALUES (1, 'apple')");
		}
		QSqlDatabase::removeDatabase(tmp.connectionName + "_1");

		{
			QSqlDatabase db2 = QSqlDatabase::addDatabase("DUCKDB", tmp.connectionName + "_2");
			db2.setDatabaseName(tmp.path);
			QVERIFY2(db2.open(), qPrintable(db2.lastError().text()));

			QSqlQuery q2(db2);
			q2.exec("SELECT name FROM items WHERE id = 1");
			QVERIFY(q2.next());
			QCOMPARE(q2.value(0).toString(), "apple");
		}
		QSqlDatabase::removeDatabase(tmp.connectionName + "_2");
	}
};
