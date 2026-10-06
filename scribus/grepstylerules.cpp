/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include "grepstylerules.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "regexsearch.h"

bool GrepStyleRules::parse(const QString& json, QList<Rule>* rules, QString* error)
{
	if (!rules)
		return false;
	QList<Rule> parsed;
	if (json.isEmpty())
	{
		*rules = parsed;
		return true;
	}
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isArray())
	{
		if (error)
			*error = QStringLiteral("GREP style rules must be a JSON array");
		return false;
	}
	for (const auto& value : document.array())
	{
		if (!value.isObject())
		{
			if (error)
				*error = QStringLiteral("Each GREP style rule must be an object");
			return false;
		}
		const auto object = value.toObject();
		if (!object.value(QStringLiteral("pattern")).isString()
			|| !object.value(QStringLiteral("characterStyle")).isString()
			|| (object.contains(QStringLiteral("ignoreCase")) && !object.value(QStringLiteral("ignoreCase")).isBool()))
		{
			if (error)
				*error = QStringLiteral("A GREP style rule needs a pattern, character style and optional case flag");
			return false;
		}
		Rule rule { object.value(QStringLiteral("pattern")).toString(),
			object.value(QStringLiteral("characterStyle")).toString(),
			object.value(QStringLiteral("ignoreCase")).toBool(false) };
		if (rule.pattern.isEmpty() || rule.characterStyle.isEmpty())
		{
			if (error)
				*error = QStringLiteral("A GREP style rule needs a nonempty pattern and character style");
			return false;
		}
		const auto expression = RegexSearch::compile(rule.pattern, rule.ignoreCase);
		if (!expression.isValid())
		{
			if (error)
				*error = QStringLiteral("Invalid regular expression at position %1: %2")
					.arg(expression.patternErrorOffset()).arg(expression.errorString());
			return false;
		}
		parsed.append(rule);
	}
	*rules = parsed;
	return true;
}

QString GrepStyleRules::serialize(const QList<Rule>& rules)
{
	if (rules.isEmpty())
		return {};
	QJsonArray array;
	for (const Rule& rule : rules)
	{
		QJsonObject object;
		object.insert(QStringLiteral("pattern"), rule.pattern);
		object.insert(QStringLiteral("characterStyle"), rule.characterStyle);
		object.insert(QStringLiteral("ignoreCase"), rule.ignoreCase);
		array.append(object);
	}
	return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QList<GrepStyleRules::Match> GrepStyleRules::matchParagraph(const QString& text, const QList<Rule>& rules)
{
	QList<Match> matches;
	for (const Rule& rule : rules)
	{
		const auto expression = RegexSearch::compile(rule.pattern, rule.ignoreCase);
		if (!expression.isValid())
			continue;
		auto iterator = expression.globalMatch(text);
		while (iterator.hasNext())
		{
			const auto match = iterator.next();
			if (match.capturedLength() > 0)
				matches.append({ static_cast<int>(match.capturedStart()),
					static_cast<int>(match.capturedLength()), rule.characterStyle });
		}
	}
	return matches;
}
