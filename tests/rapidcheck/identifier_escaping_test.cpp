#include "../helpers/rapidcheck_generators.h"
#include "../helpers/test_database.h"
#include <QCoreApplication>
#include <QSqlDriver>
#include <rapidcheck.h>

static TestDatabase &sharedDb() {
	static auto *db = new TestDatabase();
	return *db;
}

static bool isAlreadyQuoted(const QString &s) {
	return (s.startsWith('"') && s.endsWith('"')) ||
	       (s.contains('[') && s.contains(']'));
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::addLibraryPath("./plugins/");

	bool allPassed = true;

	allPassed &= rc::check("NoSQLInjection", [](const QString &identifier) {
		RC_PRE(!identifier.isEmpty());
		RC_PRE(identifier.size() <= 64);
		auto escaped = sharedDb().db().driver()->escapeIdentifier(identifier, QSqlDriver::FieldName);
		if (!isAlreadyQuoted(identifier)) {
			RC_ASSERT(escaped.startsWith('"'));
			RC_ASSERT(escaped.endsWith('"'));
		}
		RC_ASSERT(escaped.contains(identifier));
	});

	allPassed &= rc::check("TableName", [](const QString &name) {
		RC_PRE(!name.isEmpty());
		RC_PRE(name.size() <= 64);
		RC_PRE(!name.contains('.'));
		RC_PRE(!isAlreadyQuoted(name));
		auto escaped = sharedDb().db().driver()->escapeIdentifier(name, QSqlDriver::TableName);
		RC_ASSERT(escaped == "\"" + name + "\"");
	});

	allPassed &= rc::check("FieldName", [](const QString &name) {
		RC_PRE(!name.isEmpty());
		RC_PRE(name.size() <= 64);
		RC_PRE(!isAlreadyQuoted(name));
		auto escaped = sharedDb().db().driver()->escapeIdentifier(name, QSqlDriver::FieldName);
		RC_ASSERT(escaped == "\"" + name + "\"");
	});

	allPassed &= rc::check("WithDots", [](const QString &schema, const QString &table) {
		RC_PRE(!schema.isEmpty());
		RC_PRE(!table.isEmpty());
		RC_PRE(schema.size() <= 32);
		RC_PRE(table.size() <= 32);
		RC_PRE(!isAlreadyQuoted(schema));
		RC_PRE(!isAlreadyQuoted(table));

		auto full = schema + "." + table;
		auto escaped = sharedDb().db().driver()->escapeIdentifier(full, QSqlDriver::TableName);
		RC_ASSERT(escaped == "\"" + schema + "\".\"" + table + "\"");
	});

	allPassed &= rc::check("WithDoubleQuotes", [](const QString &name) {
		RC_PRE(!name.isEmpty());
		RC_PRE(name.size() <= 64);
		RC_PRE(!isAlreadyQuoted(name));
		auto escaped = sharedDb().db().driver()->escapeIdentifier(name, QSqlDriver::FieldName);
		RC_ASSERT(escaped.startsWith('"'));
		RC_ASSERT(escaped.endsWith('"'));
		if (name.contains('"')) {
			RC_ASSERT(escaped.count('"') == 2 + name.count('"'));
		}
	});

	return allPassed ? 0 : 1;
}
