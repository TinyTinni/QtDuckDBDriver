#include "QtDuckDBDriver.h"

#include "Qt5Compat.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QList>
#include <QScopedValueRollback>
#include <QSqlError>
#include <QSqlField>
#include <QSqlIndex>
#include <QSqlQuery>
#include <QVariant>
#include <duckdb.hpp>
#include <duckdb/parser/parser.hpp>
#include <private/qsqlcachedresult_p.h>
#include <private/qsqldriver_p.h>

struct DbHandle {
	duckdb::unique_ptr<duckdb::DuckDB> db;
	duckdb::unique_ptr<duckdb::Connection> con;
};

struct DuckDBStmt {
	duckdb::shared_ptr<duckdb::ClientContext> context;
	//! The prepared statement object, if successfully prepared
	duckdb::unique_ptr<duckdb::PreparedStatement> prepared;
	//! The result object, if successfully executed
	duckdb::unique_ptr<duckdb::QueryResult> result;
	//! The current chunk that we are iterating over
	duckdb::unique_ptr<duckdb::DataChunk> current_chunk;
	//! The current row into the current chunk that we are iterating over
	int64_t current_row;
	//! Bound values, used for binding to the prepared statement
	duckdb::vector<duckdb::Value> bound_values;
	int64_t last_changes = 0;
};

static QString _q_escapeIdentifier(const QString &identifier, QSqlDriver::IdentifierType type) {
	QString res = identifier;
	// If it contains [ and ] then we assume it to be escaped properly already as this indicates
	// the syntax is exactly how it should be
	if (identifier.contains(u'[') && identifier.contains(u']'))
		return res;
	if (!identifier.isEmpty() && !identifier.startsWith(u'"') && !identifier.endsWith(u'"')) {
		res.replace(u'"', "\"\""_L1);
		res.prepend(u'"').append(u'"');
		if (type == QSqlDriver::TableName)
			res.replace(u'.', "\".\""_L1);
	}
	return res;
}

static QMetaType::Type qGetColumnType(const QString &tpName) {
	const QString typeName = tpName.toLower();

	if (typeName == "integer"_L1 || typeName == "int"_L1)
		return QMetaType::Int;
	if (typeName == "double"_L1 || typeName == "float"_L1 || typeName == "real"_L1 || typeName.startsWith("numeric"_L1))
		return QMetaType::Double;
	if (typeName == "blob"_L1)
		return QMetaType::QByteArray;
	if (typeName == "boolean"_L1 || typeName == "bool"_L1)
		return QMetaType::Bool;
	return QMetaType::QString;
}

static QSqlError qMakeError(duckdb::ErrorData &errData, const QString &descr, QSqlError::ErrorType type) {
	return QSqlError(descr, QString::fromStdString(errData.Message()), type,
	                 QString::fromStdString(duckdb::Exception::ExceptionTypeToString(errData.Type())));
}

class QDuckDBResultPrivate;

class QDuckDBResult : public QSqlCachedResult {
	Q_DECLARE_PRIVATE(QDuckDBResult)
	friend class QDuckDBDriver;

public:
	explicit QDuckDBResult(const QDuckDBDriver *db);
	~QDuckDBResult();

protected:
	bool gotoNext(QSqlCachedResult::ValueCache &row, int idx) override;
	bool reset(const QString &query) override;
	bool prepare(const QString &query) override;
	bool execBatch(bool arrayBind) override;
	bool exec() override;
	int size() override;
	int numRowsAffected() override;
	QVariant lastInsertId() const override;
	QSqlRecord record() const override;
	void detachFromResultSet() override;
};

class QDuckDBDriverPrivate : public QSqlDriverPrivate {
	Q_DECLARE_PUBLIC(QDuckDBDriver)

public:
	inline QDuckDBDriverPrivate() : QSqlDriverPrivate(QSqlDriver::SQLite) {
	}
	duckdb::unique_ptr<DbHandle> access = nullptr;
	QList<QDuckDBResult *> results;
};

