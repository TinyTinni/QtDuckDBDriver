#include <QApplication>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QSqlQuery>
#include <QTableView>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + "/plugins/");
	QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB");

	db.open();
	db.exec("CREATE TABLE employee (Name VARCHAR, Salary INTEGER);");
	db.exec("INSERT INTO employee VALUES ('Paul', 5000);");
	db.exec("INSERT INTO employee VALUES ('Bert', 5500);");
	db.exec("INSERT INTO employee VALUES ('Tina', 6500);");

	QSqlTableModel *model = new QSqlTableModel(nullptr, db);
	model->setTable("employee");
	model->select();

	QTableView *view = new QTableView;
	view->setModel(model);
	view->show();

	return app.exec();
}
