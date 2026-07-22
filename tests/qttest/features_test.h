#pragma once

#include "../helpers/test_database.h"
#include <QFile>
#include <QRandomGenerator>
#include <QSqlDriver>
#include <QTest>

class FeaturesTest : public QObject {
	Q_OBJECT

private slots:
	void featuresSupported() {
		TestDatabase db;
		auto *drv = db.db().driver();
		QVERIFY(drv->hasFeature(QSqlDriver::BLOB));
		QVERIFY(drv->hasFeature(QSqlDriver::Transactions));
		QVERIFY(drv->hasFeature(QSqlDriver::Unicode));
		QVERIFY(drv->hasFeature(QSqlDriver::PreparedQueries));
		QVERIFY(drv->hasFeature(QSqlDriver::PositionalPlaceholders));
	}

	void featuresNotSupported() {
		TestDatabase db;
		auto *drv = db.db().driver();
		QVERIFY(!drv->hasFeature(QSqlDriver::LastInsertId));
		QVERIFY(!drv->hasFeature(QSqlDriver::NamedPlaceholders));
		QVERIFY(!drv->hasFeature(QSqlDriver::QuerySize));
		QVERIFY(!drv->hasFeature(QSqlDriver::BatchOperations));
		QVERIFY(!drv->hasFeature(QSqlDriver::MultipleResultSets));
	}

	void lastInsertIdNotSupported() {
		TestDatabase db;
		QVERIFY(!db.db().driver()->hasFeature(QSqlDriver::DriverFeature::LastInsertId));
		db.exec("CREATE SEQUENCE seq_liid_nosupport START 1");
		db.exec(R"(CREATE TABLE Persons (
			Personid INTEGER PRIMARY KEY DEFAULT nextval('seq_liid_nosupport'),
			LastName VARCHAR(255) NOT NULL,
			FirstName VARCHAR(255),
			Age INTEGER
		))");
		db.exec("INSERT INTO Persons (LastName, FirstName, Age) VALUES ('Doe', 'John', 99)");

