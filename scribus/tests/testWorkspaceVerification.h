/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef TESTWORKSPACEVERIFICATION_H
#define TESTWORKSPACEVERIFICATION_H

#include <QObject>

class TestWorkspaceVerification : public QObject
{
	Q_OBJECT
private slots:
	void appearanceControlsChangePalette();
	void inspectorHiddenPageSizing();
	void anchoredCanvasEditing_data();
	void anchoredCanvasEditing();
	void cleanup();
};

#endif
