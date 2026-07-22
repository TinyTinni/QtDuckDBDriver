#include "../helpers/rapidcheck_generators.h"
#include "../helpers/test_database.h"
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QSqlQuery>
#include <rapidcheck.h>

static TestDatabase &sharedDb() {
	static auto *db = new TestDatabase();
	return *db;
}

static void resetTable(const QString &type) {
	auto &db = sharedDb();
	db.exec("DROP TABLE IF EXISTS t");
	db.exec("CREATE TABLE t (x " + type + ")");
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::addLibraryPath("./plugins/");

	bool allPassed = true;

	allPassed &= rc::check("Integer roundtrip", [](int32_t val) {
		resetTable("INTEGER");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toInt() == val);
	});

	allPassed &= rc::check("Bigint roundtrip", [](int64_t val) {
		resetTable("BIGINT");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, QVariant::fromValue(static_cast<qint64>(val)));
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toLongLong() == val);
	});

	allPassed &= rc::check("Varchar roundtrip", [](const QString &val) {
		RC_PRE(val.size() <= 1000);
		resetTable("VARCHAR");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toString() == val);
	});

	allPassed &= rc::check("Real roundtrip", [](float val) {
		RC_PRE(!std::isnan(val));
		RC_PRE(!std::isinf(val));
		resetTable("REAL");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, static_cast<double>(val));
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toFloat() == val);
	});

	allPassed &= rc::check("Double roundtrip", [](double val) {
		RC_PRE(!std::isnan(val));
		RC_PRE(!std::isinf(val));
		resetTable("DOUBLE");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toDouble() == val);
	});

	allPassed &= rc::check("Date roundtrip", [](const QDate &val) {
		resetTable("DATE");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toDate() == val);
	});

	allPassed &= rc::check("Datetime roundtrip", [](const QDateTime &val) {
		resetTable("DATETIME");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toDateTime() == val);
	});

	allPassed &= rc::check("Timestamp roundtrip", [](const QDateTime &val) {
		resetTable("TIMESTAMP");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toDateTime() == val);
	});

	allPassed &= rc::check("Boolean roundtrip", [](bool val) {
		resetTable("BOOLEAN");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toBool() == val);
	});

	allPassed &= rc::check("Smallint roundtrip", [](int16_t val) {
		resetTable("SMALLINT");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, static_cast<int>(val));
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toInt() == static_cast<int>(val));
	});

	allPassed &= rc::check("Blob roundtrip", [](const QByteArray &val) {
		RC_PRE(val.size() <= 10000);
		resetTable("BLOB");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toByteArray() == val);
	});

	allPassed &= rc::check("UBigint roundtrip", [](uint64_t val) {
		resetTable("UBIGINT");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, QVariant::fromValue(static_cast<quint64>(val)));
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toULongLong() == val);
	});

	return allPassed ? 0 : 1;
}
