/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef TESTSTYLECLEANUP_H
#define TESTSTYLECLEANUP_H

#include <QObject>

class TestStyleCleanup : public QObject
{
	Q_OBJECT
private slots:
	void configuredGenerators();
	void transitiveGrepDependencies();
	void emptyDocumentDefaults();
	void managerConfirmation();
	void cleanup();
};

#endif
