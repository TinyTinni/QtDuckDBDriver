#include <QApplication>
#include <QCompleter>
#include <QLineEdit>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringListModel>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + "/plugins/");
	QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB");

	db.open();
	QSqlQuery query(db);
	query.exec("CREATE TABLE employee (Name VARCHAR, Salary INTEGER);");
	query.exec("INSERT INTO employee VALUES ('Paul', 5000);");
	query.exec("INSERT INTO employee VALUES ('Bert', 5500);");
	query.exec("INSERT INTO employee VALUES ('Tina', 6500);");

	auto lineEdit = new QLineEdit();

	auto completer = new QCompleter(lineEdit);
	lineEdit->setCompleter(completer);
	completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	completer->setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);

	auto model = new QStringListModel();
	completer->setModel(model);

	QObject::connect(lineEdit, &QLineEdit::textChanged, completer, [completer, db, model](QString text) {
		QString prevText;
		if (QStringList prev = text.split(' '); prev.size() > 1) {
			prev.removeLast();
			prevText = prev.join(' ') + ' ';
		}

		QSqlQuery query(db);
		query.exec(QString("SELECT * FROM sql_auto_complete('%1');").arg(text));

		QStringList list;
		while (query.next()) {
			list.append(prevText + query.value(0).toString());
		}
		model->setStringList(std::move(list));
	});

	lineEdit->show();
	return app.exec();

	return 0;
}
