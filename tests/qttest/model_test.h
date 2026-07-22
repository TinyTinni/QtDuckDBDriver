#pragma once

#include "../helpers/test_database.h"
#include <QSqlQuery>
#include <QSqlTableModel>
#include <QTest>

class ModelTest : public QObject {
	Q_OBJECT

private slots:
	void basicModel() {
		TestDatabase db;
		db.exec("CREATE TABLE employee (Name VARCHAR, Salary INTEGER)");
		db.exec("INSERT INTO employee VALUES ('Paul', 5000)");
		db.exec("INSERT INTO employee VALUES ('Bert', 5500)");
		db.exec("INSERT INTO employee VALUES ('Tina', 6500)");
		db.exec("INSERT INTO employee VALUES ('Alice', 6500)");

		QSqlTableModel model(nullptr, db.db());
		model.setTable("employee");
		model.select();

		QCOMPARE(model.rowCount(), 4);
		QCOMPARE(model.columnCount(), 2);
		QCOMPARE(model.data(model.index(2, 0)).toString(), "Tina");
	}

	void modelEmptyTable() {
		TestDatabase db;
		db.exec("CREATE TABLE empty_table (id INTEGER, name VARCHAR)");

		QSqlTableModel model(nullptr, db.db());
		model.setTable("empty_table");
		model.select();

		QCOMPARE(model.rowCount(), 0);
		QCOMPARE(model.columnCount(), 2);
	}

	void modelLargeDataset() {
		TestDatabase db;
		db.exec("CREATE TABLE big_table (id INTEGER, value VARCHAR)");
		QSqlQuery q(db.db());
		QVERIFY(q.prepare("INSERT INTO big_table VALUES (?, ?)"));
		for (int i = 0; i < 100; i++) {
			q.bindValue(0, i);
			q.bindValue(1, "value_" + QString::number(i));
			QVERIFY(q.exec());
		}

		QSqlTableModel model(nullptr, db.db());
		model.setTable("big_table");
		model.select();

		QCOMPARE(model.rowCount(), 100);
		QCOMPARE(model.data(model.index(50, 1)).toString(), "value_50");
	}
};
