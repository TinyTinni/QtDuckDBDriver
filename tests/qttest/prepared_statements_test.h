#pragma once

#include "../helpers/test_database.h"
#include <QSqlDriver>
#include <QTest>

class PreparedStatementsTest : public QObject {
	Q_OBJECT

private slots:
	void positionalPlaceholders() {
		TestDatabase db;
		db.exec("CREATE TABLE weather (city VARCHAR, temp_lo INTEGER, temp_hi INTEGER, prcp REAL, date DATE)");

		QSqlQuery prepared_query(db.db());
		QVERIFY(prepared_query.prepare("INSERT INTO weather VALUES (?, ?, ?, ?, ?)"));
		prepared_query.bindValue(0, "San Francisco");
		prepared_query.bindValue(1, 46);
		prepared_query.bindValue(2, 50);
		prepared_query.bindValue(3, 0.25);
		prepared_query.bindValue(4, "1994-11-27");
		prepared_query.exec();
		db.checkNoError(prepared_query);

		prepared_query.bindValue(0, "New York");
		prepared_query.bindValue(1, 30);
		prepared_query.bindValue(2, 40);
		prepared_query.bindValue(3, 0.5);
		prepared_query.bindValue(4, "2020-01-15");
		prepared_query.exec();
		db.checkNoError(prepared_query);

		auto query = db.exec("SELECT COUNT(*) FROM weather");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 2);

		query = db.exec("SELECT prcp FROM weather");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toDouble(), 0.25);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toDouble(), 0.5);
	}

	void namedPlaceholders() {
		TestDatabase db;
		db.exec("CREATE TABLE weather (city VARCHAR, temp_lo INTEGER, temp_hi INTEGER, prcp REAL, date DATE)");
		QVERIFY(!db.db().driver()->hasFeature(QSqlDriver::NamedPlaceholders));

		QSqlQuery prepared_query(db.db());
		QVERIFY(prepared_query.prepare(
		    "INSERT INTO weather (city, temp_lo, temp_hi, prcp, date) VALUES (:city, :temp_lo, :temp_hi, :prcp, :date)"));
		prepared_query.bindValue(":city", "San Francisco");
		prepared_query.bindValue(":temp_lo", 46);
		prepared_query.bindValue(":temp_hi", 50);
		prepared_query.bindValue(":prcp", 0.25);
		prepared_query.bindValue(":date", "1994-11-27");
		prepared_query.exec();
		db.checkNoError(prepared_query);

		prepared_query.bindValue(":city", "New York");
		prepared_query.bindValue(1, 30);
		prepared_query.bindValue(2, 40);
		prepared_query.bindValue(":prcp", 0.5);
		prepared_query.bindValue(4, "2020-01-15");
		prepared_query.exec();
		db.checkNoError(prepared_query);

		auto query = db.exec("SELECT COUNT(*) FROM weather");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 2);
	}

	void timeBinding() {
		TestDatabase db;
		db.exec("CREATE TABLE schedules (id INTEGER, t TIME)");

		QTime t(9, 15, 30);
		QSqlQuery q(db.db());
		QVERIFY(q.prepare("INSERT INTO schedules VALUES (?, ?)"));
		q.bindValue(0, 1);
		q.bindValue(1, t);
		QVERIFY(q.exec());
		db.checkNoError(q);

		auto result = db.exec("SELECT t FROM schedules WHERE id = 1");
		db.checkNoError(result);
		QVERIFY(result.next());
		auto retrieved = result.value(0).toTime();
		QCOMPARE(retrieved.hour(), t.hour());
		QCOMPARE(retrieved.minute(), t.minute());
		QCOMPARE(retrieved.second(), t.second());
	}

};
