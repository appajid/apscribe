/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef TESTSTYLESETTINGS_H
#define TESTSTYLESETTINGS_H

#include <QObject>

class TestStyleSettings : public QObject
{
	Q_OBJECT
private slots:
	void rename_data();
	void rename();
	void replacement_data();
	void replacement();
	void resetCharacterThenApply_data();
	void resetCharacterThenApply();
	void cleanup();
};

#endif
