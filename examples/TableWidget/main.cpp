#include <QsqlTableModel>
#include <QTableView>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMainWindow>
#include <QApplication>
#include <QtConcurrent>
#include <QFutureWatcher>

#include <chrono>
#include <iostream>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB");
    db.open();
    //auto start = std::chrono::high_resolution_clock::now();
    QSqlQuery loading_stmt("CREATE TABLE new_tbl AS SELECT * FROM read_csv_auto(?) LIMIT 1000;", db);
    QSqlQuery loading_stmt_complete("CREATE TABLE new_tbl2 AS SELECT * FROM read_csv_auto(?);", db);
    loading_stmt.bindValue(0, "test.csv");
    loading_stmt_complete.bindValue(0, "test.csv");
    auto start = std::chrono::high_resolution_clock::now();
    loading_stmt.exec();
    //db.exec("CREATE TABLE new_tbl AS SELECT * FROM read_csv_auto('C:/Users/Anwender/source/repos/QtDuckDBDriver/QtDuckDBDriver/duckdb/data/csv/customer.csv');");
    std::cout << "loading time: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << " microseconds\n";
    //db.exec("CREATE TABLE employee (Name VARCHAR, Salary INTEGER);");
    //db.exec("INSERT INTO employee VALUES ('Paul', 5000);");
    //db.exec("INSERT INTO employee VALUES ('Ludger', 5500);");
    //db.exec("INSERT INTO employee VALUES ('Dirk', 6500);");
    QSqlTableModel* model = new QSqlTableModel(nullptr, db);
    auto tables = db.tables();
    model->setTable(tables[0]);
    model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    //model->select();

    QTableView* view = new QTableView;
    view->setModel(model);
    //for (int i = 0; i < 7; ++i)
    //    view->hideColumn(i);
    //view->setSortingEnabled(true);
    view->show();
    QSqlTableModel* model2 = new QSqlTableModel(nullptr, db);

    QtConcurrent::run([&]() {model->select(); });

    //auto fut = [view, db, model2, &loading_stmt_complete]() {
    //    //std::this_thread::sleep_for(std::chrono::seconds{ 2 });


    //    auto start = std::chrono::high_resolution_clock::now();
    //    loading_stmt_complete.exec();

    //    auto tables = db.tables();

    //    model2->setTable("new_tbl2");
    //    model2->setEditStrategy(QSqlTableModel::OnManualSubmit);
    //    model2->select();
    //    view->setModel(model2);
    //    return true;
    //    //d
    //}();

    //fut.then(QtFuture::Launch::Sync, [view, model2, db]() {view->setModel(model2); view->update(); db.exec("DROP TABLE new_tbl"); });

    return app.exec();
}
