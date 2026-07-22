#include "../helpers/rapidcheck_generators.h"
#include "../helpers/test_database.h"
#include <QCoreApplication>
#include <QSqlQuery>
#include <rapidcheck.h>

static TestDatabase &sharedDb() {
	static auto *db = new TestDatabase();
	return *db;
}

static void resetTable(const QString &schema) {
	sharedDb().exec("DROP TABLE IF EXISTS t");
	sharedDb().exec("CREATE TABLE t (" + schema + ")");
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::addLibraryPath("./plugins/");

	bool allPassed = true;

	allPassed &= rc::check("IntegerBinding", [](int32_t val1, int32_t val2) {
		resetTable("a INTEGER, b INTEGER");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?, ?)"));
		q.bindValue(0, val1);
		q.bindValue(1, val2);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT a, b FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toInt() == val1);
		RC_ASSERT(q.value(1).toInt() == val2);
	});

	allPassed &= rc::check("StringBinding", [](const QString &val) {
		RC_PRE(val.size() <= 1000);
		resetTable("x VARCHAR");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, val);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toString() == val);
	});

	allPassed &= rc::check("MixedTypes", [](int32_t intVal, const QString &strVal) {
		RC_PRE(strVal.size() <= 1000);
		resetTable("id INTEGER, name VARCHAR");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?, ?)"));
		q.bindValue(0, intVal);
		q.bindValue(1, strVal);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT id, name FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toInt() == intVal);
		RC_ASSERT(q.value(1).toString() == strVal);
	});

	allPassed &= rc::check("ReuseWithNewValues", [](int32_t val1, int32_t val2) {
		resetTable("x INTEGER");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));

		q.bindValue(0, val1);
		RC_ASSERT(q.exec());

		q.bindValue(0, val2);
		RC_ASSERT(q.exec());

		auto count = sharedDb().exec("SELECT COUNT(*) FROM t");
		RC_ASSERT(count.next());
		RC_ASSERT(count.value(0).toInt() == 2);

		auto check = sharedDb().exec("SELECT x FROM t ORDER BY rowid");
		RC_ASSERT(check.next());
		RC_ASSERT(check.value(0).toInt() == val1);
		RC_ASSERT(check.next());
		RC_ASSERT(check.value(0).toInt() == val2);
	});

	return allPassed ? 0 : 1;
}
