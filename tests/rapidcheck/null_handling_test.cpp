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

	allPassed &= rc::check("IntegerNotNull", [](int32_t value) {
		resetTable("x INTEGER");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, value);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(!q.value(0).isNull());
		RC_ASSERT(q.value(0).toInt() == value);
	});

	allPassed &= rc::check("StringNotNull", [](const QString &value) {
		RC_PRE(value.size() <= 1000);
		resetTable("x VARCHAR");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, value);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(!q.value(0).isNull());
		RC_ASSERT(q.value(0).toString() == value);
	});

	allPassed &= rc::check("IntegerNullViaSql", []() {
		resetTable("x INTEGER");
		sharedDb().exec("INSERT INTO t VALUES (NULL)");
		auto q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).isNull());
	});

	allPassed &= rc::check("VarcharNullViaSql", []() {
		resetTable("x VARCHAR");
		sharedDb().exec("INSERT INTO t VALUES (NULL)");
		auto q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).isNull());
	});

	allPassed &= rc::check("IntegerNullViaBind", []() {
		resetTable("x INTEGER");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, QVariant());
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).isNull());
	});

	allPassed &= rc::check("VarcharNullViaBind", []() {
		resetTable("x VARCHAR");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?)"));
		q.bindValue(0, QVariant());
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT x FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).isNull());
	});

	allPassed &= rc::check("MultipleColumns", [](int32_t val1, int32_t val3) {
		resetTable("a INTEGER, b INTEGER, c INTEGER");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?, ?, ?)"));
		q.bindValue(0, val1);
		q.bindValue(1, 42);
		q.bindValue(2, val3);
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT a, b, c FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(q.value(0).toInt() == val1);
		RC_ASSERT(q.value(1).toInt() == 42);
		RC_ASSERT(q.value(2).toInt() == val3);
	});

	allPassed &= rc::check("MixedNullAndValues", [](int32_t val1) {
		resetTable("a INTEGER, b INTEGER");
		QSqlQuery q(sharedDb().db());
		RC_ASSERT(q.prepare("INSERT INTO t VALUES (?, ?)"));
		q.bindValue(0, val1);
		q.bindValue(1, QVariant());
		RC_ASSERT(q.exec());

		q = sharedDb().exec("SELECT a, b FROM t");
		RC_ASSERT(q.next());
		RC_ASSERT(!q.value(0).isNull());
		RC_ASSERT(q.value(0).toInt() == val1);
		RC_ASSERT(q.value(1).isNull());
	});

	return allPassed ? 0 : 1;
}
