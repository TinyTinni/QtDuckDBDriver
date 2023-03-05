
// #ifdef USE_DUCKDB_SHELL_WRAPPER
#include <duckdb_shell_wrapper.h>
// #endif
#include "QtDuckDBDriver.h"

#include <QScopedValueRollback>
#include <functional>
#include <private/qsqlcachedresult_p.h>
#include <private/qsqldriver_p.h>
#include <qcoreapplication.h>
#include <qdatetime.h>
#include <qdebug.h>
#include <qlist.h>
#include <qsqlerror.h>
#include <qsqlfield.h>
#include <qsqlindex.h>
#include <qsqlquery.h>
#include <qstringlist.h>
#include <qvariant.h>
#include <sqlite3.h>
#include <udf_struct_sqlite3.h>

#if defined Q_OS_WIN
#include <qt_windows.h>
#else
#include <unistd.h>
#endif

Q_DECLARE_OPAQUE_POINTER(DuckDBConnectionHandle *)
Q_DECLARE_METATYPE(DuckDBConnectionHandle *)

using DuckDBStmt = sqlite3_stmt;

Q_DECLARE_OPAQUE_POINTER(DuckDBStmt *)
Q_DECLARE_METATYPE(DuckDBStmt *)

using namespace Qt::StringLiterals;

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