class QDuckDBResultPrivate : public QSqlCachedResultPrivate {
	Q_DECLARE_PUBLIC(QDuckDBResult)

public:
	Q_DECLARE_SQLDRIVER_PRIVATE(QDuckDBDriver)
	using QSqlCachedResultPrivate::QSqlCachedResultPrivate;
	void cleanup();
	bool fetchNext(QSqlCachedResult::ValueCache &values, int idx, bool initialFetch);
	// initializes the recordInfo and the cache
	void initColumns(bool emptyResultset);
	void finalize();

	std::unique_ptr<DuckDBStmt> stmt = nullptr;
	QSqlRecord rInf;
	QSqlCachedResult::ValueCache firstRow;
	bool skippedStatus = false; // the status of the fetchNext() that's skipped
	bool skipRow = false;       // skip the next fetchNext()?
};

void QDuckDBResultPrivate::cleanup() {
	Q_Q(QDuckDBResult);
	finalize();
	rInf.clear();
	skippedStatus = false;
	skipRow = false;
	q->setAt(QSql::BeforeFirstRow);
	q->setActive(false);
	q->cleanup();
}

void QDuckDBResultPrivate::finalize() {
	if (!stmt)
		return;

	stmt.reset();
}

QMetaType::Type duckdbTypeToQtType(const duckdb::LogicalType &type) {
	switch (type.id()) {
	case duckdb::LogicalTypeId::BOOLEAN:
		return QMetaType::Bool;
	case duckdb::LogicalTypeId::TINYINT:
		return QMetaType::Short;
	case duckdb::LogicalTypeId::SMALLINT:
		return QMetaType::Short;
	case duckdb::LogicalTypeId::INTEGER:
		return QMetaType::Int;
	case duckdb::LogicalTypeId::BIGINT:
		return QMetaType::Long;
	case duckdb::LogicalTypeId::FLOAT:
		return QMetaType::Float;
	case duckdb::LogicalTypeId::DOUBLE:
		return QMetaType::Double;
	case duckdb::LogicalTypeId::BLOB:
		return QMetaType::QByteArray;
	case duckdb::LogicalTypeId::DECIMAL:
	case duckdb::LogicalTypeId::DATE:
	case duckdb::LogicalTypeId::TIME:
	case duckdb::LogicalTypeId::TIMESTAMP:
	case duckdb::LogicalTypeId::TIMESTAMP_NS:
	case duckdb::LogicalTypeId::TIMESTAMP_MS:
	case duckdb::LogicalTypeId::TIMESTAMP_SEC:
	case duckdb::LogicalTypeId::VARCHAR:
	case duckdb::LogicalTypeId::LIST:
	case duckdb::LogicalTypeId::MAP:
	case duckdb::LogicalTypeId::STRUCT:
	default:
		return QMetaType::QString;
	}
}

void QDuckDBResultPrivate::initColumns(bool emptyResultset) {
	Q_Q(QDuckDBResult);
	if (!stmt || !stmt->prepared)
		return;

	duckdb::idx_t nCols = stmt->prepared->ColumnCount();
	if (nCols <= 0)
		return;

	q->init(nCols);

	const auto &columnNamesVec = stmt->prepared->GetNames();
	const auto &columnTypesVec = stmt->prepared->GetTypes();

	for (duckdb::idx_t i = 0; i < nCols; ++i) {
		QString colName = QString::fromStdString(columnNamesVec[i]).remove(u'"');
		auto fieldType = duckdbTypeToQtType(columnTypesVec[i]);

		QSqlField fld(colName, toQtType(fieldType));
		fld.setSqlType(fieldType);
		rInf.append(fld);
	}
}

///////////////////////

