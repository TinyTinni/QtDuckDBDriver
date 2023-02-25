// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QSQL_SQLITE_H
#define QSQL_SQLITE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtSql/qsqldriver.h>
#include <QSqlDriverPlugin>

struct sqlite3;

#ifdef QT_PLUGIN
#define Q_EXPORT_SQLDRIVER_DUCKDB
#else
#define Q_EXPORT_SQLDRIVER_DUCKDB Q_SQL_EXPORT
#endif

class QSqlResult;
class QDuckDBDriverPrivate;

class Q_EXPORT_SQLDRIVER_DUCKDB QDuckDBDriver : public QSqlDriver
{
    Q_DECLARE_PRIVATE(QDuckDBDriver);
    Q_OBJECT;
    friend class QDuckDBResultPrivate;
public:
    explicit QDuckDBDriver(QObject* parent = nullptr);
    explicit QDuckDBDriver(sqlite3* connection, QObject* parent = nullptr);
    ~QDuckDBDriver();
    bool hasFeature(DriverFeature f) const override;
    bool open(const QString& db,
        const QString& user,
        const QString& password,
        const QString& host,
        int port,
        const QString& connOpts) override;
    void close() override;
    QSqlResult* createResult() const override;
    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;
    QStringList tables(QSql::TableType) const override;

    QSqlRecord record(const QString& tablename) const override;
    QSqlIndex primaryIndex(const QString& table) const override;
    QVariant handle() const override;
    QString escapeIdentifier(const QString& identifier, IdentifierType) const override;

};



#endif // QSQL_SQLITE_H
