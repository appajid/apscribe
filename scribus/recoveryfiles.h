/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef RECOVERYFILES_H
#define RECOVERYFILES_H

#include <QDir>
#include <QDirIterator>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

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
	QStringList files = foundFiles.values();
	std::sort(files.begin(), files.end(), [](const QString& left, const QString& right) {
		const int comparison = QString::compare(left, right, Qt::CaseInsensitive);
		return comparison < 0 || (comparison == 0 && left < right);
	});
	return files;
}

inline void findAsync(QStringList directories, QObject* receiver,
                      std::function<void(QStringList)> onFinished)
{
	auto result = std::make_shared<QStringList>();
	auto* thread = QThread::create([directories = std::move(directories), result]() {
		*result = find(directories);
	});
	QObject::connect(thread, &QThread::finished, receiver,
	                 [result, onFinished = std::move(onFinished)]() { onFinished(*result); });
	QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
	thread->start();
}
}

#endif