bool QDuckDBResultPrivate::fetchNext(QSqlCachedResult::ValueCache &values, int idx, bool initialFetch) {
	Q_Q(QDuckDBResult);

	if (skipRow) {
		// already fetched
		Q_ASSERT(!initialFetch);
		skipRow = false;
		for (int i = 0; i < firstRow.size(); i++)
			values[i] = firstRow[i];
		return skippedStatus;
	}
	skipRow = initialFetch;

	if (initialFetch) {
		firstRow.clear();
		if (stmt && stmt->prepared)
			firstRow.resize(stmt->prepared->ColumnCount());
	}

	if (!stmt || !stmt->context) {
		q->setLastError(QSqlError(QCoreApplication::translate("QDuckDbResult", "Unable to fetch row"),
		                          QCoreApplication::translate("QDuckDbResult", "No query"),
		                          QSqlError::ConnectionError));
		q->setAt(QSql::AfterLastRow);
		return false;
	}
	if (!stmt->prepared) {
		q->setLastError(
		    QSqlError(QCoreApplication::translate("QDuckDbResult", "Unable to fetch row"),
		              QCoreApplication::translate("QDuckDbResult",
		                                          "Attempting fetchNext() on a non-successfully prepared statement."),
		              QSqlError::ConnectionError));
		q->setAt(QSql::AfterLastRow);
		return false;
	}

	auto buildError = [this, q](duckdb::ErrorData &errData) {
		stmt->result = nullptr;
		stmt->current_chunk = nullptr;
		q->setLastError(qMakeError(errData, "Unable to fetch row.", QSqlError::ConnectionError));
		q->setAt(QSql::AfterLastRow);
	};

	auto fetchNext = [&]() {
		duckdb::ErrorData errData;
		if (!stmt->result->TryFetch(stmt->current_chunk, errData)) {
			buildError(errData);
			return false;
		}
		return true;
	};

	auto isDone = [&]() {
		if (!stmt->current_chunk || stmt->current_chunk->size() == 0) {
			stmt->result = nullptr;
			stmt->current_chunk = nullptr;
			if (rInf.isEmpty())
				initColumns(true);
			q->setAt(QSql::AfterLastRow);
			return true;
		}
		return false;
	};

	auto executeQuery = [&]() {
		stmt->result = stmt->prepared->Execute(stmt->bound_values, true);
		if (stmt->result->HasError()) {
			// error in execute: clear prepared statement
			buildError(stmt->result->GetErrorObject());
			return false;
		}
		return true;
	};

	auto collectStats = [&]() {
		auto properties = stmt->prepared->GetStatementProperties();
		if (properties.return_type == duckdb::StatementReturnType::CHANGED_ROWS && stmt->current_chunk &&
		    stmt->current_chunk->size() > 0) {
			// update total changes
			auto row_changes = stmt->current_chunk->GetValue(0, 0);
			if (!row_changes.IsNull() && row_changes.DefaultTryCastAs(duckdb::LogicalType::BIGINT)) {
				stmt->last_changes = row_changes.GetValue<int64_t>();
			}
		}
		if (properties.return_type != duckdb::StatementReturnType::QUERY_RESULT) {
			stmt->current_chunk.reset();
			stmt->result.reset();
		}
	};

	auto fillRow = [&]() {
		//// check to see if should fill out columns
		if (rInf.isEmpty())
			// must be first call.
			initColumns(false);
		if (idx < 0 && !initialFetch)
			return true;
		for (int i = 0; i < rInf.count(); ++i) {

			duckdb::Value val = stmt->current_chunk->data[i].GetValue(stmt->current_row);
			switch (val.type().id()) {
			case duckdb::LogicalTypeId::BLOB: {
				val = val.CastAs(*stmt->context, duckdb::LogicalType::BLOB);
				const auto &str = duckdb::StringValue::Get(val);
				values[i + idx] = QByteArray(str.data(), str.size());
				break;
			}
			case duckdb::LogicalTypeId::BOOLEAN:
			case duckdb::LogicalTypeId::TINYINT:
			case duckdb::LogicalTypeId::SMALLINT:
			case duckdb::LogicalTypeId::INTEGER:
			case duckdb::LogicalTypeId::BIGINT:
				val = val.CastAs(*stmt->context, duckdb::LogicalType::INTEGER);
				values[i + idx] = duckdb::IntegerValue::Get(val);
				break;
			case duckdb::LogicalTypeId::FLOAT:
			case duckdb::LogicalTypeId::DOUBLE:
			case duckdb::LogicalTypeId::DECIMAL:
				switch (q->numericalPrecisionPolicy()) {
				case QSql::LowPrecisionInt32:
					val = val.CastAs(*stmt->context, duckdb::LogicalType::INTEGER);
					values[i + idx] = duckdb::IntegerValue::Get(val);
					break;
				case QSql::LowPrecisionInt64:
					val = val.CastAs(*stmt->context, duckdb::LogicalType::BIGINT);
					values[i + idx] = QVariant((qint64)duckdb::BigIntValue::Get(val));
					break;
				case QSql::LowPrecisionDouble:
				case QSql::HighPrecision:
				default:
					val = val.CastAs(*stmt->context, duckdb::LogicalType::DOUBLE);
					values[i + idx] = duckdb::DoubleValue::Get(val);
					break;
				};
				break;
			default:
				if (!val.IsNull() && val.TryCastAs(*stmt->context, duckdb::LogicalType::VARCHAR)) {
					values[i + idx] = QString::fromStdString(duckdb::StringValue::Get(val));
				} else {
					values[i + idx] = QVariant::fromValue(QString());
				}
				break;
			}
		}
		return true;
	};

	///////////////
	if (!stmt->result) {
		// no result yet! call Execute()
		if (!executeQuery()) {
			return false;
		}
		// fetch a chunk
		if (!fetchNext()) {
			return false;
		}

		stmt->current_row = -1;
		collectStats();
	}
	if (isDone()) {
		return false;
	}
	stmt->current_row++;
	if (stmt->current_row >= (int32_t)stmt->current_chunk->size()) {
		// have to fetch again!
		stmt->current_row = 0;
		if (!fetchNext()) {
			return false;
		}
		if (isDone()) {
			return false;
		}
	}
	return fillRow();
}

