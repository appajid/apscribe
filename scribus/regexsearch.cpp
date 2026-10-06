/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include "regexsearch.h"

namespace
{
bool isWholeWord(const QString& text, int start, int length)
{
	return (start == 0 || !text.at(start - 1).isLetterOrNumber())
		&& (start + length == text.length() || !text.at(start + length).isLetterOrNumber());
}
}

QRegularExpression RegexSearch::compile(const QString& pattern, bool ignoreCase, bool wholeWords)
{
	QRegularExpression::PatternOptions flags = QRegularExpression::UseUnicodePropertiesOption;
	if (ignoreCase)
		flags |= QRegularExpression::CaseInsensitiveOption;
	// Put the boundaries in the pattern so the engine can try a longer
	// alternative at the same position (for example, cat|cats in "cats").
	const QString expression = wholeWords
		? QStringLiteral("(?<![\\p{L}\\p{N}])(?:%1)(?![\\p{L}\\p{N}])").arg(pattern)
		: pattern;
	return QRegularExpression(expression, flags);
}

RegexSearch::Match RegexSearch::findNext(const QString& text, const QRegularExpression& expression, int from, bool wholeWords)
{
	if (!expression.isValid() || from < 0 || from >= text.length())
		return {};

	auto matches = expression.globalMatch(text, from);
	while (matches.hasNext())
	{
		const auto match = matches.next();
		if (match.capturedLength() > 0 && (!wholeWords || isWholeWord(text, match.capturedStart(), match.capturedLength())))
			return { static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()) };
	}
	return {};
}

RegexSearch::Match RegexSearch::findPrevious(const QString& text, const QRegularExpression& expression, int before, bool wholeWords)
{
	if (!expression.isValid() || before <= 0)
		return {};

	Match result;
	auto matches = expression.globalMatch(text);
	while (matches.hasNext())
	{
		const auto match = matches.next();
		if (match.capturedStart() + match.capturedLength() > before)
			break;
		if (match.capturedLength() > 0 && (!wholeWords || isWholeWord(text, match.capturedStart(), match.capturedLength())))
			result = { static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()) };
	}
	return result;
}
