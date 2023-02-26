# Qt Driver Plugin for DuckDB
Implements a [driver plugin for Qt's SQL Databases](https://doc.qt.io/qt-6/sql-driver.html) for [DuckDB](https://duckdb.org/).  
Just copy the .dll/.so to your Qt plugin file and you can add a "DUCKDB" database.

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


## Requires
[DuckDB](https://duckdb.org/) >= 0.7.1 (Included in this repo as submodule. just select your version there)
[Qt6](https://www.qt.io/) (Version 5 should be possible, just cmake needs some minor adjustments)

DuckDB will be linked statically.  
Please also have a look on what Qt version can load plugins build with a given Qt version: 
[Qt Doc](https://doc.qt.io/qt-6/deployment-plugins.html#loading-and-verifying-plugins-dynamically)

## Known Limitations

The function `QSqlDatabase::handle()` does not return a `duckdb::Connection` handle yet.

## License
Based on [Qt's](https://www.qt.io/) sqlite driver code, which is licensed under the LGPL v3.  
Based on [DuckDB](https://duckdb.org/) under the MIT license.
[LGPL v3](./LICENSE)
