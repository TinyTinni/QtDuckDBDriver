#include "../helpers/duckdb_sut.h"
#include <QCoreApplication>
#include <QTextStream>
#include <functional>
#include <rapidcheck.h>
#include <rapidcheck/state.h>
#include <vector>

struct PreparedModel {
	bool isOpen = true;
	bool prepared = false;
	QString preparedTable;
	std::map<QString, int> tableExecCount;
	std::vector<std::pair<QString, int>> insertedValues;
};

using PsCmd = rc::state::Command<PreparedModel, DuckDBSut>;

struct PrepareInsert : PsCmd {
	QString table;
	void checkPreconditions(const PreparedModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(!m.prepared);
	}
	void apply(PreparedModel &m) const override {
		m.prepared = true;
		m.preparedTable = table;
	}
	void run(const PreparedModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.prepareInsert(table)); }
	void show(std::ostream &os) const override { os << "PrepareInsert(" << table.toStdString() << ")"; }
};

struct BindAndExec : PsCmd {
	int value;
	void checkPreconditions(const PreparedModel &m) const override { RC_PRE(m.prepared); }
	void apply(PreparedModel &m) const override {
		m.tableExecCount[m.preparedTable]++;
		m.insertedValues.push_back({m.preparedTable, value});
	}
	void run(const PreparedModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.bindAndExec(value)); }
	void show(std::ostream &os) const override { os << "BindAndExec(" << value << ")"; }
};

struct ReBindAndExec : PsCmd {
	int value;
	void checkPreconditions(const PreparedModel &m) const override {
		RC_PRE(m.prepared);
		RC_PRE(m.tableExecCount.count(m.preparedTable) && m.tableExecCount.at(m.preparedTable) > 0);
	}
	void apply(PreparedModel &m) const override {
		m.tableExecCount[m.preparedTable]++;
		m.insertedValues.push_back({m.preparedTable, value});
	}
	void run(const PreparedModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.rebindAndExec(value)); }
	void show(std::ostream &os) const override { os << "ReBindAndExec(" << value << ")"; }
};

struct FinalizeStmt : PsCmd {
	void checkPreconditions(const PreparedModel &m) const override { RC_PRE(m.prepared); }
	void apply(PreparedModel &m) const override { m.prepared = false; }
	void run(const PreparedModel &, DuckDBSut &sut) const override { sut.finalizeStmt(); }
	void show(std::ostream &os) const override { os << "FinalizeStmt()"; }
};

struct CheckItemAfterExec : PsCmd {
	int value;
	void checkPreconditions(const PreparedModel &m) const override { RC_PRE(m.isOpen); }
	void apply(PreparedModel &) const override {}
	void run(const PreparedModel &m, DuckDBSut &sut) const override {
		int expected = 0;
		for (const auto &[table, val] : m.insertedValues) {
			if (table == m.preparedTable && val == value)
				expected++;
		}
		RC_ASSERT(sut.queryItemExists(m.preparedTable, value) == (expected > 0));
	}
	void show(std::ostream &os) const override { os << "CheckItemAfterExec(" << value << ")"; }
};

struct QueryAfterExecs : PsCmd {
	void checkPreconditions(const PreparedModel &m) const override { RC_PRE(m.isOpen); }
	void apply(PreparedModel &) const override {}
	void run(const PreparedModel &m, DuckDBSut &sut) const override {
		auto it = m.tableExecCount.find(m.preparedTable);
		RC_ASSERT(sut.queryCount(m.preparedTable) == (it != m.tableExecCount.end() ? it->second : 0));
	}
	void show(std::ostream &os) const override { os << "QueryAfterExecs()"; }
};

struct RePrepareSameTable : PsCmd {
	QString table;
	void checkPreconditions(const PreparedModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(!m.prepared);
	}
	void apply(PreparedModel &m) const override {
		m.prepared = true;
		m.preparedTable = table;
	}
	void run(const PreparedModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.prepareInsert(table)); }
	void show(std::ostream &os) const override { os << "RePrepareSameTable(" << table.toStdString() << ")"; }
};

