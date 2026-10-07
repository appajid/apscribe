/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QWheelEvent>
#include <QtTest>

#include "testDocumentZoom.h"
#include "appmodes.h"
#include "canvas.h"
#include "commonstrings.h"
#include "prefsmanager.h"
#include "scpage.h"
#include "scribus.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "scribusview.h"
#include "selection.h"
#include "ui/scrspinbox.h"

void TestDocumentZoom::cleanup()
{
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	if (window->HaveDoc)
	{
		window->doc->appMode = modeNormal;
		window->doc->setModified(false);
		window->slotFileClose();
	}
}

void TestDocumentZoom::longDocument_data()
{
	QTest::addColumn<int>("pageCount");
	QTest::addColumn<int>("arrangement");
	QTest::addColumn<int>("pageIndex");
	QTest::newRow("single-first") << 200 << 0 << 0;
	QTest::newRow("single-middle") << 200 << 0 << 100;
	QTest::newRow("single-last") << 200 << 0 << 199;
	QTest::newRow("facing-last") << 200 << 1 << 199;
	QTest::newRow("thousand-first") << 1000 << 0 << 0;
	QTest::newRow("thousand-middle") << 1000 << 0 << 500;
	QTest::newRow("thousand-last") << 1000 << 0 << 999;
}

void TestDocumentZoom::longDocument()
{
	QFETCH(int, pageCount);
	QFETCH(int, arrangement);
	QFETCH(int, pageIndex);
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	window->resize(1200, 800);
	window->show();
	ScribusDoc* doc = window->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, arrangement, 0, 0, 0, 1, QSizeF(), true, pageCount);
	QVERIFY(doc);
	QCOMPARE(doc->Pages->count(), pageCount);
	ScribusView* view = doc->view();
	QVERIFY(view);
	QCoreApplication::processEvents();
	QVERIFY(view->viewport()->width() > 300);
	QVERIFY(view->viewport()->height() > 300);
	const ScPage* page = doc->Pages->at(pageIndex);
	const QPointF target(page->xOffset() + page->width() / 2,
		page->yOffset() + page->height() / 2);
	view->zoom(target.x(), target.y(), 1.0, false);
	doc->setModified(false);

	// Ordinary zoom commands must preserve the visible centre when no object
	// is selected, even far down the document or after pasteboard expansion.
	for (int i = 0; i < 24; ++i)
	{
		const QPointF before = view->visibleCanvasRect().center();
		if (i < 12)
			view->slotZoomIn();
		else
			view->slotZoomOut();
		const QPointF after = view->visibleCanvasRect().center();
		const double tolerance = 2.0 / view->scale();
		QVERIFY2(qAbs(after.x() - before.x()) <= tolerance &&
			qAbs(after.y() - before.y()) <= tolerance,
			qPrintable(QString("Zoom command moved centre from %1,%2 to %3,%4")
				.arg(before.x()).arg(before.y()).arg(after.x()).arg(after.y())));
	}

	// Exercise the real status-bar spinbox connection, not just zoom().
	const double displayScale = PrefsManager::instance().appPrefs.displayPrefs.displayScale;
	for (double percent : {25.0, 150.0, 50.0, 100.0, 3200.0, 32000.0, 32000.0, 100.0})
	{
		const QPointF before = view->visibleCanvasRect().center();
		const double oldScale = view->scale();
		window->zoomSpinBox->setValue(percent);
		if (percent <= 150.0)
		{
			QVERIFY(view->scale() != oldScale);
			QVERIFY(qAbs(view->scale() - percent / 100.0 * displayScale) < 0.001);
		}
		// The control must report the actual zoom, including a canvas-size cap.
		QVERIFY(qAbs(window->zoomSpinBox->value() - view->scale() / displayScale * 100.0) < 0.01);
		const QPointF after = view->visibleCanvasRect().center();
		const double tolerance = 2.0 / view->scale();
		QVERIFY(qAbs(after.x() - before.x()) <= tolerance);
		QVERIFY2(qAbs(after.y() - before.y()) <= tolerance,
			qPrintable(QString("%1% zoom moved centre from %2 to %3 (canvas height %4)")
				.arg(percent).arg(before.y()).arg(after.y()).arg(view->m_canvas->height())));
		if (percent > 150.0)
		{
			// Zooming further at the size limit must also preserve the pointer
			// anchor, including when the effective scale cannot increase.
			const QPoint pointer(135, 165);
			const QPoint global = view->viewport()->mapToGlobal(pointer);
			const FPoint anchor = view->m_canvas->globalToCanvas(global);
			QWheelEvent wheel(pointer, global, QPoint(), QPoint(0, 120),
				Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
			QApplication::sendEvent(view->viewport(), &wheel);
			QVERIFY(wheel.isAccepted());
			const FPoint result = view->m_canvas->globalToCanvas(global);
			QVERIFY(qAbs(result.x() - anchor.x()) <= 2.0 / view->scale());
			QVERIFY(qAbs(result.y() - anchor.y()) <= 2.0 / view->scale());
		}
	}

	// A wheel zoom must leave the canvas point under the pointer stationary.
	const QPoint pointer(135, 165);
	const QPoint global = view->viewport()->mapToGlobal(pointer);
	for (int delta : {120, 240, -120, -240})
	{
		const FPoint before = view->m_canvas->globalToCanvas(global);
		const double oldScale = view->scale();
		QWheelEvent wheel(pointer, global, QPoint(), QPoint(0, delta),
			Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
		QApplication::sendEvent(view->viewport(), &wheel);
		QVERIFY(wheel.isAccepted());
		QVERIFY(view->scale() != oldScale);
		const FPoint after = view->m_canvas->globalToCanvas(global);
		const double tolerance = 2.0 / view->scale();
		QVERIFY(qAbs(after.x() - before.x()) <= tolerance);
		QVERIFY(qAbs(after.y() - before.y()) <= tolerance);
	}
	QVERIFY(!doc->isModified());
}

void TestDocumentZoom::selectionCenteredZoom()
{
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	ScribusDoc* doc = window->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, 0, 0, 0, 0, 1, QSizeF(), true);
	QVERIFY(doc);
	ScribusView* view = doc->view();
	QCoreApplication::processEvents();
	const int index = doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified,
		100, 150, 120, 80, 0, CommonStrings::None, CommonStrings::None);
	QVERIFY(index >= 0);
	PageItem* item = doc->Items->at(index);
	doc->m_Selection->addItem(item);
	doc->setModified(false);
	view->slotZoomIn();
	const QRectF selection = doc->m_Selection->getVisualGroupRect();
	const QPointF center = view->visibleCanvasRect().center();
	QVERIFY(qAbs(center.x() - selection.center().x()) <= 2.0 / view->scale());
	QVERIFY(qAbs(center.y() - selection.center().y()) <= 2.0 / view->scale());
	QCOMPARE(doc->m_Selection->count(), 1);
	QCOMPARE(doc->m_Selection->itemAt(0), item);
	QVERIFY(!doc->isModified());
}
