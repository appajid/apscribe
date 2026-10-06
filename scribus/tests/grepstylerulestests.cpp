/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest>

#include "grepstylerules.h"

class GrepStyleRulesTests : public QObject
{
	Q_OBJECT

private slots:
	void roundTripsUnicodeRules();
	void rejectsMalformedRulesAtomically();
	void recalculatesMatchesAfterTextChanges();
	void skipsZeroWidthMatches();
};

void GrepStyleRulesTests::roundTripsUnicodeRules()
{
	const QList<GrepStyleRules::Rule> expected {
		{ QStringLiteral("\\b\\p{L}+\\b"), QStringLiteral("शब्द शैली"), false },
		{ QStringLiteral("chapter\\s+\\d+"), QStringLiteral("Heading"), true }
	};
	QList<GrepStyleRules::Rule> parsed;
	QVERIFY(GrepStyleRules::parse(GrepStyleRules::serialize(expected), &parsed));
	QCOMPARE(parsed, expected);
	QVERIFY(GrepStyleRules::serialize({}).isEmpty());
}

void GrepStyleRulesTests::rejectsMalformedRulesAtomically()
{
	QList<GrepStyleRules::Rule> rules { { QStringLiteral("keep"), QStringLiteral("Keep"), false } };
	QString error;
	QVERIFY(!GrepStyleRules::parse(QStringLiteral("[{}]"), &rules, &error));
	QVERIFY(!error.isEmpty());
	QCOMPARE(rules.size(), 1);
	QVERIFY(!GrepStyleRules::parse(QStringLiteral("[{\"pattern\":\"(\",\"characterStyle\":\"Bad\"}]"), &rules, &error));
	QVERIFY(error.contains(QStringLiteral("Invalid regular expression")));
	QCOMPARE(rules.size(), 1);
	QVERIFY(!GrepStyleRules::parse(QStringLiteral("{\"pattern\":\"x\"}"), &rules));
	QCOMPARE(rules.size(), 1);
}

void GrepStyleRulesTests::recalculatesMatchesAfterTextChanges()
{
	const QList<GrepStyleRules::Rule> rules {
		{ QStringLiteral("chapter\\s+\\d+"), QStringLiteral("Chapter"), true }
	};
	QCOMPARE(GrepStyleRules::matchParagraph(QStringLiteral("Chapter 3 and chapter 4"), rules),
		(QList<GrepStyleRules::Match> { { 0, 9, QStringLiteral("Chapter") },
			{ 14, 9, QStringLiteral("Chapter") } }));
	QCOMPARE(GrepStyleRules::matchParagraph(QStringLiteral("New chapter 42"), rules),
		(QList<GrepStyleRules::Match> { { 4, 10, QStringLiteral("Chapter") } }));
}

void GrepStyleRulesTests::skipsZeroWidthMatches()
{
	const QList<GrepStyleRules::Rule> rules {
		{ QStringLiteral("(?=a)"), QStringLiteral("No Visible Range"), false }
	};
	QVERIFY(GrepStyleRules::matchParagraph(QStringLiteral("a a"), rules).isEmpty());
}

QTEST_GUILESS_MAIN(GrepStyleRulesTests)
#include "grepstylerulestests.moc"
