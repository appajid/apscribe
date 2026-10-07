/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef TESTDOCUMENTZOOM_H
#define TESTDOCUMENTZOOM_H

#include <QObject>

class TestDocumentZoom : public QObject
{
	Q_OBJECT
private slots:
	void longDocument_data();
	void longDocument();
	void selectionCenteredZoom();
	void cleanup();
};

#endif
