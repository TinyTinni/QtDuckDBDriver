#include "../helpers/duckdb_sut.h"
#include <QCoreApplication>
#include <QMap>
#include <QTextStream>
#include <functional>
#include <rapidcheck.h>
#include <rapidcheck/state.h>
#include <vector>

struct TransactionModel {
	bool isOpen = true;
	bool inTransaction = false;
	QMap<QString, int> tables;
	QMap<QString, QMap<int, int>> committed;
	QMap<QString, QMap<int, int>> pending;
};

using TxCmd = rc::state::Command<TransactionModel, DuckDBSut>;

static std::vector<QString> modelTableNames(const TransactionModel &m) {
	std::vector<QString> names;
	for (auto it = m.tables.constBegin(); it != m.tables.constEnd(); ++it)
		names.push_back(it.key());
	return names;
}

static std::vector<QString> nonEmptyCommittedNames(const TransactionModel &m) {
	std::vector<QString> names;
	for (auto it = m.committed.constBegin(); it != m.committed.constEnd(); ++it) {
		int total = 0;
		for (auto vit = it.value().constBegin(); vit != it.value().constEnd(); ++vit)
			total += vit.value();
		if (total > 0)
			names.push_back(it.key());
	}
	return names;
}

struct CreateTableTx : TxCmd {
	QString name;
	void checkPreconditions(const TransactionModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(!m.tables.contains(name));
	}
	void apply(TransactionModel &m) const override {
		m.tables[name] = 0;
		m.committed[name] = {};
		m.pending[name] = {};
	}
	void run(const TransactionModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.createTable(name)); }
	void show(std::ostream &os) const override { os << "CreateTableTx(" << name.toStdString() << ")"; }
};

struct BeginTx : TxCmd {
	void checkPreconditions(const TransactionModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(!m.inTransaction);
	}
	void apply(TransactionModel &m) const override { m.inTransaction = true; }
	void run(const TransactionModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.beginTransaction()); }
	void show(std::ostream &os) const override { os << "BeginTx()"; }
};

struct InsertInTx : TxCmd {
	QString name;
	int value;
	void checkPreconditions(const TransactionModel &m) const override {
		RC_PRE(m.inTransaction);
		RC_PRE(m.tables.contains(name));
	}
	void apply(TransactionModel &m) const override { m.pending[name][value]++; }
	void run(const TransactionModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.insertValue(name, value)); }
	void show(std::ostream &os) const override { os << "InsertInTx(" << name.toStdString() << "," << value << ")"; }
};

struct CommitTx : TxCmd {
	void checkPreconditions(const TransactionModel &m) const override { RC_PRE(m.inTransaction); }
	void apply(TransactionModel &m) const override {
		for (auto it = m.pending.constBegin(); it != m.pending.constEnd(); ++it) {
			for (auto vit = it.value().constBegin(); vit != it.value().constEnd(); ++vit)
				m.committed[it.key()][vit.key()] += vit.value();
		}
		m.pending.clear();
		m.inTransaction = false;
	}
	void run(const TransactionModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.commitTransaction()); }
	void show(std::ostream &os) const override { os << "CommitTx()"; }
};

struct RollbackTx : TxCmd {
	void checkPreconditions(const TransactionModel &m) const override { RC_PRE(m.inTransaction); }
	void apply(TransactionModel &m) const override {
		m.pending.clear();
		m.inTransaction = false;
	}
	void run(const TransactionModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.rollbackTransaction()); }
	void show(std::ostream &os) const override { os << "RollbackTx()"; }
};

struct InsertDirect : TxCmd {
	QString name;
	int value;
	void checkPreconditions(const TransactionModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(!m.inTransaction);
		RC_PRE(m.tables.contains(name));
	}
	void apply(TransactionModel &m) const override { m.committed[name][value]++; }
	void run(const TransactionModel &, DuckDBSut &sut) const override { RC_ASSERT(sut.insertValue(name, value)); }
	void show(std::ostream &os) const override { os << "InsertDirect(" << name.toStdString() << "," << value << ")"; }
};

struct DeleteDirect : TxCmd {
	QString name;
	void checkPreconditions(const TransactionModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(!m.inTransaction);
		RC_PRE(m.tables.contains(name));
		int total = 0;
		for (auto it = m.committed[name].constBegin(); it != m.committed[name].constEnd(); ++it)
			total += it.value();
		RC_PRE(total > 0);
	}
	void apply(TransactionModel &m) const override { m.committed[name].clear(); }
	void run(const TransactionModel &, DuckDBSut &sut) const override {
		RC_ASSERT(sut.deleteAll(name));
		RC_ASSERT(sut.queryCount(name) == 0);
	}
	void show(std::ostream &os) const override { os << "DeleteDirect(" << name.toStdString() << ")"; }
};

struct CheckItemInTx : TxCmd {
	QString name;
	int value;
	void checkPreconditions(const TransactionModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(m.tables.contains(name));
	}
	void apply(TransactionModel &) const override {}
	void run(const TransactionModel &m, DuckDBSut &sut) const override {
		int committedCount = m.committed[name].value(value, 0);
		int pendingCount = m.pending[name].value(value, 0);
		RC_ASSERT(sut.queryItemExists(name, value) == ((committedCount + pendingCount) > 0));
	}
	void show(std::ostream &os) const override { os << "CheckItemInTx(" << name.toStdString() << "," << value << ")"; }
};

