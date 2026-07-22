#include "../helpers/duckdb_sut.h"
#include <QCoreApplication>
#include <QMap>
#include <QTextStream>
#include <functional>
#include <rapidcheck.h>
#include <rapidcheck/state.h>
#include <vector>

struct LifecycleModel {
	bool isOpen = true;
	QMap<QString, int> tables;
	QMap<QString, QMap<int, int>> tableData;
};

using LfCmd = rc::state::Command<LifecycleModel, DuckDBSut>;

static std::vector<QString> modelTableNames(const LifecycleModel &m) {
	std::vector<QString> names;
	for (auto it = m.tables.constBegin(); it != m.tables.constEnd(); ++it)
		names.push_back(it.key());
	return names;
}

static std::vector<QString> nonEmptyTableNames(const LifecycleModel &m) {
	std::vector<QString> names;
	for (auto it = m.tables.constBegin(); it != m.tables.constEnd(); ++it) {
		if (it.value() > 0)
			names.push_back(it.key());
	}
	return names;
}

static const char namePool[] = {'a', 'b', 'c', 'd'};

struct CreateTable : LfCmd {
	QString name;
	void checkPreconditions(const LifecycleModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(!m.tables.contains(name));
	}
	void apply(LifecycleModel &m) const override {
		m.tables[name] = 0;
		m.tableData[name] = {};
	}
	void run(const LifecycleModel &, DuckDBSut &sut) const override {
		RC_ASSERT(sut.createTable(name));
		RC_ASSERT(sut.tableExists(name));
	}
	void show(std::ostream &os) const override { os << "CreateTable(" << name.toStdString() << ")"; }
};

struct InsertInto : LfCmd {
	QString name;
	int value;
	void checkPreconditions(const LifecycleModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(m.tables.contains(name));
	}
	void apply(LifecycleModel &m) const override {
		m.tables[name]++;
		m.tableData[name][value]++;
	}
	void run(const LifecycleModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.insertValue(name, value)); }
	void show(std::ostream &os) const override { os << "InsertInto(" << name.toStdString() << "," << value << ")"; }
};

struct DeleteAll : LfCmd {
	QString name;
	void checkPreconditions(const LifecycleModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(m.tables.contains(name));
		RC_PRE(m.tables[name] > 0);
	}
	void apply(LifecycleModel &m) const override {
		m.tables[name] = 0;
		m.tableData[name].clear();
	}
	void run(const LifecycleModel &, DuckDBSut &sut) const override {
		RC_ASSERT(sut.deleteAll(name));
		RC_ASSERT(sut.queryCount(name) == 0);
	}
	void show(std::ostream &os) const override { os << "DeleteAll(" << name.toStdString() << ")"; }
};

struct CheckItemExists : LfCmd {
	QString name;
	int value;
	void checkPreconditions(const LifecycleModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(m.tables.contains(name));
	}
	void apply(LifecycleModel &) const override {}
	void run(const LifecycleModel &m, DuckDBSut &sut) const override {
		int expected = m.tableData[name].value(value, 0);
		RC_ASSERT(sut.queryItemExists(name, value) == (expected > 0));
	}
	void show(std::ostream &os) const override { os << "CheckItemExists(" << name.toStdString() << "," << value << ")"; }
};

struct QueryCountCmd : LfCmd {
	QString name;
	void checkPreconditions(const LifecycleModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(m.tables.contains(name));
	}
	void apply(LifecycleModel &) const override {}
	void run(const LifecycleModel &m, DuckDBSut &sut) const override {
		RC_ASSERT(sut.queryCount(name) == m.tables[name]);
	}
	void show(std::ostream &os) const override { os << "QueryCount(" << name.toStdString() << ")"; }
};

struct DropTableCmd : LfCmd {
	QString name;
	void checkPreconditions(const LifecycleModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(m.tables.contains(name));
	}
	void apply(LifecycleModel &m) const override {
		m.tables.remove(name);
		m.tableData.remove(name);
	}
	void run(const LifecycleModel &, DuckDBSut &sut) const override {
		RC_ASSERT(sut.dropTable(name));
		RC_ASSERT(!sut.tableExists(name));
	}
	void show(std::ostream &os) const override { os << "DropTable(" << name.toStdString() << ")"; }
};

struct RecreateTableCmd : LfCmd {
	QString name;
	void checkPreconditions(const LifecycleModel &m) const override { RC_PRE(m.tables.contains(name)); }
	void apply(LifecycleModel &m) const override {
		m.tables[name] = 0;
		m.tableData[name] = {};
	}
	void run(const LifecycleModel &, DuckDBSut &sut) const override {
		RC_ASSERT(sut.dropTable(name));
		RC_ASSERT(sut.createTable(name));
		RC_ASSERT(sut.tableExists(name));
		RC_ASSERT(sut.queryCount(name) == 0);
	}
	void show(std::ostream &os) const override { os << "RecreateTable(" << name.toStdString() << ")"; }
};

struct CloseDatabaseCmd : LfCmd {
	void checkPreconditions(const LifecycleModel &m) const override { RC_PRE(m.isOpen); }
	void apply(LifecycleModel &m) const override { m.isOpen = false; }
	void run(const LifecycleModel &, DuckDBSut &sut) const override {
		sut.close();
		RC_ASSERT(!sut.isOpen());
	}
	void show(std::ostream &os) const override { os << "CloseDatabase()"; }
};

struct ReopenDatabaseCmd : LfCmd {
	void checkPreconditions(const LifecycleModel &m) const override { RC_PRE(!m.isOpen); }
	void apply(LifecycleModel &m) const override {
		m.isOpen = true;
		m.tables.clear();
		m.tableData.clear();
	}
	void run(const LifecycleModel &, DuckDBSut &sut) const override {
		RC_ASSERT(sut.reopen());
		RC_ASSERT(sut.isOpen());
	}
	void show(std::ostream &os) const override { os << "ReopenDatabase()"; }
};