struct QueryAfterReprepare : PsCmd {
	void checkPreconditions(const PreparedModel &m) const override { RC_PRE(m.isOpen); }
	void apply(PreparedModel &) const override {}
	void run(const PreparedModel &m, DuckDBSut &sut) const override {
		auto it = m.tableExecCount.find(m.preparedTable);
		RC_ASSERT(sut.queryCount(m.preparedTable) == (it != m.tableExecCount.end() ? it->second : 0));
	}
	void show(std::ostream &os) const override { os << "QueryAfterReprepare()"; }
};

using GenCmd = rc::Gen<std::shared_ptr<const PsCmd>>;

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::addLibraryPath("./plugins/");
	if (qEnvironmentVariableIsEmpty("RC_PARAMS"))
		qputenv("RC_PARAMS", "max_success=30 max_size=30");

	bool allPassed = rc::check("Prepared statement lifecycle follows model", [] {
		PreparedModel initialState;
		DuckDBSut sut;
		sut.createTable("t1");
		sut.createTable("t2");
		initialState.preparedTable = "t1";
		rc::state::check(initialState, sut, [](const PreparedModel &m) {
			static const std::vector<QString> tables{"t1", "t2"};

			std::vector<int> validTypes;
			if (!m.prepared)
				validTypes.push_back(0);
			if (m.prepared)
				validTypes.push_back(1);
			if (m.prepared) {
				auto it = m.tableExecCount.find(m.preparedTable);
				if (it != m.tableExecCount.end() && it->second > 0)
					validTypes.push_back(2);
			}
			if (m.prepared)
				validTypes.push_back(3);
			validTypes.push_back(4);
			validTypes.push_back(5);
			if (!m.prepared)
				validTypes.push_back(6);
			validTypes.push_back(7);

			RC_PRE(!validTypes.empty());

			return rc::gen::mapcat(rc::gen::elementOf(validTypes), [=](int type) -> GenCmd {
				switch (type) {
				case 0:
					return rc::gen::map(rc::gen::elementOf(tables), [](const QString &table) -> std::shared_ptr<const PsCmd> {
						auto cmd = std::make_shared<PrepareInsert>();
						cmd->table = table;
						return cmd;
					});
				case 1:
					return rc::gen::map(rc::gen::inRange(0, 10), [](int value) -> std::shared_ptr<const PsCmd> {
						auto cmd = std::make_shared<BindAndExec>();
						cmd->value = value;
						return cmd;
					});
				case 2:
					return rc::gen::map(rc::gen::inRange(0, 10), [](int value) -> std::shared_ptr<const PsCmd> {
						auto cmd = std::make_shared<ReBindAndExec>();
						cmd->value = value;
						return cmd;
					});
				case 3:
					return rc::gen::map(rc::gen::arbitrary<bool>(), [](bool) -> std::shared_ptr<const PsCmd> {
						return std::make_shared<FinalizeStmt>();
					});
				case 4:
					return rc::gen::map(rc::gen::inRange(0, 10), [](int value) -> std::shared_ptr<const PsCmd> {
						auto cmd = std::make_shared<CheckItemAfterExec>();
						cmd->value = value;
						return cmd;
					});
				case 5:
					return rc::gen::map(rc::gen::arbitrary<bool>(), [](bool) -> std::shared_ptr<const PsCmd> {
						return std::make_shared<QueryAfterExecs>();
					});
				case 6:
					return rc::gen::map(rc::gen::elementOf(tables), [](const QString &table) -> std::shared_ptr<const PsCmd> {
						auto cmd = std::make_shared<RePrepareSameTable>();
						cmd->table = table;
						return cmd;
					});
				case 7:
					return rc::gen::map(rc::gen::arbitrary<bool>(), [](bool) -> std::shared_ptr<const PsCmd> {
						return std::make_shared<QueryAfterReprepare>();
					});
				default:
					RC_FAIL("Invalid type");
				}
			});
		});
	});

	return allPassed ? 0 : 1;
}