struct QueryCommittedCount : TxCmd {
	QString name;
	void checkPreconditions(const TransactionModel &m) const override {
		RC_PRE(m.isOpen);
		RC_PRE(m.tables.contains(name));
	}
	void apply(TransactionModel &) const override {}
	void run(const TransactionModel &m, DuckDBSut &sut) const override {
		int expected = 0;
		for (auto it = m.committed[name].constBegin(); it != m.committed[name].constEnd(); ++it)
			expected += it.value();
		for (auto it = m.pending[name].constBegin(); it != m.pending[name].constEnd(); ++it)
			expected += it.value();
		RC_ASSERT(sut.queryCount(name) == expected);
	}
	void show(std::ostream &os) const override { os << "QueryCommittedCount(" << name.toStdString() << ")"; }
};

using GenCmd = rc::Gen<std::shared_ptr<const TxCmd>>;

static const char namePool[] = {'a', 'b', 'c', 'd'};

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::addLibraryPath("./plugins/");
	if (qEnvironmentVariableIsEmpty("RC_PARAMS"))
		qputenv("RC_PARAMS", "max_success=30 max_size=30");

	bool allPassed = rc::check("Transaction isolation follows model", [] {
		TransactionModel initialState;
		DuckDBSut sut;
		rc::state::check(initialState, sut, [](const TransactionModel &m) {
			auto names = modelTableNames(m);
			auto nonEmpty = nonEmptyCommittedNames(m);

			std::vector<int> validTypes;
			if (m.isOpen && !m.inTransaction) {
				std::vector<QString> avail;
				for (char c : namePool) {
					QString n(c);
					if (!m.tables.contains(n))
						avail.push_back(n);
				}
				if (!avail.empty())
					validTypes.push_back(0);
			}
			if (m.isOpen && !m.inTransaction)
				validTypes.push_back(1);
			if (m.inTransaction && !names.empty())
				validTypes.push_back(2);
			if (m.inTransaction)
				validTypes.push_back(3);
			if (m.inTransaction)
				validTypes.push_back(4);
			if (m.isOpen && !m.inTransaction && !names.empty())
				validTypes.push_back(5);
			if (m.isOpen && !m.inTransaction && !nonEmpty.empty())
				validTypes.push_back(6);
			if (m.isOpen && !names.empty())
				validTypes.push_back(7);
			if (m.isOpen && !names.empty())
				validTypes.push_back(8);

			RC_PRE(!validTypes.empty());

			return rc::gen::mapcat(rc::gen::elementOf(validTypes), [=](int type) -> GenCmd {
				switch (type) {
				case 0: {
					std::vector<QString> avail;
					for (char c : namePool) {
						QString n(c);
						if (!m.tables.contains(n))
							avail.push_back(n);
					}
					return rc::gen::map(rc::gen::elementOf(avail), [](const QString &name) -> std::shared_ptr<const TxCmd> {
						auto cmd = std::make_shared<CreateTableTx>();
						cmd->name = name;
						return cmd;
					});
				}
				case 1:
					return rc::gen::map(rc::gen::arbitrary<bool>(), [](bool) -> std::shared_ptr<const TxCmd> {
						return std::make_shared<BeginTx>();
					});
				case 2:
					return rc::gen::map(
					    rc::gen::tuple(rc::gen::elementOf(names), rc::gen::inRange(0, 10)),
					    [](const std::tuple<QString, int> &args) -> std::shared_ptr<const TxCmd> {
						    auto cmd = std::make_shared<InsertInTx>();
						    cmd->name = std::get<0>(args);
						    cmd->value = std::get<1>(args);
						    return cmd;
					    });
				case 3:
					return rc::gen::map(rc::gen::arbitrary<bool>(), [](bool) -> std::shared_ptr<const TxCmd> {
						return std::make_shared<CommitTx>();
					});
				case 4:
					return rc::gen::map(rc::gen::arbitrary<bool>(), [](bool) -> std::shared_ptr<const TxCmd> {
						return std::make_shared<RollbackTx>();
					});
				case 5:
					return rc::gen::map(
					    rc::gen::tuple(rc::gen::elementOf(names), rc::gen::inRange(0, 10)),
					    [](const std::tuple<QString, int> &args) -> std::shared_ptr<const TxCmd> {
						    auto cmd = std::make_shared<InsertDirect>();
						    cmd->name = std::get<0>(args);
						    cmd->value = std::get<1>(args);
						    return cmd;
					    });
				case 6:
					return rc::gen::map(rc::gen::elementOf(nonEmpty), [](const QString &name) -> std::shared_ptr<const TxCmd> {
						auto cmd = std::make_shared<DeleteDirect>();
						cmd->name = name;
						return cmd;
					});
				case 7:
					return rc::gen::map(
					    rc::gen::tuple(rc::gen::elementOf(names), rc::gen::inRange(0, 10)),
					    [](const std::tuple<QString, int> &args) -> std::shared_ptr<const TxCmd> {
						    auto cmd = std::make_shared<CheckItemInTx>();
						    cmd->name = std::get<0>(args);
						    cmd->value = std::get<1>(args);
						    return cmd;
					    });
				case 8:
					return rc::gen::map(rc::gen::elementOf(names), [](const QString &name) -> std::shared_ptr<const TxCmd> {
						auto cmd = std::make_shared<QueryCommittedCount>();
						cmd->name = name;
						return cmd;
					});
				default:
					RC_FAIL("Invalid type");
				}
			});
		});
	});

	return allPassed ? 0 : 1;
}
