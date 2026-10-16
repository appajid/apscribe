/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest>
#include <QVBoxLayout>

#include "ui/widgets/stacked_container.h"

class InspectorStackTests : public QObject
{
	Q_OBJECT
private slots:
	void currentPageControlsMinimum();
	void emptyStack();
};

void InspectorStackTests::currentPageControlsMinimum()
{
	QWidget host;
	auto* layout = new QVBoxLayout(&host);
	auto* stack = new StackedContainer(&host);
	layout->addWidget(stack);
	auto* compact = new QWidget;
	compact->setMinimumSize(160, 100);
	auto* large = new QWidget;
	large->setMinimumSize(600, 900);
	stack->addWidget(compact);
	stack->addWidget(large);
	host.show();
	QCoreApplication::processEvents();
	QVERIFY2(stack->minimumWidth() <= 160 && stack->minimumHeight() <= 100,
		"A hidden inspector page imposed its minimum on the active page");
	host.resize(220, 180);
	QVERIFY(host.width() <= 220 && host.height() <= 180);
	stack->setCurrentWidget(large);
	QCoreApplication::processEvents();
	QVERIFY(host.minimumWidth() >= 600 && host.minimumHeight() >= 900);
	stack->setCurrentWidget(compact);
	QCoreApplication::processEvents();
	host.resize(220, 180);
	QVERIFY(host.width() <= 220 && host.height() <= 180);
	// Active-page hints remain responsive when a section expands/collapses.
	compact->setMinimumSize(180, 140);
	QCoreApplication::processEvents();
	QTRY_VERIFY(host.minimumWidth() >= 180 && host.minimumHeight() >= 140);
}

void InspectorStackTests::emptyStack()
{
	StackedContainer stack;
	QVERIFY(!stack.minimumSizeHint().isValid() || stack.minimumSizeHint().isEmpty());
	QVERIFY(!stack.sizeHint().isValid() || stack.sizeHint().isEmpty());
	QWidget page;
	stack.addWidget(&page);
	stack.removeWidget(&page);
	QVERIFY(!stack.minimumSizeHint().isValid() || stack.minimumSizeHint().isEmpty());
}

QTEST_MAIN(InspectorStackTests)
#include "inspectorstacktests.moc"
