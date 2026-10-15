/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest/QtTest>

#include "regexsearch.h"

class RegexSearchTests : public QObject
{
	Q_OBJECT

private slots:
	void findsForwardAndBackward();
	void respectsUnicodeWordsAndCase();
	void skipsZeroLengthMatches();
	void triesLongerWholeWordAlternative();
	void rejectsInvalidExpression();
	void expandsCaptures();
	void rejectsUnknownCapture();
};

void RegexSearchTests::findsForwardAndBackward()
{
	const QString text = QStringLiteral("Chapter 12\nChapter 34");
	const auto expression = RegexSearch::compile(QStringLiteral("Chapter \\d+"), false);
	QVERIFY(expression.isValid());
	const auto first = RegexSearch::findNext(text, expression, 0, false);
	QCOMPARE(first.start, 0);
	QCOMPARE(first.length, 10);
	const auto second = RegexSearch::findNext(text, expression, first.start + first.length, false);
	QCOMPARE(second.start, 11);
	QCOMPARE(RegexSearch::findPrevious(text, expression, text.length(), false).start, 11);
	QCOMPARE(RegexSearch::findPrevious(text, expression, 11, false).start, 0);
}

void RegexSearchTests::respectsUnicodeWordsAndCase()
{
	const QString text = QString::fromUtf8("కథ కథలు KATHA");
	const auto expression = RegexSearch::compile(QString::fromUtf8("కథ|katha"), true);
	QVERIFY(expression.isValid());
	QCOMPARE(RegexSearch::findNext(text, expression, 0, true).start, 0);
	QCOMPARE(RegexSearch::findNext(text, expression, 3, true).start, 8);
}

void RegexSearchTests::skipsZeroLengthMatches()
{
	const auto expression = RegexSearch::compile(QStringLiteral("^|dog"), false);
	const auto match = RegexSearch::findNext(QStringLiteral("dog"), expression, 0, false);
	QCOMPARE(match.start, 0);
	QCOMPARE(match.length, 3);
}

void RegexSearchTests::triesLongerWholeWordAlternative()
{
	const auto expression = RegexSearch::compile(QStringLiteral("cat|cats"), false, true);
	QVERIFY(expression.isValid());
	const auto match = RegexSearch::findNext(QStringLiteral("cats"), expression, 0, true);
	QCOMPARE(match.start, 0);
	QCOMPARE(match.length, 4);
}

void RegexSearchTests::rejectsInvalidExpression()
{
	const auto expression = RegexSearch::compile(QStringLiteral("["), false);
	QVERIFY(!expression.isValid());
	QCOMPARE(RegexSearch::findNext(QStringLiteral("text"), expression, 0, false).start, -1);
}

void RegexSearchTests::expandsCaptures()
{
	const auto expression = RegexSearch::compile(QStringLiteral("(?<chapter>Chapter) (\\d+)"), false);
	const auto match = expression.match(QStringLiteral("Chapter 12"));
	QString result;
	QString error;
	QVERIFY(RegexSearch::expandReplacement(QStringLiteral("${chapter} $2 / $0 / $$"), match, &result, &error));
	QCOMPARE(result, QStringLiteral("Chapter 12 / Chapter 12 / $"));
}

void RegexSearchTests::rejectsUnknownCapture()
{
	const auto match = RegexSearch::compile(QStringLiteral("(abc)"), false).match(QStringLiteral("abc"));
	QString result;
	QString error;
	QVERIFY(!RegexSearch::expandReplacement(QStringLiteral("$2"), match, &result, &error));
	QVERIFY(error.contains(QStringLiteral("$2")) || error.contains(QLatin1Char('2')));
	QVERIFY(!RegexSearch::expandReplacement(QStringLiteral("${missing}"), match, &result, &error));
}

QTEST_APPLESS_MAIN(RegexSearchTests)
#include "regexsearchtests.moc"