static int qGetColumnType(const QString &tpName) {
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

static QSqlError qMakeError(sqlite3 *access, const QString &descr, QSqlError::ErrorType type, int errorCode) {
	return QSqlError(descr, QString::fromUtf8(sqlite3_errmsg(access)), type, QString::number(errorCode));
}

class QDuckDBResultPrivate;

class QDuckDBResult : public QSqlCachedResult {
	Q_DECLARE_PRIVATE(QDuckDBResult)
	friend class QDuckDBDriver;

public:
	explicit QDuckDBResult(const QDuckDBDriver *db);
	~QDuckDBResult();
	QVariant handle() const override;

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
	sqlite3 *access = nullptr;
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

	sqlite3_stmt *stmt = nullptr;
	QSqlRecord rInf;
	QList<QVariant> firstRow;
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

	sqlite3_finalize(stmt);
	stmt = 0;
}

void QDuckDBResultPrivate::initColumns(bool emptyResultset) {
	Q_Q(QDuckDBResult);
	int nCols = sqlite3_column_count(stmt);
	if (nCols <= 0)
		return;

	q->init(nCols);

	for (int i = 0; i < nCols; ++i) {
		QString colName = QString::fromUtf8(sqlite3_column_name(stmt, i)).remove(u'"');
		const QString tableName = QString::fromUtf8(sqlite3_column_table_name(stmt, i)).remove(u'"');
		// must use typeName for resolving the type to match QDuckDBDriver::record
		QString typeName = QString::fromUtf8(sqlite3_column_decltype(stmt, i));
		// sqlite3_column_type is documented to have undefined behavior if the result set is empty
		int stp = emptyResultset ? -1 : sqlite3_column_type(stmt, i);

		int fieldType;

		if (!typeName.isEmpty()) {
			fieldType = qGetColumnType(typeName);
		} else {
			// Get the proper type for the field based on stp value
			switch (stp) {
			case SQLITE_INTEGER:
				fieldType = QMetaType::Int;
				break;
			case SQLITE_FLOAT:
				fieldType = QMetaType::Double;
				break;
			case SQLITE_BLOB:
				fieldType = QMetaType::QByteArray;
				break;
			case SQLITE_TEXT:
				fieldType = QMetaType::QString;
				break;
			case SQLITE_NULL:
			default:
				fieldType = QMetaType::UnknownType;
				break;
			}
		}

		QSqlField fld(colName, QMetaType(fieldType), tableName);
		fld.setSqlType(stp);
		rInf.append(fld);
	}
}

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
		firstRow.resize(sqlite3_column_count(stmt));
	}

	if (!stmt) {
		q->setLastError(QSqlError(QCoreApplication::translate("QSQLiteResult", "Unable to fetch row"),
		                          QCoreApplication::translate("QSQLiteResult", "No query"),
		                          QSqlError::ConnectionError));
		q->setAt(QSql::AfterLastRow);
		return false;
	}
	int res = sqlite3_step(stmt);
	switch (res) {
	case SQLITE_ROW:
		// check to see if should fill out columns
		if (rInf.isEmpty())
			// must be first call.
			initColumns(false);
		if (idx < 0 && !initialFetch)
			return true;
		for (int i = 0; i < rInf.count(); ++i) {
			switch (sqlite3_column_type(stmt, i)) {
			case SQLITE_BLOB:
				values[i + idx] =
				    QByteArray(static_cast<const char *>(sqlite3_column_blob(stmt, i)), sqlite3_column_bytes(stmt, i));
				break;
			case SQLITE_INTEGER:
				values[i + idx] = sqlite3_column_int64(stmt, i);
				break;
			case SQLITE_FLOAT:
				switch (q->numericalPrecisionPolicy()) {
				case QSql::LowPrecisionInt32:
					values[i + idx] = sqlite3_column_int(stmt, i);
					break;
				case QSql::LowPrecisionInt64:
					values[i + idx] = sqlite3_column_int64(stmt, i);
					break;
				case QSql::LowPrecisionDouble:
				case QSql::HighPrecision:
				default:
					values[i + idx] = sqlite3_column_double(stmt, i);
					break;
				};
				break;
			case SQLITE_NULL:
				values[i + idx] = QVariant(QMetaType::fromType<QString>());
				break;
			default:
				values[i + idx] = QString::fromUtf8((const char *)sqlite3_column_text(stmt, i),
				                                    (int)(sqlite3_column_bytes(stmt, i) / sizeof(char)));
				break;
			}
		}
		return true;
	case SQLITE_DONE:
		if (rInf.isEmpty())
			// must be first call.
			initColumns(true);
		q->setAt(QSql::AfterLastRow);
		sqlite3_reset(stmt);
		return false;
	case SQLITE_CONSTRAINT:
	case SQLITE_ERROR:
		// SQLITE_ERROR is a generic error code and we must call sqlite3_reset()
		// to get the specific error message.
		res = sqlite3_reset(stmt);
		q->setLastError(qMakeError(drv_d_func()->access,
		                           QCoreApplication::translate("QSQLiteResult", "Unable to fetch row"),
		                           QSqlError::ConnectionError, res));
		q->setAt(QSql::AfterLastRow);
		return false;
	case SQLITE_MISUSE:
	case SQLITE_BUSY:
	default:
		// something wrong, don't get col info, but still return false
		q->setLastError(qMakeError(drv_d_func()->access,
		                           QCoreApplication::translate("QSQLiteResult", "Unable to fetch row"),
		                           QSqlError::ConnectionError, res));
		sqlite3_reset(stmt);
		q->setAt(QSql::AfterLastRow);
		return false;
	}
	return false;
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

	const char *pzTail = nullptr;
	QByteArray query_string = query.toUtf8();
	int res = sqlite3_prepare_v2(d->drv_d_func()->access, query_string.constData(), query_string.size(), &d->stmt,
	                             (const char **)&pzTail);

	if (res != SQLITE_OK) {
		setLastError(qMakeError(d->drv_d_func()->access,
		                        QCoreApplication::translate("QSQLiteResult", "Unable to execute statement"),
		                        QSqlError::StatementError, res));
		d->finalize();
		return false;
	} else if (pzTail && !QString(pzTail).trimmed().isEmpty()) {
		QString debug = QString(pzTail).trimmed();
		setLastError(
		    qMakeError(d->drv_d_func()->access,
		               QCoreApplication::translate("QSQLiteResult", "Unable to execute multiple statements at a time"),
		               QSqlError::StatementError, SQLITE_MISUSE));
		d->finalize();
		return false;
	}
	return true;
}