		QSqlQuery query(db.db());
		QVERIFY(query.exec("SELECT Personid FROM Persons"));
		QVERIFY(query.next());
		QVERIFY(!query.lastInsertId().isValid());
	}

	void lastInsertIdWithReturning() {
		TestDatabase db;
		db.exec("CREATE SEQUENCE seq_liid_returning START 1");
		db.exec(R"(CREATE TABLE Persons (
			Personid INTEGER PRIMARY KEY DEFAULT nextval('seq_liid_returning'),
			LastName VARCHAR(255) NOT NULL,
			FirstName VARCHAR(255),
			Age INTEGER
		))");

		QSqlQuery query(db.db());
		query.exec("INSERT INTO Persons (LastName, FirstName, Age) VALUES ('Doe', 'John', 99) RETURNING (Personid)");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 1);
	}

	void numRowsAffectedInsert() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		QSqlQuery query(db.db());
		query.exec("INSERT INTO items VALUES (1, 'apple')");
		db.checkNoError(query);
		QCOMPARE(query.numRowsAffected(), 1);
	}

	void numRowsAffectedMultipleInsert() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		QSqlQuery query(db.db());
		query.exec("INSERT INTO items VALUES (1, 'apple')");
		db.checkNoError(query);
		query.exec("INSERT INTO items VALUES (2, 'banana')");
		db.checkNoError(query);
		query.exec("INSERT INTO items VALUES (3, 'cherry')");
		db.checkNoError(query);

		query.exec("SELECT COUNT(*) FROM items");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 3);
	}

	void timestampTest() {
		TestDatabase db;
		auto query = db.exec("SELECT TIMESTAMP '1992-09-20 11:30:00.123456789'");
		db.checkNoError(query);
		QVERIFY(query.next());
		auto test = query.value(0).toDateTime();
		QCOMPARE(test.date(), QDate(1992, 9, 20));
		QCOMPARE(test.time(), QTime(11, 30, 00, 123));
		QVERIFY(!query.next());
	}

	void autoCompleterExtension() {
		TestDatabase db;
		db.exec("CREATE TABLE weather (city VARCHAR, temp_lo INTEGER)");
		auto query = db.exec("SELECT * FROM sql_auto_complete('SELECT ci')");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toString(), "city");
	}

	void numRowsAffectedUpdate() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		db.exec("INSERT INTO items VALUES (2, 'banana')");

		QSqlQuery q(db.db());
		q.exec("UPDATE items SET name = 'avocado' WHERE id = 1");
		db.checkNoError(q);
		QCOMPARE(q.numRowsAffected(), 1);
	}

	void numRowsAffectedDelete() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		db.exec("INSERT INTO items VALUES (2, 'banana')");

		QSqlQuery q(db.db());
		q.exec("DELETE FROM items WHERE id = 1");
		db.checkNoError(q);
		QCOMPARE(q.numRowsAffected(), 1);
	}

	void numRowsAffectedMultiRowUpdate() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		db.exec("INSERT INTO items VALUES (2, 'banana')");
		db.exec("INSERT INTO items VALUES (3, 'cherry')");

		QSqlQuery q(db.db());
		q.exec("UPDATE items SET name = 'fruit'");
		db.checkNoError(q);
		QCOMPARE(q.numRowsAffected(), 3);
	}

	void decimalBinding() {
		TestDatabase db;
		db.exec("CREATE TABLE measurements (id INTEGER, val DECIMAL(10,2))");

		QSqlQuery q(db.db());
		QVERIFY(q.prepare("INSERT INTO measurements VALUES (?, ?)"));
		q.bindValue(0, 1);
		q.bindValue(1, 3.14);
		QVERIFY(q.exec());
		db.checkNoError(q);

		auto result = db.exec("SELECT val FROM measurements WHERE id = 1");
		db.checkNoError(result);
		QVERIFY(result.next());
		QCOMPARE(result.value(0).toDouble(), 3.14);
	}

	void timestampVariants() {
		TestDatabase db;
		auto q1 = db.exec("SELECT TIMESTAMP_MS '2025-01-15 10:30:00.123'");
		db.checkNoError(q1);
		QVERIFY(q1.next());
		auto dt1 = q1.value(0).toDateTime();
		QCOMPARE(dt1.date(), QDate(2025, 1, 15));
		QCOMPARE(dt1.time().hour(), 10);
		QCOMPARE(dt1.time().minute(), 30);

		auto q2 = db.exec("SELECT TIMESTAMP_NS '2025-01-15 10:30:00.123456789'");
		db.checkNoError(q2);
		QVERIFY(q2.next());
		auto val2 = q2.value(0);
		QVERIFY(!val2.toString().isEmpty());

		auto q3 = db.exec("SELECT TIMESTAMP_S '2025-01-15 10:30:00'");
		db.checkNoError(q3);
		QVERIFY(q3.next());
		auto dt3 = q3.value(0).toDateTime();
		QCOMPARE(dt3.date(), QDate(2025, 1, 15));
		QCOMPARE(dt3.time().hour(), 10);
	}

	void readOnlyConnectionOption() {
		const QString dbName = "readonly_test_" + QString::number(QRandomGenerator::global()->generate());
		{
			// Create the database file with data first
			QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB", dbName + "_create");
			db.setDatabaseName(dbName);
			QVERIFY2(db.open(), qPrintable(db.lastError().text()));
			QSqlQuery q(db);
			QVERIFY(q.exec("CREATE TABLE items (id INTEGER, name VARCHAR)"));
			QVERIFY(q.exec("INSERT INTO items VALUES (1, 'apple')"));
			db.close();
		}
		QSqlDatabase::removeDatabase(dbName + "_create");

		{
			// Open as READONLY via connection options
			QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB", dbName + "_ro");
			db.setDatabaseName(dbName);
			db.setConnectOptions("READONLY");
			QVERIFY2(db.open(), qPrintable(db.lastError().text()));

			// Reading should work
			QSqlQuery q(db);
			QVERIFY(q.exec("SELECT name FROM items WHERE id = 1"));
			QVERIFY(q.next());
			QCOMPARE(q.value(0).toString(), "apple");

			// Writing should fail
			QSqlQuery w(db);
			QVERIFY(!w.exec("INSERT INTO items VALUES (2, 'banana')"));

			db.close();
		}
		QSqlDatabase::removeDatabase(dbName + "_ro");

		// Cleanup the database file
		QFile::remove(dbName);
	}

	void execBatchInsert() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");

		QSqlQuery q(db.db());
		QVERIFY(q.prepare("INSERT INTO items VALUES (?, ?)"));

		QVariantList ids;
		ids << 1 << 2 << 3;
		QVariantList names;
		names << "apple" << "banana" << "cherry";

		q.addBindValue(ids);
		q.addBindValue(names);
		QVERIFY(q.execBatch());
		db.checkNoError(q);

		auto result = db.exec("SELECT COUNT(*) FROM items");
		db.checkNoError(result);
		QVERIFY(result.next());
		QCOMPARE(result.value(0).toInt(), 3);

		result = db.exec("SELECT id, name FROM items ORDER BY id");
		db.checkNoError(result);
		QVERIFY(result.next());
		QCOMPARE(result.value(0).toInt(), 1);
		QCOMPARE(result.value(1).toString(), "apple");
		QVERIFY(result.next());
		QCOMPARE(result.value(0).toInt(), 2);
		QCOMPARE(result.value(1).toString(), "banana");
		QVERIFY(result.next());
		QCOMPARE(result.value(0).toInt(), 3);
		QCOMPARE(result.value(1).toString(), "cherry");
		QVERIFY(!result.next());
	}
};