QDuckDBResult::QDuckDBResult(const QDuckDBDriver *db) : QSqlCachedResult(*new QDuckDBResultPrivate(this, db)) {
	Q_D(QDuckDBResult);
	const_cast<QDuckDBDriverPrivate *>(d->drv_d_func())->results.append(this);
}

QDuckDBResult::~QDuckDBResult() {
	Q_D(QDuckDBResult);
	if (d->drv_d_func())
		const_cast<QDuckDBDriverPrivate *>(d->drv_d_func())->results.removeOne(this);
	d->cleanup();
}

bool QDuckDBResult::reset(const QString &query) {
	if (!prepare(query))
		return false;
	return exec();
}

bool QDuckDBResult::prepare(const QString &query) {
	Q_D(QDuckDBResult);
	if (!driver() || !driver()->isOpen() || driver()->isOpenError())
		return false;

	d->cleanup();

	setSelect(false);

	const auto &query_str = query.toStdString();
	auto &&db = d->drv_d_func()->access;

	auto build_error = [this, d](duckdb::ErrorData &errData) {
		setLastError(qMakeError(errData, QCoreApplication::translate("QDuckDBResult", "Unable to execute statement"),
		                        QSqlError::StatementError));
		d->finalize();
	};

	if (!db) {
		return false;
	}
	try {
		duckdb::Parser parser(db->con->context->GetParserOptions());
		parser.ParseQuery(query_str);
		if (parser.statements.size() == 0) {
			return true;
		}
		// extract the remainder
		idx_t next_location = parser.statements[0]->stmt_location + parser.statements[0]->stmt_length;
		// extract the remainder of the query
		if (next_location < query_str.size() && !QString(query_str.data() + next_location + 1).trimmed().isEmpty()) {
			setLastError(QSqlError(
			    QCoreApplication::translate("QDuckDbResult", "Unable to fetch row"),
			    QCoreApplication::translate("QDuckDbResult", "Unable to execute multiple statements at a time")));
			d->finalize();
			return false;
		}

		// extract the first statement
		duckdb::vector<duckdb::unique_ptr<duckdb::SQLStatement>> statements;
		statements.push_back(std::move(parser.statements[0]));

		db->con->context->HandlePragmaStatements(statements);
		if (statements.empty()) {
			return true;
		}

		// if there are multiple statements here, we are dealing with an import database statement
		// we directly execute all statements besides the final one
		for (idx_t i = 0; i + 1 < statements.size(); i++) {
			auto res = db->con->Query(std::move(statements[i]));
			if (res->HasError()) {
				build_error(res->GetErrorObject());
				return false;
			}
		}

		// now prepare the query
		auto prepared = db->con->Prepare(std::move(statements.back()));
		if (prepared->HasError()) {
			// failed to prepare: set the error message
			build_error(prepared->error);
			return false;
		}

		// create the statement entry
		d->stmt = duckdb::make_uniq<DuckDBStmt>();
		d->stmt->context = db->con->context;
		d->stmt->prepared = std::move(prepared);
		d->stmt->current_row = -1;
		d->stmt->bound_values.resize(d->stmt->prepared->n_param);

		return true;
	} catch (std::exception &ex) {
		auto errData = duckdb::ErrorData(ex);
		db->con->context->ProcessError(errData, query_str);
		build_error(errData);
		return false;
	}
	return false;
}

