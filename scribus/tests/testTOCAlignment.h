/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef TESTTOCALIGNMENT_H
#define TESTTOCALIGNMENT_H
#include <QObject>

class TestTOCAlignment : public QObject
{
	Q_OBJECT
private slots:
	void generation_data();
	void generation();
	void mixedEntries();
	void wrappedAndColumnFlow_data();
	void wrappedAndColumnFlow();
	void preferences();
	void persistence_data();
	void persistence();
	void cleanup();
};
#endif
