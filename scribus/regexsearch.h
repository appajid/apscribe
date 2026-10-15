/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef REGEXSEARCH_H
#define REGEXSEARCH_H

#include <QRegularExpression>
#include <QString>

namespace RegexSearch
{
struct Match
{
	int start { -1 };
	int length { 0 };
};

QRegularExpression compile(const QString& pattern, bool ignoreCase, bool wholeWords = false);
Match findNext(const QString& text, const QRegularExpression& expression, int from, bool wholeWords);
Match findPrevious(const QString& text, const QRegularExpression& expression, int before, bool wholeWords);
// Replacement syntax: $0 for the full match, $1... for numbered groups,
// ${name} for named groups, and $$ for a literal dollar sign.
bool expandReplacement(const QString& replacement, const QRegularExpressionMatch& match, QString* result, QString* error);
}

#endif
