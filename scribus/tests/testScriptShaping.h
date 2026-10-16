/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef TESTSCRIPTSHAPING_H
#define TESTSCRIPTSHAPING_H
#include <QObject>

class TestScriptShaping : public QObject
{
	Q_OBJECT
private slots:
	void shaping_data();
	void shaping();
	void mixedDirectionAndPersistence();
	void directionInspector_data();
	void directionInspector();
	void cleanup();
};
#endif
