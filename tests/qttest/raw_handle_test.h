#pragma once

#include "../../QtDuckDBDriver/QtDuckDBDriver.h"
#include "../helpers/test_database.h"
#include <QSqlDatabase>
#include <QTest>
#include <duckdb.hpp>

class RawHandleTest : public QObject {
	Q_OBJECT

private slots:
	void handleValid() {
		TestDatabase db;
		auto handle = db.db().driver()->handle().value<DuckDBConnectionHandle>();
		QVERIFY(handle.db);
		QVERIFY(handle.connection);
	}

	void handleCreateTable() {
		TestDatabase db;
		auto handle = db.db().driver()->handle().value<DuckDBConnectionHandle>();
		QVERIFY(handle.connection);

		auto result = handle.connection->SendQuery(R"(CREATE TABLE weather (
			city VARCHAR,
			temp_lo INTEGER,
			temp_hi INTEGER,
			prcp REAL,
			date DATE
		))");
		if (result->HasError())
			qDebug() << result->GetError().c_str();
		QVERIFY(!result->HasError());
	}

	void handleInsertViaRaw() {
		TestDatabase db;
		auto handle = db.db().driver()->handle().value<DuckDBConnectionHandle>();
		QVERIFY(handle.connection);

		auto result = handle.connection->SendQuery("CREATE TABLE items (id INTEGER, name VARCHAR)");
		QVERIFY(!result->HasError());

		result = handle.connection->SendQuery("INSERT INTO items VALUES (1, 'test')");
		QVERIFY(!result->HasError());

		auto query = db.exec("SELECT COUNT(*) FROM items");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 1);
	}
};