bool QDuckDBResult::execBatch(bool arrayBind) {
	Q_UNUSED(arrayBind);
	Q_D(QSqlResult);
	QScopedValueRollback<QList<QVariant>> valuesScope(d->values);
	QList<QVariant> values = d->values;
	if (values.size() == 0)
		return false;

	for (int i = 0; i < values.at(0).toList().size(); ++i) {
		d->values.clear();
		QScopedValueRollback<QHash<QString, QList<int>>> indexesScope(d->indexes);
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
	QList<QVariant> values = boundValues();

	d->skippedStatus = false;
	d->skipRow = false;
	d->rInf.clear();
	clearValues();
	setLastError(QSqlError());

	int res = sqlite3_reset(d->stmt);
	if (res != SQLITE_OK) {
		setLastError(qMakeError(d->drv_d_func()->access,
		                        QCoreApplication::translate("QSQLiteResult", "Unable to reset statement"),
		                        QSqlError::StatementError, res));
		d->finalize();
		return false;
	}

	int paramCount = sqlite3_bind_parameter_count(d->stmt);
	bool paramCountIsValid = paramCount == values.size();

#if (SQLITE_VERSION_NUMBER >= 3003011)
	// In the case of the reuse of a named placeholder
	// We need to check explicitly that paramCount is greater than or equal to 1, as sqlite
	// can end up in a case where for virtual tables it returns 0 even though it
	// has parameters
	if (paramCount >= 1 && paramCount < values.size()) {
		const auto countIndexes = [](int counter, const QList<int> &indexList) {
			return counter + indexList.size();
		};

		const int bindParamCount = std::accumulate(d->indexes.cbegin(), d->indexes.cend(), 0, countIndexes);

		paramCountIsValid = bindParamCount == values.size();
		// When using named placeholders, it will reuse the index for duplicated
		// placeholders. So we need to ensure the QList has only one instance of
		// each value as SQLite will do the rest for us.
		QList<QVariant> prunedValues;
		QList<int> handledIndexes;
		for (int i = 0, currentIndex = 0; i < values.size(); ++i) {
			if (handledIndexes.contains(i))
				continue;
			const char *parameterName = sqlite3_bind_parameter_name(d->stmt, currentIndex + 1);
			if (!parameterName) {
				paramCountIsValid = false;
				continue;
			}
			const auto placeHolder = QString::fromUtf8(parameterName);
			const auto &indexes = d->indexes.value(placeHolder);
			handledIndexes << indexes;
			prunedValues << values.at(indexes.first());
			++currentIndex;
		}
		values = prunedValues;
	}
#endif

	if (paramCountIsValid) {
		for (int i = 0; i < paramCount; ++i) {
			res = SQLITE_OK;
			const QVariant &value = values.at(i);

			if (QSqlResultPrivate::isVariantNull(value)) {
				res = sqlite3_bind_null(d->stmt, i + 1);
			} else {
				switch (value.userType()) {
				case QMetaType::QByteArray: {
					const QByteArray *ba = static_cast<const QByteArray *>(value.constData());
					res = sqlite3_bind_blob(d->stmt, i + 1, ba->constData(), ba->size(), SQLITE_STATIC);
					break;
				}
				case QMetaType::Int:
				case QMetaType::Bool:
					res = sqlite3_bind_int(d->stmt, i + 1, value.toInt());
					break;
				case QMetaType::Double:
					res = sqlite3_bind_double(d->stmt, i + 1, value.toDouble());
					break;
				case QMetaType::UInt:
				case QMetaType::LongLong:
					res = sqlite3_bind_int64(d->stmt, i + 1, value.toLongLong());
					break;
				case QMetaType::QDateTime: {
					const QDateTime dateTime = value.toDateTime();
					const QString str = dateTime.toString(Qt::ISODateWithMs);
					res = sqlite3_bind_text(d->stmt, i + 1, str.toUtf8(), int(str.size() * sizeof(char)),
					                        SQLITE_TRANSIENT);
					break;
				}
				case QMetaType::QTime: {
					const QTime time = value.toTime();
					const QString str = time.toString(u"hh:mm:ss.zzz");
					res = sqlite3_bind_text(d->stmt, i + 1, str.toUtf8(), int(str.size() * sizeof(char)),
					                        SQLITE_TRANSIENT);
					break;
				}
				case QMetaType::QString: {
					// lifetime of string == lifetime of its qvariant
					const QString *str = static_cast<const QString *>(value.constData());
					res = sqlite3_bind_text(d->stmt, i + 1, str->toUtf8(), int(str->size()) * sizeof(char),
					                        SQLITE_STATIC);
					break;
				}
				default: {
					const QString str = value.toString();
					// SQLITE_TRANSIENT makes sure that sqlite buffers the data
					res = sqlite3_bind_text(d->stmt, i + 1, str.toUtf8(), int(str.size() * sizeof(char)),
					                        SQLITE_TRANSIENT);
					break;
				}
				}
			}
			if (res != SQLITE_OK) {
				setLastError(qMakeError(d->drv_d_func()->access,
				                        QCoreApplication::translate("QSQLiteResult", "Unable to bind parameters"),
				                        QSqlError::StatementError, res));
				d->finalize();
				return false;
			}
		}
	} else {
		setLastError(QSqlError(QCoreApplication::translate("QSQLiteResult", "Parameter count mismatch"), QString(),
		                       QSqlError::StatementError));
		return false;
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
	return -1;
}

int QDuckDBResult::numRowsAffected() {
	Q_D(const QDuckDBResult);
	return sqlite3_changes(d->drv_d_func()->access);
}

QVariant QDuckDBResult::lastInsertId() const {
	Q_D(const QDuckDBResult);
	if (isActive()) {
		qint64 id = sqlite3_last_insert_rowid(d->drv_d_func()->access);
		if (id)
			return id;
	}
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
	if (d->stmt)
		sqlite3_reset(d->stmt);
}

QVariant QDuckDBResult::handle() const {
	Q_D(const QDuckDBResult);
	return QVariant::fromValue(d->stmt);
}

/////////////////////////////////////////////////////////

QDuckDBDriver::QDuckDBDriver(QObject *parent) : QSqlDriver(*new QDuckDBDriverPrivate, parent) {
}

QDuckDBDriver::~QDuckDBDriver() {
	close();
}

bool QDuckDBDriver::hasFeature(DriverFeature f) const {
	switch (f) {
	case BLOB:
	case Transactions:
	case Unicode:
	case LastInsertId:
	case PreparedQueries:
	case PositionalPlaceholders:
	case SimpleLocking:
	case FinishQuery:
	case NamedPlaceholders:
		return true;
	case LowPrecisionNumbers: // unsure
	case EventNotifications:
	case QuerySize:
	case BatchOperations:
	case MultipleResultSets:
	case CancelQuery:
		return false;
	}
	return false;
}

/*
   SQLite dbs have no user name, passwords, hosts or ports.
   just file names.
*/
bool QDuckDBDriver::open(const QString &db, const QString &, const QString &, const QString &, int,
                         const QString &conOpts) {
	Q_D(QDuckDBDriver);
	if (isOpen())
		close();

	bool sharedCache = false;
	bool openReadOnlyOption = false;
	bool openUriOption = false;

	const auto opts = QStringView {conOpts}.split(u';');
	for (auto option : opts) {
		option = option.trimmed();
		if (option == "QSQLITE_OPEN_READONLY"_L1) {
			openReadOnlyOption = true;
		} else if (option == "QSQLITE_OPEN_URI"_L1) {
			openUriOption = true;
		} else if (option == "QSQLITE_ENABLE_SHARED_CACHE"_L1) {
			sharedCache = true;
		}
	}

	int openMode = (openReadOnlyOption ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
	openMode |= (sharedCache ? SQLITE_OPEN_SHAREDCACHE : SQLITE_OPEN_PRIVATECACHE);
	if (openUriOption)
		openMode |= SQLITE_OPEN_URI;

	openMode |= SQLITE_OPEN_NOMUTEX;

	const int res = sqlite3_open_v2(db.toUtf8().constData(), &d->access, openMode, nullptr);

	if (res == SQLITE_OK) {
		setOpen(true);
		setOpenError(false);
		return true;
	} else {
		setLastError(qMakeError(d->access, tr("Error opening database"), QSqlError::ConnectionError, res));
		setOpenError(true);

		if (d->access) {
			sqlite3_close(d->access);
			d->access = 0;
		}

		return false;
	}
}

void QDuckDBDriver::close() {
	Q_D(QDuckDBDriver);
	if (isOpen()) {
		for (QDuckDBResult *result : std::as_const(d->results))
			result->d_func()->finalize();

		const int res = sqlite3_close(d->access);

		if (res != SQLITE_OK)
			setLastError(qMakeError(d->access, tr("Error closing database"), QSqlError::ConnectionError, res));
		d->access = 0;
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

	QString table_sql = QString("SELECT table_name FROM duckdb_tables() %1;").arg(system_tables);
	QString view_sql = QString("SELECT view_name FROM duckdb_views() %1;").arg(system_tables);

	if (type & QSql::Tables) {
		if (!table_sql.isEmpty() && q.exec(table_sql)) {
			while (q.next())
				res.append(q.value(0).toString());
		}
	}

	if (type & QSql::Views) {
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

		QSqlField fld(q.value(1).toString(), QMetaType(qGetColumnType(typeName)), tableName);
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
	DuckDBConnectionHandle handle {d->access->db.get(), d->access->con.get()};
	return QVariant::fromValue(handle);
}

QString QDuckDBDriver::escapeIdentifier(const QString &identifier, IdentifierType type) const {
	return _q_escapeIdentifier(identifier, type);
}

#include "moc_QtDuckDBDriver.cpp"
