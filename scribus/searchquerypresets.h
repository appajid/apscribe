/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef SEARCHQUERYPRESETS_H
#define SEARCHQUERYPRESETS_H

#include <QMap>
#include <QString>

struct SearchTextQuery
{
	QString find;
	QString replace;
	bool regex { false };
	bool ignoreCase { false };
	bool wholeWords { false };
};

namespace SearchQueryPresets
{
using Queries = QMap<QString, SearchTextQuery>;
Queries fromJson(const QString& json);
QString toJson(const Queries& queries);
}

#endif
