#pragma once

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTime>
#include <QVariant>
#include <rapidcheck/gen/Arbitrary.h>
#include <rapidcheck/gen/Container.h>
#include <rapidcheck/gen/Numeric.h>
#include <rapidcheck/gen/Select.h>
#include <tuple>

namespace rc {

template <>
struct Arbitrary<QString> {
	static Gen<QString> arbitrary() {
		static const std::string validChars =
		    "abcdefghijklmnopqrstuvwxyz"
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		    "0123456789_";
		return gen::map(
		    gen::container<std::string>(gen::elementOf(validChars)),
		    [](const std::string &s) { return QString::fromStdString(s); });
	}
};

template <>
struct Arbitrary<QDate> {
	static Gen<QDate> arbitrary() {
		return gen::map(
		    gen::inRange<int64_t>(
		        static_cast<int64_t>(QDate(1970, 1, 1).toJulianDay()),
		        static_cast<int64_t>(QDate(2299, 12, 31).toJulianDay())),
		    [](int64_t julianDay) { return QDate::fromJulianDay(static_cast<qlonglong>(julianDay)); });
	}
};

template <>
struct Arbitrary<QTime> {
	static Gen<QTime> arbitrary() {
		return gen::map(
		    gen::inRange<int>(0, 86399999),
		    [](int ms) { return QTime(0, 0, 0).addMSecs(ms); });
	}
};

template <>
struct Arbitrary<QDateTime> {
	static Gen<QDateTime> arbitrary() {
		return gen::map(
		    gen::tuple(Arbitrary<QDate>::arbitrary(), Arbitrary<QTime>::arbitrary()),
		    [](std::tuple<QDate, QTime> t) {
			    return QDateTime(std::get<0>(t), std::get<1>(t));
		    });
	}
};

template <>
struct Arbitrary<QByteArray> {
	static Gen<QByteArray> arbitrary() {
		return gen::map(
		    gen::container<std::string>(gen::inRange<char>(0, 127)),
		    [](const std::string &s) {
			    return QByteArray(s.data(), static_cast<qsizetype>(s.size()));
		    });
	}
};

} // namespace rc
