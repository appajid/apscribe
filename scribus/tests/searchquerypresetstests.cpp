/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest/QtTest>

#include "searchquerypresets.h"

class SearchQueryPresetsTests : public QObject
{
	Q_OBJECT
private slots:
	void roundTripsUnicodeAndRegex();
	void rejectsMalformedOrUnknownVersions();
	void skipsIncompleteEntries();
};

void SearchQueryPresetsTests::roundTripsUnicodeAndRegex()
{
	SearchQueryPresets::Queries queries;
	SearchTextQuery query;
	query.find = QString::fromUtf8("అధ్యాయం (\\d+)");
	query.replace = QStringLiteral("Chapter $1");
	query.regex = true;
	query.ignoreCase = true;
	queries.insert(QString::fromUtf8("Telugu chapters"), query);
	const auto restored = SearchQueryPresets::fromJson(SearchQueryPresets::toJson(queries));
	QCOMPARE(restored.size(), 1);
	QCOMPARE(restored.value(QString::fromUtf8("Telugu chapters")).find, query.find);
	QCOMPARE(restored.value(QString::fromUtf8("Telugu chapters")).replace, query.replace);
	QVERIFY(restored.value(QString::fromUtf8("Telugu chapters")).regex);
	QVERIFY(restored.value(QString::fromUtf8("Telugu chapters")).ignoreCase);
}

void SearchQueryPresetsTests::rejectsMalformedOrUnknownVersions()
{
	QVERIFY(SearchQueryPresets::fromJson(QStringLiteral("not json")).isEmpty());
	QVERIFY(SearchQueryPresets::fromJson(QStringLiteral("{\"version\":2,\"queries\":{}}")).isEmpty());
}

void SearchQueryPresetsTests::skipsIncompleteEntries()
{
	const auto queries = SearchQueryPresets::fromJson(QStringLiteral(
		"{\"version\":1,\"queries\":{\"bad\":{\"find\":\"x\"},"
		"\"good\":{\"find\":\"x\",\"replace\":\"\"}}}"));
	QCOMPARE(queries.size(), 1);
	QVERIFY(queries.contains(QStringLiteral("good")));
}

QTEST_APPLESS_MAIN(SearchQueryPresetsTests)
#include "searchquerypresetstests.moc"