bool QDuckDBResult::execBatch(bool arrayBind) {
	Q_UNUSED(arrayBind);
	Q_D(QSqlResult);
	QScopedValueRollback<QSqlCachedResult::ValueCache> valuesScope(d->values);
	auto values = d->values;
	if (values.size() == 0)
		return false;

	for (int i = 0; i < values.at(0).toList().size(); ++i) {
		d->values.clear();
		QScopedValueRollback<QSqlResultPrivate::IndexMap> indexesScope(d->indexes);
		auto it = d->indexes.constBegin();
		while (it != d->indexes.constEnd()) {
			bindValue(it.key(), values.at(it.value().first()).toList().at(i), QSql::In);
			++it;
		}
		if (!exec())
			return false;
	}
	return true;
}

bool QDuckDBResult::exec() {
	Q_D(QDuckDBResult);
	auto values = boundValues();

	if (!d->stmt)
		return false;

	d->skippedStatus = false;
	d->skipRow = false;
	d->rInf.clear();
	clearValues();
	setLastError(QSqlError());

	d->stmt->result.reset();
	d->stmt->current_chunk.reset();

	int paramCount = d->stmt->prepared->n_param;
	if (paramCount != values.size()) {
		setLastError(QSqlError(QCoreApplication::translate("QDuckDBResult", "Parameter count mismatch"), QString(),
		                       QSqlError::StatementError));
		return false;
	}

	for (int i = 0; i < paramCount; ++i) {
		const QVariant &value = values.at(i);
		if (value.isNull()) {
			d->stmt->bound_values[i] = duckdb::Value();
		} else {
			switch (value.userType()) {
			case QMetaType::QByteArray: {
				const QByteArray *ba = static_cast<const QByteArray *>(value.constData());
				d->stmt->bound_values[i] = duckdb::Value::BLOB(ba->toStdString());
				break;
			}
			case QMetaType::Int:
			case QMetaType::Bool:
				d->stmt->bound_values[i] = duckdb::Value::INTEGER(value.toInt());
				break;
			case QMetaType::Double:
				d->stmt->bound_values[i] = duckdb::Value::DOUBLE(value.toInt());
				break;
			case QMetaType::UInt:
			case QMetaType::LongLong:
				d->stmt->bound_values[i] = duckdb::Value::BIGINT(value.toInt());
				break;
			case QMetaType::QDateTime: {
				const QDateTime dateTime = value.toDateTime();
				const QString str = dateTime.toString(Qt::ISODateWithMs);
				d->stmt->bound_values[i] = duckdb::Value(str.toStdString());
				break;
			}
			case QMetaType::QTime: {
				const QTime time = value.toTime();
				const QString str = time.toString(u"hh:mm:ss.zzz");
				d->stmt->bound_values[i] = duckdb::Value(str.toStdString());
				break;
			}
			case QMetaType::QString: {
				const QString *str = static_cast<const QString *>(value.constData());
				d->stmt->bound_values[i] = duckdb::Value(str->toUtf8().toStdString());
				break;
			}
			default: {
				const QString str = value.toString();
				d->stmt->bound_values[i] = duckdb::Value(str.toStdString());
				break;
			}
			}
		}
	}

	d->skippedStatus = d->fetchNext(d->firstRow, 0, true);
	if (lastError().isValid()) {
		setSelect(false);
		setActive(false);
		return false;
	}
	setSelect(!d->rInf.isEmpty());
	setActive(true);
	return true;
}

bool QDuckDBResult::gotoNext(QSqlCachedResult::ValueCache &row, int idx) {
	Q_D(QDuckDBResult);
	return d->fetchNext(row, idx, false);
}

int QDuckDBResult::size() {
	Q_D(QDuckDBResult);
	return 0;
}

int QDuckDBResult::numRowsAffected() {
	Q_D(const QDuckDBResult);
	if (!d->stmt)
		return -1;

	return d->stmt->last_changes;
}

