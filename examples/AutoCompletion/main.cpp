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

	QLineEdit *lineEdit = new QLineEdit();

	auto *completer = new QCompleter(lineEdit);
	lineEdit->setCompleter(completer);
	completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	completer->setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);

	auto model = new QStringListModel();
	completer->setModel(model);

	QObject::connect(lineEdit, &QLineEdit::textChanged, completer, [completer, db, model](QString text) {
		QStringList prev = text.split(' ');
		if (!prev.isEmpty())
			prev.removeLast();
		QString prevText = prev.join(' ');
		if (!prevText.isEmpty())
			prevText += ' ';

		QSqlQuery query;
		query.exec(QString("SELECT * FROM sql_auto_complete('%1');").arg(text));

		QStringList list;
		while (query.next()) {
			QString result = query.value(0).toString();
			QString completionText = prevText + result;
			list.append(completionText);
		}
		model->setStringList(std::move(list));
	});

	lineEdit->show();
	return app.exec();

	return 0;
}
