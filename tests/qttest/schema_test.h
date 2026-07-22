#pragma once

#include "../helpers/test_database.h"
#include <QSqlDriver>
#include <QSqlIndex>
#include <QSqlRecord>
#include <QTest>

class SchemaTest : public QObject {
	Q_OBJECT

private slots:
	void tableEnumeration() {
		TestDatabase db;
		db.exec("CREATE TABLE weather (city VARCHAR, temp_lo INTEGER, temp_hi INTEGER, prcp REAL, date DATE)");
		db.exec("CREATE TABLE weather2 (city VARCHAR, temp_lo INTEGER, temp_hi INTEGER, prcp REAL, date DATE)");

		auto tables = db.db().tables();
		QCOMPARE(tables.size(), 2);
		tables.sort();
		QCOMPARE(tables[0], "weather");
		QCOMPARE(tables[1], "weather2");
	}

	void tableEnumerationEmpty() {
		TestDatabase db;
		auto tables = db.db().tables();
		QCOMPARE(tables.size(), 0);
	}

	void recordTest() {
		TestDatabase db;
		db.exec("CREATE TABLE weather (city VARCHAR, temp_lo INTEGER, temp_hi INTEGER, prcp REAL, date DATE)");
		auto rec = db.db().driver()->record("weather");
		QVERIFY(rec.count() == 5);
		QVERIFY(rec.contains("city"));
		QVERIFY(rec.contains("temp_lo"));
		QVERIFY(rec.contains("temp_hi"));
		QVERIFY(rec.contains("prcp"));
		QVERIFY(rec.contains("date"));
	}

	void recordNonExistentTable() {
		TestDatabase db;
		auto rec = db.db().driver()->record("nonexistent");
		QCOMPARE(rec.count(), 0);
	}

	void primaryIndexTest() {
		TestDatabase db;
		db.exec("CREATE SEQUENCE seq_personid START 1");
		db.exec(R"(CREATE TABLE Persons (
			Personid INTEGER PRIMARY KEY DEFAULT nextval('seq_personid'),
			LastName VARCHAR(255) NOT NULL,
			FirstName VARCHAR(255),
			Age INTEGER
		))");

		auto index = db.db().primaryIndex("Persons");
		QCOMPARE(index.count(), 1);
		QCOMPARE(index.fieldName(0), "Personid");
	}

	void recordWithEscapedIdentifier() {
		TestDatabase db;
		db.exec("CREATE TABLE mytable (id INTEGER, name VARCHAR)");
		auto rec = db.db().driver()->record("\"mytable\"");
		QVERIFY(rec.count() == 2);
		QVERIFY(rec.contains("id"));
		QVERIFY(rec.contains("name"));
	}

	void escapeIdentifierEmpty() {
		TestDatabase db;
		auto escaped = db.db().driver()->escapeIdentifier("", QSqlDriver::FieldName);
		QVERIFY(escaped.isEmpty());
	}

	void escapeIdentifierBrackets() {
		TestDatabase db;
		auto escaped = db.db().driver()->escapeIdentifier("[mytable]", QSqlDriver::TableName);
		QCOMPARE(escaped, "[mytable]");
	}

	void viewEnumeration() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		db.exec("CREATE VIEW active_items AS SELECT * FROM items WHERE id = 1");

		auto views = db.db().tables(QSql::Views);
		QVERIFY(views.contains("active_items"));

		auto tablesOnly = db.db().tables(QSql::Tables);
		QVERIFY(!tablesOnly.contains("active_items"));
	}

	void systemTablesEnumeration() {
		TestDatabase db;
		db.exec("CREATE TABLE mytable (id INTEGER)");

		auto systemTables = db.db().tables(static_cast<QSql::TableType>(QSql::Tables | QSql::SystemTables));
		QVERIFY(systemTables.size() > 0);
		QVERIFY(systemTables.contains("mytable"));

		auto userTables = db.db().tables(QSql::Tables);
		QVERIFY(userTables.contains("mytable"));
		QVERIFY(!userTables.contains("sqlite_master"));
	}
};