QVariant QDuckDBResult::lastInsertId() const {
	Q_D(const QDuckDBResult);
	return QVariant();
}

QSqlRecord QDuckDBResult::record() const {
	Q_D(const QDuckDBResult);
	if (!isActive() || !isSelect())
		return QSqlRecord();
	return d->rInf;
}

void QDuckDBResult::detachFromResultSet() {
	Q_D(QDuckDBResult);
	if (d->stmt) {
		d->stmt->result.reset();
		d->stmt->current_chunk.reset();
	}
}

/////////////////////////////////////////////////////////

QDuckDBDriver::QDuckDBDriver(QObject *parent) : QSqlDriver(*new QDuckDBDriverPrivate, parent) {
}

QDuckDBDriver::~QDuckDBDriver() {
	QDuckDBDriver::close();
}

bool QDuckDBDriver::hasFeature(DriverFeature f) const {
	switch (f) {
	case BLOB:
	case Transactions:
	case Unicode:
	case PreparedQueries:
	case PositionalPlaceholders:
	case SimpleLocking:
	case FinishQuery:
	case LowPrecisionNumbers:
		return true;
	case QuerySize:
	case LastInsertId:
	case NamedPlaceholders:
	case EventNotifications:
	case BatchOperations:
	case MultipleResultSets:
	case CancelQuery:
		return false;
	}
	return false;
}

bool QDuckDBDriver::open(const QString &db, const QString &, const QString &, const QString &, int,
                         const QString &conOpts) {
	Q_D(QDuckDBDriver);
	if (isOpen())
		close();

	bool openReadOnlyOption = false;
	for (const auto &option : conOpts.split(u';')) {
		if (option.trimmed() == "READONLY"_L1) {
			openReadOnlyOption = true;
		}
	}

	try {
		d->access = duckdb::make_uniq<DbHandle>();
		duckdb::DBConfig config;
		config.options.access_mode = duckdb::AccessMode::AUTOMATIC;
		if (openReadOnlyOption) {
			config.options.access_mode = duckdb::AccessMode::READ_ONLY;
		}
		d->access->db = duckdb::make_uniq<duckdb::DuckDB>(db.toStdString(), &config);
		d->access->con = duckdb::make_uniq<duckdb::Connection>(*d->access->db);
	} catch (std::exception &ex) {
		if (d->access) {
			auto errData = duckdb::ErrorData(ex);
			setLastError(qMakeError(errData, tr("Error opening database"), QSqlError::ConnectionError));
			setOpenError(true);

			d->access.reset();
			return false;
		}
	}

	setOpen(true);
	setOpenError(false);
	return true;
}

void QDuckDBDriver::close() {
	Q_D(QDuckDBDriver);
	if (isOpen()) {
		for (QDuckDBResult *result : qtAsConst(d->results)) {
			result->d_func()->finalize();
		}

		d->access.reset();
		setOpen(false);
		setOpenError(false);
	}
}

QSqlResult *QDuckDBDriver::createResult() const {
	return new QDuckDBResult(this);
}

bool QDuckDBDriver::beginTransaction() {
	if (!isOpen() || isOpenError())
		return false;

	QSqlQuery q(createResult());
	if (!q.exec("BEGIN"_L1)) {
		setLastError(
		    QSqlError(tr("Unable to begin transaction"), q.lastError().databaseText(), QSqlError::TransactionError));
		return false;
	}

	return true;
}

bool QDuckDBDriver::commitTransaction() {
	if (!isOpen() || isOpenError())
		return false;

	QSqlQuery q(createResult());
	if (!q.exec("COMMIT"_L1)) {
		setLastError(
		    QSqlError(tr("Unable to commit transaction"), q.lastError().databaseText(), QSqlError::TransactionError));
		return false;
	}

	return true;
}

bool QDuckDBDriver::rollbackTransaction() {
	if (!isOpen() || isOpenError())
		return false;

	QSqlQuery q(createResult());
	if (!q.exec("ROLLBACK"_L1)) {
		setLastError(
		    QSqlError(tr("Unable to rollback transaction"), q.lastError().databaseText(), QSqlError::TransactionError));
		return false;
	}

	return true;
}

