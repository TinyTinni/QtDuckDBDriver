# Qt Driver Plugin for DuckDB
Implements a [driver plugin for Qt's SQL Databases](https://doc.qt.io/qt-6/sql-driver.html) for [DuckDB](https://duckdb.org/).  
Just copy the .dll/.so to your Qt plugin file and you can add a "DUCKDB" database.

It enables you to use duckdb in an easy way with Qt and Qt widgets, e.g. building a table widget [see TableWidget example](./examples/TableWidget).

Exmaple for creating a in memory database:
```cpp
    QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB");
	db.setDatabaseName("test.db"); // creates a persistent database file "test.db"
	db.open(); 
	db.exec("CREATE TABLE employee (Name VARCHAR, Salary INTEGER);");
```

You can also use it to load csv or parquet (if duckdb was compiled with parquet support) files:
```
	QSqlDatabase db = QSqlDatabase::addDatabase("DUCKDB");
	db.open(); // no database name given -> creates a in-memory database
	db.exec("CREATE TABLE new_tbl AS SELECT * FROM read_csv_auto('my_csv.csv');";
```


## Requirement
[DuckDB](https://duckdb.org/) >= 0.7.1 (Included in this repo as submodule. just select your version there)  
[Qt6](https://www.qt.io/) (Version 5 should be possible, just cmake needs some minor adjustments)

DuckDB will be linked statically.  


For pre-build dlls, please choose the right version (See [Qt Doc about plugin version](https://doc.qt.io/qt-6/deployment-plugins.html#loading-and-verifying-plugins-dynamically))  
tl;dr: don't use the plugin on Qt version below the version it was build for.


## License
Based on [Qt's](https://www.qt.io/) sqlite driver code, which is licensed under the LGPL v3.  
Based on [DuckDB](https://duckdb.org/) under the MIT license.  
[LGPL v3](./LICENSE)
