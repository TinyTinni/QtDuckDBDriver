#include <QCoreApplication>
#include <QTest>

#include "qttest/error_handling_test.h"
#include "qttest/features_test.h"
#include "qttest/model_test.h"
#include "qttest/prepared_statements_test.h"
#include "qttest/query_execution_test.h"
#include "qttest/raw_handle_test.h"
#include "qttest/schema_test.h"

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::addLibraryPath("./plugins/");

	int failures = 0;

	{
		QueryExecutionTest test;
		failures += QTest::qExec(&test, argc, argv);
	}
	{
		PreparedStatementsTest test;
		failures += QTest::qExec(&test, argc, argv);
	}
	{
		SchemaTest test;
		failures += QTest::qExec(&test, argc, argv);
	}
	{
		ModelTest test;
		failures += QTest::qExec(&test, argc, argv);
	}
	{
		RawHandleTest test;
		failures += QTest::qExec(&test, argc, argv);
	}
	{
		FeaturesTest test;
		failures += QTest::qExec(&test, argc, argv);
	}
	{
		ErrorHandlingTest test;
		failures += QTest::qExec(&test, argc, argv);
	}

	return failures;
}
