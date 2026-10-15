/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef TESTSLALOADVALIDATION_H
#define TESTSLALOADVALIDATION_H
#include <QObject>

class TestSLALoadValidation : public QObject
{
	Q_OBJECT
private slots:
	void incompleteDocument_data();
	void incompleteDocument();
	void validDocuments_data();
	void validDocuments();
	void rejectsAndRecovers_data();
	void rejectsAndRecovers();
	void cleanup();
};
#endif
