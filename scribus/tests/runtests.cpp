
#include <QTest>
//#include "testGlyphStore.h"
//#include "testIndex.h"
#include "testStoryText.h"
#include "runtests.h"
#include "testTableParagraphStyle.h"
#include "testDocumentZoom.h"
#include "testStyleCleanup.h"

int RunTests::runDocumentTests(const QStringList& arguments)
{
	// Leave QtTest's function/output options intact; select the initialized
	// document suite separately from those options.
	QStringList testArguments = arguments;
	if (testArguments.removeAll("--suite=zoom") > 0)
	{
		TestDocumentZoom tests;
		return QTest::qExec(&tests, testArguments);
	}
	if (testArguments.removeAll("--suite=style-cleanup") > 0)
	{
		TestStyleCleanup tests;
		return QTest::qExec(&tests, testArguments);
	}
	TestTableParagraphStyle tests;
	return QTest::qExec(&tests, testArguments);
}

int RunTests::runTests(const QStringList& arguments)
{ 
	QList<QObject *> testObjects;
//	testObjects << new TestGlyphStore();
	testObjects << new TestStoryText();
//	testObjects << new TestIndex();
	int failed = 0;
	for (int i = 0; i < testObjects.count(); ++i)
	{
		int rc = QTest::qExec(testObjects[i], arguments);
		if (rc != 0)
		{
			qDebug() << "          <<<<<<<<<< FAIL!" << rc << ">>>>>>>>>";
			++failed;
		}
	}
	return failed;
}
