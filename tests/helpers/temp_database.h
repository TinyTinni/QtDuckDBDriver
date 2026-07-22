#pragma once

#include <QDir>
#include <QFile>
#include <QSqlDatabase>

class TempDatabase {
public:
	QString path;
	QString connectionName;

	explicit TempDatabase(const QString &name)
	    : path(QDir::tempPath() + "/qtduckdb_" + name + ".duckdb")
	    , connectionName(name) {}

	~TempDatabase() { cleanup(); }

	TempDatabase(const TempDatabase &) = delete;
	TempDatabase &operator=(const TempDatabase &) = delete;

	void cleanup() {
		{
			QSqlDatabase db = QSqlDatabase::database(connectionName, false);
			if (db.isValid())
				db.close();
		}
		QSqlDatabase::removeDatabase(connectionName);
		QFile::remove(path);
	}
};