using GenCmd = rc::Gen<std::shared_ptr<const LfCmd>>;

static GenCmd genCreateTable(const LifecycleModel &m) {
	std::vector<QString> available;
	for (char c : namePool) {
		QString n(c);
		if (!m.tables.contains(n))
			available.push_back(n);
	}
	return rc::gen::map(rc::gen::elementOf(available), [](const QString &name) -> std::shared_ptr<const LfCmd> {
		auto cmd = std::make_shared<CreateTable>();
		cmd->name = name;
		return cmd;
	});
}

static GenCmd genInsertInto(const LifecycleModel &m) {
	auto names = modelTableNames(m);
	return rc::gen::map(
	    rc::gen::tuple(rc::gen::elementOf(names), rc::gen::inRange(0, 10)),
	    [](const std::tuple<QString, int> &args) -> std::shared_ptr<const LfCmd> {
		    auto cmd = std::make_shared<InsertInto>();
		    cmd->name = std::get<0>(args);
		    cmd->value = std::get<1>(args);
		    return cmd;
	    });
}

static GenCmd genDeleteAll(const LifecycleModel &m) {
	auto names = nonEmptyTableNames(m);
	return rc::gen::map(rc::gen::elementOf(names), [](const QString &name) -> std::shared_ptr<const LfCmd> {
		auto cmd = std::make_shared<DeleteAll>();
		cmd->name = name;
		return cmd;
	});
}

static GenCmd genCheckItemExists(const LifecycleModel &m) {
	auto names = modelTableNames(m);
	return rc::gen::map(
	    rc::gen::tuple(rc::gen::elementOf(names), rc::gen::inRange(0, 10)),
	    [](const std::tuple<QString, int> &args) -> std::shared_ptr<const LfCmd> {
		    auto cmd = std::make_shared<CheckItemExists>();
		    cmd->name = std::get<0>(args);
		    cmd->value = std::get<1>(args);
		    return cmd;
	    });
}

static GenCmd genQueryCount(const LifecycleModel &m) {
	auto names = modelTableNames(m);
	return rc::gen::map(rc::gen::elementOf(names), [](const QString &name) -> std::shared_ptr<const LfCmd> {
		auto cmd = std::make_shared<QueryCountCmd>();
		cmd->name = name;
		return cmd;
	});
}

static GenCmd genDropTable(const LifecycleModel &m) {
	auto names = modelTableNames(m);
	return rc::gen::map(rc::gen::elementOf(names), [](const QString &name) -> std::shared_ptr<const LfCmd> {
		auto cmd = std::make_shared<DropTableCmd>();
		cmd->name = name;
		return cmd;
	});
}

static GenCmd genRecreateTable(const LifecycleModel &) {
	std::vector<QString> pool;
	for (char c : namePool)
		pool.push_back(QString(c));
	return rc::gen::map(rc::gen::elementOf(pool), [](const QString &name) -> std::shared_ptr<const LfCmd> {
		auto cmd = std::make_shared<RecreateTableCmd>();
		cmd->name = name;
		return cmd;
	});
}

static GenCmd genCloseDatabase() {
	return rc::gen::map(rc::gen::arbitrary<bool>(), [](bool) -> std::shared_ptr<const LfCmd> {
		return std::make_shared<CloseDatabaseCmd>();
	});
}

static GenCmd genReopenDatabase() {
	return rc::gen::map(rc::gen::arbitrary<bool>(), [](bool) -> std::shared_ptr<const LfCmd> {
		return std::make_shared<ReopenDatabaseCmd>();
	});
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::addLibraryPath("./plugins/");
	if (qEnvironmentVariableIsEmpty("RC_PARAMS"))
		qputenv("RC_PARAMS", "max_success=30 max_size=30");

	bool allPassed = rc::check("Database lifecycle follows model", [] {
		LifecycleModel initialState;
		DuckDBSut sut;

		rc::state::check(initialState, sut, [](const LifecycleModel &m) {
			auto names = modelTableNames(m);
			auto nonEmpty = nonEmptyTableNames(m);

			std::vector<int> validTypes;
			if (m.isOpen) {
				for (char c : namePool) {
					if (!m.tables.contains(QString(c))) {
						validTypes.push_back(0);
						break;
					}
				}
			}
			if (m.isOpen && !names.empty())
				validTypes.push_back(1);
			if (m.isOpen && !nonEmpty.empty())
				validTypes.push_back(2);
			if (m.isOpen && !names.empty())
				validTypes.push_back(3);
			if (m.isOpen && !names.empty())
				validTypes.push_back(4);
			if (m.isOpen && !names.empty())
				validTypes.push_back(5);
			if (m.isOpen && !names.empty())
				validTypes.push_back(6);
			if (m.isOpen)
				validTypes.push_back(7);
			if (!m.isOpen)
				validTypes.push_back(8);

			RC_PRE(!validTypes.empty());

			return rc::gen::mapcat(rc::gen::elementOf(validTypes), [=](int type) -> GenCmd {
				switch (type) {
				case 0:
					return genCreateTable(m);
				case 1:
					return genInsertInto(m);
				case 2:
					return genDeleteAll(m);
				case 3:
					return genCheckItemExists(m);
				case 4:
					return genQueryCount(m);
				case 5:
					return genDropTable(m);
				case 6:
					return genRecreateTable(m);
				case 7:
					return genCloseDatabase();
				case 8:
					return genReopenDatabase();
				default:
					RC_FAIL("Invalid type");
				}
			});
		});
	});

	return allPassed ? 0 : 1;
}
