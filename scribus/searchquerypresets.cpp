/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include "searchquerypresets.h"

#include <QJsonDocument>
#include <QJsonObject>

SearchQueryPresets::Queries SearchQueryPresets::fromJson(const QString& json)
{
	Queries queries;
	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject())
		return queries;
	const QJsonObject root = document.object();
	if (root.value("version").toInt() != 1 || !root.value("queries").isObject())
		return queries;
	const QJsonObject entries = root.value("queries").toObject();
	for (auto it = entries.constBegin(); it != entries.constEnd(); ++it)
	{
		if (it.key().trimmed().isEmpty() || !it.value().isObject())
			continue;
		const QJsonObject value = it.value().toObject();
		if (!value.value("find").isString() || !value.value("replace").isString())
			continue;
		SearchTextQuery query;
		query.find = value.value("find").toString();
		query.replace = value.value("replace").toString();
		query.regex = value.value("regex").toBool();
		query.ignoreCase = value.value("ignoreCase").toBool();
		query.wholeWords = value.value("wholeWords").toBool();
		queries.insert(it.key(), query);
	}
	return queries;
}

QString SearchQueryPresets::toJson(const Queries& queries)
{
	QJsonObject entries;
	for (auto it = queries.constBegin(); it != queries.constEnd(); ++it)
	{
		QJsonObject value;
		value.insert("find", it.value().find);
		value.insert("replace", it.value().replace);
		value.insert("regex", it.value().regex);
		value.insert("ignoreCase", it.value().ignoreCase);
		value.insert("wholeWords", it.value().wholeWords);
		entries.insert(it.key(), value);
	}
	QJsonObject root;
	root.insert("version", 1);
	root.insert("queries", entries);
	return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}
