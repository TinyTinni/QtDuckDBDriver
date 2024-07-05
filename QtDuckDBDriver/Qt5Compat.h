#pragma once
#include <QMetaObject>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

#include <QLatin1String>

// replaces "using namespace Qt::StringLiterals;"
QLatin1String operator""_L1(const char *c, size_t s) {
	return QLatin1String(c, s);
}

QVariant::Type toQtType(QMetaType::Type type) {
	return static_cast<QVariant::Type>(type);
}

#else

#include <QString>

using namespace Qt::StringLiterals;

QMetaType toQtType(QMetaType::Type type) {
	return QMetaType(type);
}

#endif