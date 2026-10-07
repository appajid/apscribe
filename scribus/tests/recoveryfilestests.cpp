/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include "recoveryfiles.h"

class RecoveryFilesTests : public QObject
{
	Q_OBJECT

private slots:
	void findsBothRecoveryTypesOnceWithoutScanningSubdirectories();
	void reportsAsyncResultsOnTheUiThread();
};

void RecoveryFilesTests::findsBothRecoveryTypesOnceWithoutScanningSubdirectories()
{
	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	const QDir root(directory.path());
	const QString emergency = root.filePath(QStringLiteral("book_emergency_123.sla"));
	const QString autosave = root.filePath(QStringLiteral("book_autosave_456.sla"));
	const QString unrelated = root.filePath(QStringLiteral("book.sla"));
	for (const QString& path : { emergency, autosave, unrelated })
	{
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
	}
	QVERIFY(root.mkdir(QStringLiteral("nested")));
	QFile nested(root.filePath(QStringLiteral("nested/book_autosave_789.sla")));
	QVERIFY(nested.open(QIODevice::WriteOnly));
	QVERIFY(root.mkdir(QStringLiteral("directory_emergency_111.sla")));

	const QStringList files = RecoveryFiles::find({ directory.path(), directory.path(),
	                                                QString(), root.filePath(QStringLiteral("missing")) });
	QCOMPARE(QSet<QString>(files.begin(), files.end()), QSet<QString>({ emergency, autosave }));
	QCOMPARE(files.count(), 2);

	const QString shortcut = root.filePath(QStringLiteral("link_autosave_999.sla"));
	if (QFile::link(autosave, shortcut))
	{
		const QStringList withoutLinks = RecoveryFiles::find({ directory.path() });
		QVERIFY(!withoutLinks.contains(shortcut));
	}
}

void RecoveryFilesTests::reportsAsyncResultsOnTheUiThread()
{
	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	const QString path = QDir(directory.path()).filePath(QStringLiteral("book_autosave_123.sla"));
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.close();

	QObject receiver;
	QEventLoop loop;
	QStringList files;
	bool called = false;
	RecoveryFiles::findAsync({ directory.path() }, &receiver, [&](QStringList found) {
		called = true;
		files = std::move(found);
		QCOMPARE(QThread::currentThread(), QCoreApplication::instance()->thread());
		loop.quit();
	});
	QTimer::singleShot(5000, &loop, &QEventLoop::quit);
	loop.exec();
	QVERIFY(called);
	QCOMPARE(files, QStringList({ path }));
}

QTEST_GUILESS_MAIN(RecoveryFilesTests)
#include "recoveryfilestests.moc"