QStringList QDuckDBDriver::tables(QSql::TableType type) const {
	QStringList res;
	if (!isOpen())
		return res;

	QSqlQuery q(createResult());
	q.setForwardOnly(true);

	QString system_tables = (type & QSql::SystemTables) ? "" : "WHERE internal = 'FALSE'";

	if (type & QSql::Tables) {
		QString table_sql = QString("SELECT table_name FROM duckdb_tables() %1;").arg(system_tables);
		if (!table_sql.isEmpty() && q.exec(table_sql)) {
			while (q.next())
				res.append(q.value(0).toString());
		}
	}

	if (type & QSql::Views) {
		QString view_sql = QString("SELECT view_name FROM duckdb_views() %1;").arg(system_tables);
		if (!view_sql.isEmpty() && q.exec(view_sql)) {
			while (q.next())
				res.append(q.value(0).toString());
		}
	}

	return res;
}

static QSqlIndex qGetTableInfo(QSqlQuery &q, const QString &tableName, bool onlyPIndex = false) {
	QString schema;
	QString table(tableName);
	const qsizetype indexOfSeparator = tableName.indexOf(u'.');
	if (indexOfSeparator > -1) {
		const qsizetype indexOfCloseBracket = tableName.indexOf(u']');
		if (indexOfCloseBracket != tableName.size() - 1) {
			// Handles a case like databaseName.tableName
			schema = tableName.left(indexOfSeparator + 1);
			table = tableName.mid(indexOfSeparator + 1);
		} else {
			const qsizetype indexOfOpenBracket = tableName.lastIndexOf(u'[', indexOfCloseBracket);
			if (indexOfOpenBracket > 0) {
				// Handles a case like databaseName.[tableName]
				schema = tableName.left(indexOfOpenBracket);
				table = tableName.mid(indexOfOpenBracket);
			}
		}
	}

	q.exec("PRAGMA "_L1 + schema + "table_info ("_L1 + _q_escapeIdentifier(table, QSqlDriver::TableName) + u')');
	QSqlIndex ind;
	while (q.next()) {
		bool isPk = q.value(5).toInt();
		if (onlyPIndex && !isPk)
			continue;
		QString typeName = q.value(2).toString().toLower();
		QString defVal = q.value(4).toString();
		if (!defVal.isEmpty() && defVal.at(0) == u'\'') {
			const int end = defVal.lastIndexOf(u'\'');
			if (end > 0)
				defVal = defVal.mid(1, end - 1);
		}

		QSqlField fld(q.value(1).toString(), toQtType(qGetColumnType(typeName)), tableName);
		if (isPk && (typeName == "integer"_L1))
			// INTEGER PRIMARY KEY fields are auto-generated in sqlite
			// INT PRIMARY KEY is not the same as INTEGER PRIMARY KEY!
			fld.setAutoValue(true);
		fld.setRequired(q.value(3).toInt() != 0);
		fld.setDefaultValue(defVal);
		ind.append(fld);
	}
	return ind;
}

QSqlIndex QDuckDBDriver::primaryIndex(const QString &tblname) const {
	if (!isOpen())
		return QSqlIndex();

	QString table = tblname;
	if (isIdentifierEscaped(table, QSqlDriver::TableName))
		table = stripDelimiters(table, QSqlDriver::TableName);

	QSqlQuery q(createResult());
	q.setForwardOnly(true);
	return qGetTableInfo(q, table, true);
}

QSqlRecord QDuckDBDriver::record(const QString &tbl) const {
	if (!isOpen())
		return QSqlRecord();

	QString table = tbl;
	if (isIdentifierEscaped(table, QSqlDriver::TableName))
		table = stripDelimiters(table, QSqlDriver::TableName);

	QSqlQuery q(createResult());
	q.setForwardOnly(true);
	return qGetTableInfo(q, table);
}

QVariant QDuckDBDriver::handle() const {
	Q_D(const QDuckDBDriver);
	if (!d->access) {
		return QVariant::fromValue(DuckDBConnectionHandle {});
	}

	DuckDBConnectionHandle handle {d->access->db.get(), d->access->con.get()};
	return QVariant::fromValue(handle);
}

QString QDuckDBDriver::escapeIdentifier(const QString &identifier, IdentifierType type) const {
	return _q_escapeIdentifier(identifier, type);
}
