#include <QApplication>
#include <QFutureWatcher>
#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlTableModel>
#include <QTableView>
#include <QtConcurrent>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB");
	db.open();
	db.exec("CREATE TABLE employee (Name VARCHAR, Salary INTEGER);");
	db.exec("INSERT INTO employee VALUES ('Paul', 5000);");
	db.exec("INSERT INTO employee VALUES ('Ludger', 5500);");
	db.exec("INSERT INTO employee VALUES ('Dirk', 6500);");

	QSqlTableModel *model = new QSqlTableModel(nullptr, db);
	auto tables = db.tables();
	model->setTable(tables[0]);
	model->setEditStrategy(QSqlTableModel::OnManualSubmit);
	model->select();

	QTableView *view = new QTableView;
	view->setModel(model);
	view->show();

	return app.exec();
}
