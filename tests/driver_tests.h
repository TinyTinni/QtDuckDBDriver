#pragma once
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTest>

class SimpleTests : public QObject {
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

	void simpleExecution() {
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

	void tableFunctions() {
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

	void transactionTest() {
	}

	void cleanupTestCase() {
	}
};