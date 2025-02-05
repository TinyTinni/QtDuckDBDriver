#pragma once

#include <QSqlDriver>
#include <QSqlDriverPlugin>

namespace duckdb {
class DuckDB;
class Connection;
} // namespace duckdb

struct DuckDBConnectionHandle {
	duckdb::DuckDB *db = nullptr;
	duckdb::Connection *connection = nullptr;
};

#ifdef QT_PLUGIN
#define Q_EXPORT_SQLDRIVER_DUCKDB
#else
#define Q_EXPORT_SQLDRIVER_DUCKDB Q_SQL_EXPORT
#endif

class QDuckDBDriverPrivate;

class Q_EXPORT_SQLDRIVER_DUCKDB QDuckDBDriver : public QSqlDriver {
	Q_DECLARE_PRIVATE(QDuckDBDriver)
	friend class QDuckDBResultPrivate;

public:
	explicit QDuckDBDriver(QObject *parent = nullptr);
	~QDuckDBDriver();
	bool hasFeature(DriverFeature f) const override;
	bool open(const QString &db, const QString &user, const QString &password, const QString &host, int port,
	          const QString &connOpts) override;
	void close() override;
	QSqlResult *createResult() const override;
	bool beginTransaction() override;
	bool commitTransaction() override;
	bool rollbackTransaction() override;
	QStringList tables(QSql::TableType) const override;

	QSqlRecord record(const QString &tablename) const override;
	QSqlIndex primaryIndex(const QString &table) const override;
	/// returns a DuckDBConnectionHandle
	QVariant handle() const override;
	QString escapeIdentifier(const QString &identifier, IdentifierType) const override;
};

Q_DECLARE_METATYPE(DuckDBConnectionHandle)