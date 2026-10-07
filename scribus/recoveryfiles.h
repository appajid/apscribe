/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef RECOVERYFILES_H
#define RECOVERYFILES_H

#include <QDir>
#include <QDirIterator>
#include <QSet>
#include <QStringList>

namespace RecoveryFiles
{
inline QStringList find(const QStringList& directories)
{
	const QStringList patterns { QStringLiteral("*_emergency_*.sla"), QStringLiteral("*_autosave_*.sla") };
	const QDir::Filters filters = QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot;
	QSet<QString> visitedDirectories;
	QSet<QString> foundFiles;
	for (const QString& directory : directories)
	{
		if (directory.isEmpty())
			continue;
		const QString absoluteDirectory = QDir(directory).absolutePath();
		if (visitedDirectories.contains(absoluteDirectory))
			continue;
		visitedDirectories.insert(absoluteDirectory);

		// QDir::entryInfoList sorted the entire directory twice at startup.
		// Recovery only needs matching file paths, not a sorted directory list.
		QDirIterator it(absoluteDirectory, patterns, filters);
		while (it.hasNext())
		{
			it.next();
			foundFiles.insert(it.fileInfo().absoluteFilePath());
		}
	}
	return foundFiles.values();
}
}

#endif
