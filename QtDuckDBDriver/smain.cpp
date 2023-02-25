#include "QtDuckDBDriver.h"

class DuckDBSqlPlugin : public QSqlDriverPlugin {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QSqlDriverFactoryInterface" FILE "duckdb.json")
public:
	QSqlDriver *create(const QString &key);
};

QSqlDriver *DuckDBSqlPlugin::create(const QString &key) {
	if (key == "DUCKDB")
		return new QDuckDBDriver();
	return nullptr;
}

#include "smain.moc"
