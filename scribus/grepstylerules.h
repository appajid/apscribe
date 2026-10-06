/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef GREPSTYLERULES_H
#define GREPSTYLERULES_H

#include <QList>
#include <QString>

namespace GrepStyleRules
{
struct Rule
{
	QString pattern;
	QString characterStyle;
	bool ignoreCase { false };

	bool operator==(const Rule& other) const
	{
		return pattern == other.pattern && characterStyle == other.characterStyle && ignoreCase == other.ignoreCase;
	}
};

struct Match
{
	int start { -1 };
	int length { 0 };
	QString characterStyle;

	bool operator==(const Match& other) const
	{
		return start == other.start && length == other.length && characterStyle == other.characterStyle;
	}
};

// An empty string means no rules. Invalid data is rejected atomically, so a
// malformed document cannot leave a partially active set of style rules.
bool parse(const QString& json, QList<Rule>* rules, QString* error = nullptr);
QString serialize(const QList<Rule>& rules);

// Match offsets are UTF-16 positions relative to one paragraph. Later rules
// have precedence where matches overlap; applying the styles is a later slice.
QList<Match> matchParagraph(const QString& text, const QList<Rule>& rules);
}

#endif
