/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef TESTTABLEPARAGRAPHSTYLE_H
#define TESTTABLEPARAGRAPHSTYLE_H

#include <QObject>

class TestTableParagraphStyle : public QObject
{
	Q_OBJECT
private slots:
	void selectedCellsAndTextCursor();
	void cleanup();
};

#endif
