#pragma once

#include "../helpers/test_database.h"
#include <QSqlRecord>
#include <QTest>

class QueryExecutionTest : public QObject {
	Q_OBJECT

private slots:
	void selectBasic() {
		TestDatabase db;
		db.exec("CREATE TABLE weather (city VARCHAR, temp_lo INTEGER, temp_hi INTEGER, prcp REAL, date DATE)");
		db.exec("INSERT INTO weather VALUES ('San Francisco', 46, 50, 0.25, '1994-11-27')");

		auto query = db.exec("SELECT city, temp_lo, temp_hi, prcp, date FROM weather");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toString(), "San Francisco");
		QCOMPARE(query.value(1).toInt(), 46);
		QCOMPARE(query.value(2).toInt(), 50);
		QCOMPARE(query.value(3).toDouble(), 0.25);
		QCOMPARE(query.value(4).toDate(), QDate(1994, 11, 27));
		QVERIFY(!query.next());
	}

	void insertAndSelect() {
		TestDatabase db;
		db.exec("CREATE TABLE weather (city VARCHAR, temp_lo INTEGER, temp_hi INTEGER, prcp REAL, date DATE)");
		db.exec("INSERT INTO weather VALUES ('San Francisco', 46, 50, 0.25, '1994-11-27')");
		db.exec("INSERT INTO weather VALUES ('New York', 30, 40, 0.5, '2020-01-15')");

		auto query = db.exec("SELECT COUNT(*) FROM weather");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 2);
	}

	void insertMultipleRowsAndVerify() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		db.exec("INSERT INTO items VALUES (2, 'banana')");
		db.exec("INSERT INTO items VALUES (3, 'cherry')");

		auto query = db.exec("SELECT name FROM items ORDER BY id");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toString(), "apple");
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toString(), "banana");
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toString(), "cherry");
		QVERIFY(!query.next());
	}

	void updateAndSelect() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		db.exec("UPDATE items SET name = 'avocado' WHERE id = 1");

		auto query = db.exec("SELECT name FROM items WHERE id = 1");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toString(), "avocado");
	}

	void deleteAndSelect() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		db.exec("INSERT INTO items VALUES (2, 'banana')");
		db.exec("DELETE FROM items WHERE id = 1");

		auto query = db.exec("SELECT COUNT(*) FROM items");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 1);

		query = db.exec("SELECT name FROM items");
		db.checkNoError(query);
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toString(), "banana");
	}

	void largeResultSet() {
		TestDatabase db;
		db.exec("CREATE TABLE big (id INTEGER, val VARCHAR)");

		const int rowCount = 1000;
		const int batchSize = 100;
		for (int batch = 0; batch < rowCount; batch += batchSize) {
			QString sql = "INSERT INTO big VALUES ";
			for (int i = batch; i < batch + batchSize && i < rowCount; ++i) {
				if (i > batch)
					sql += ", ";
				sql += "(" + QString::number(i) + ", 'row_" + QString::number(i) + "')";
			}
			auto ins = db.exec(sql);
			db.checkNoError(ins);
		}

		auto q = db.exec("SELECT COUNT(*) FROM big");
		db.checkNoError(q);
		QVERIFY(q.next());
		QCOMPARE(q.value(0).toInt(), rowCount);

		q = db.exec("SELECT id, val FROM big ORDER BY id");
		db.checkNoError(q);
		int count = 0;
		int lastId = -1;
		while (q.next()) {
			QCOMPARE(q.value(0).toInt(), count);
			QCOMPARE(q.value(1).toString(), "row_" + QString::number(count));
			lastId = q.value(0).toInt();
			++count;
		}
		QCOMPARE(count, rowCount);
		QCOMPARE(lastId, rowCount - 1);
	}

	void unicodeData() {
		TestDatabase db;
		db.exec("CREATE TABLE uni (id INTEGER, text VARCHAR)");

		const QString emoji = QStringLiteral("Hello \U0001F600 World \U0001F4A9");
		const QString cjk = QStringLiteral("\u4F60\u597D\u4E16\u754C");
		const QString arabic = QStringLiteral("\u0645\u0631\u062D\u0628\u0627");
		const QString accented = QStringLiteral("caf\u00E9 \u00FCber \u00F1");

		QSqlQuery q(db.db());
		QVERIFY(q.prepare("INSERT INTO uni VALUES (?, ?)"));
		q.bindValue(0, 1);
		q.bindValue(1, emoji);
		QVERIFY(q.exec());
		q.bindValue(0, 2);
		q.bindValue(1, cjk);
		QVERIFY(q.exec());
		q.bindValue(0, 3);
		q.bindValue(1, arabic);
		QVERIFY(q.exec());
		q.bindValue(0, 4);
		q.bindValue(1, accented);
		QVERIFY(q.exec());

		auto result = db.exec("SELECT id, text FROM uni ORDER BY id");
		db.checkNoError(result);
		QVERIFY(result.next());
		QCOMPARE(result.value(1).toString(), emoji);
		QVERIFY(result.next());
		QCOMPARE(result.value(1).toString(), cjk);
		QVERIFY(result.next());
		QCOMPARE(result.value(1).toString(), arabic);
		QVERIFY(result.next());
		QCOMPARE(result.value(1).toString(), accented);
	}

	void resultSetRecord() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR, price REAL)");
		db.exec("INSERT INTO items VALUES (1, 'apple', 1.5)");

		QSqlQuery q = db.exec("SELECT id, name, price FROM items");
		db.checkNoError(q);
		auto rec = q.record();
		QCOMPARE(rec.count(), 3);
		QCOMPARE(rec.fieldName(0), "id");
		QCOMPARE(rec.fieldName(1), "name");
		QCOMPARE(rec.fieldName(2), "price");
	}

	void resultSetRecordEmpty() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");

		QSqlQuery q = db.exec("SELECT id, name FROM items WHERE 1=0");
		db.checkNoError(q);
		auto rec = q.record();
		QCOMPARE(rec.count(), 2);
		QCOMPARE(rec.fieldName(0), "id");
		QCOMPARE(rec.fieldName(1), "name");
	}

	void deleteNumRowsAffected() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		db.exec("INSERT INTO items VALUES (1, 'apple')");
		db.exec("INSERT INTO items VALUES (2, 'banana')");
		db.exec("INSERT INTO items VALUES (3, 'cherry')");

		QSqlQuery q(db.db());
		q.exec("DELETE FROM items WHERE id <= 2");
		db.checkNoError(q);
		QCOMPARE(q.numRowsAffected(), 2);

		auto count = db.exec("SELECT COUNT(*) FROM items");
		db.checkNoError(count);
		QVERIFY(count.next());
		QCOMPARE(count.value(0).toInt(), 1);
	}

	void detachFromResultSet() {
		TestDatabase db;
		db.exec("CREATE TABLE items (id INTEGER, name VARCHAR)");
		for (int i = 0; i < 5; ++i)
			db.exec(QString("INSERT INTO items VALUES (%1, 'item_%1')").arg(i));

		QSqlQuery q(db.db());
		QVERIFY(q.exec("SELECT id, name FROM items ORDER BY id"));
		QVERIFY(q.next());
		QCOMPARE(q.value(0).toInt(), 0);

		q.finish();
		QVERIFY(!q.isActive());
		QVERIFY(!q.next());
	}
};
