/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include "testWorkspaceVerification.h"
#include "appmodes.h"
#include "canvas.h"
#include "commonstrings.h"
#include "pageitem_table.h"
#include "pageitem_textframe.h"
#include "scribus.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "scribusview.h"
#include "selection.h"
#include "ui/contentpalette.h"
#include "ui/propertiespalette_text.h"
#include "ui/scrspinbox.h"
#include "undomanager.h"

namespace
{
ScribusDoc* newDocument()
{
	auto* window = ScCore->primaryMainWindow();
	window->resize(1200, 900);
	window->show();
	return window->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, 0, 0, 0, 0, 1, QSizeF(), true);
}

void mouse(ScribusView* view, QEvent::Type type, const QPointF& canvasPoint)
{
	const QPointF local = view->m_canvas->canvasToLocalF(canvasPoint);
	const QPointF global = view->m_canvas->canvasToGlobalF(canvasPoint);
	QMouseEvent event(type, local, global,
		type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
		type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(view->m_canvas, &event);
	QCoreApplication::processEvents();
}

void drag(ScribusView* view, const QPointF& from, const QPointF& to)
{
	mouse(view, QEvent::MouseButtonPress, from);
	mouse(view, QEvent::MouseMove, to);
	mouse(view, QEvent::MouseButtonRelease, to);
}

QTransform anchorTransform(PageItem_TextFrame* story)
{
	QTransform transform = story->getTransform();
	if (story->imageFlippedH())
	{
		transform.translate(story->width(), 0);
		transform.scale(-1, 1);
	}
	if (story->imageFlippedV())
	{
		transform.translate(0, story->height());
		transform.scale(1, -1);
	}
	return transform;
}

QRectF canvasRect(PageItem_TextFrame* story, PageItem* item)
{
	story->layout();
	return anchorTransform(story).mapRect(story->resolvedAnchoredObjectRect(item->inlineCharID));
}
}

void TestWorkspaceVerification::cleanup()
{
	auto* window = ScCore->primaryMainWindow();
	if (window->HaveDoc)
	{
		window->view->requestMode(modeNormal);
		window->doc->setModified(false);
		window->slotFileClose();
	}
}

void TestWorkspaceVerification::inspectorHiddenPageSizing()
{
	QVERIFY(newDocument());
	auto* inspector = ScCore->primaryMainWindow()->contentPalette->findChild<PropertiesPalette_Text*>();
	QVERIFY(inspector);
	auto* tabs = inspector->findChild<QTabBar*>("textInspectorTabs");
	auto* stack = inspector->findChild<QStackedWidget*>("textInspectorStack");
	QVERIFY(tabs && stack);
	tabs->setCurrentIndex(0);
	const QSize before = inspector->minimumSizeHint();
	// Simulate a tall expanded section on the *inactive* Paragraph tab.
	QWidget sentinel;
	sentinel.setMinimumSize(before.width() + 300, before.height() + 500);
	stack->widget(1)->layout()->addWidget(&sentinel);
	QCoreApplication::processEvents();
	QCOMPARE(inspector->minimumSizeHint(), before);
	tabs->setCurrentIndex(1);
	QVERIFY(inspector->minimumSizeHint().height() >= sentinel.minimumHeight());
	tabs->setCurrentIndex(0);
	QCOMPARE(inspector->minimumSizeHint(), before);
	stack->widget(1)->layout()->removeWidget(&sentinel);
	sentinel.setParent(nullptr);
}

void TestWorkspaceVerification::anchoredCanvasEditing_data()
{
	QTest::addColumn<bool>("table");
	QTest::addColumn<int>("mode");
	QTest::addColumn<int>("transform");
	for (bool table : {false, true})
		for (int mode : {0, 1, 2})
			for (int transform : {0, 1, 2})
				QTest::newRow(qPrintable(QString("%1-mode-%2-transform-%3").arg(table ? "table" : "image").arg(mode).arg(transform))) << table << mode << transform;
}

void TestWorkspaceVerification::anchoredCanvasEditing()
{
	QFETCH(bool, table);
	QFETCH(int, mode);
	QFETCH(int, transform);
	auto* doc = newDocument();
	QVERIFY(doc);
	auto* window = ScCore->primaryMainWindow();
	auto* view = doc->view();
	const int storyIndex = doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified,
		100, 150, 350, 500, 0, CommonStrings::None, CommonStrings::None);
	QVERIFY(storyIndex >= 0);
	auto* story = doc->Items->at(storyIndex)->asTextFrame();
	if (transform == 1)
		story->setRotation(17);
	if (transform == 2)
	{
		story->setImageFlippedH(true);
		story->setImageFlippedV(true);
	}
	story->itemText.insertChars(0, QStringLiteral("First line") + SpecialChars::PARSEP + "Anchor here and more text following.");
	const int objectIndex = doc->itemAdd(table ? PageItem::Table : PageItem::ImageFrame,
		PageItem::Unspecified, 100, 100, 80, 50, 0, CommonStrings::None, CommonStrings::None);
	QVERIFY(objectIndex >= 0);
	auto* item = doc->Items->at(objectIndex);
	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	if (!table)
	{
		QImage image(80, 50, QImage::Format_RGB32);
		image.fill(QColor(30, 90, 160));
		const QString path = directory.filePath("source.png");
		QVERIFY(image.save(path));
		QVERIFY(doc->loadPict(path, item));
	}
	else
		item->asTable()->cellAt(0, 0).textFrame()->itemText.insertChars(0, QStringLiteral("Cell content"));
	item->isEmbedded = true;
	item->gXpos = item->gYpos = 0;
	item->gWidth = item->width();
	item->gHeight = item->height();
	const int id = doc->addToInlineFrames(item);
	doc->Items->removeAt(objectIndex);
	AnchorPosition position;
	position.mode = static_cast<AnchorPosition::Mode>(mode);
	if (mode == 2)
	{
		position.xOffset = 100;
		position.yOffset = 60;
	}
	item->setAnchorPosition(position);
	story->itemText.insertObject(11, id);
	story->invalidateLayout();
	story->layout();
	view->zoom(280, 300, 1.0, false);
	QCoreApplication::processEvents();
	const QRectF initial = canvasRect(story, item);
	QVERIFY(!initial.isEmpty());
	view->requestMode(modeNormal);
	view->deselectItems();
	mouse(view, QEvent::MouseButtonPress, initial.center());
	mouse(view, QEvent::MouseButtonRelease, initial.center());
	QCOMPARE(doc->appMode, int(modeEdit));
	QCOMPARE(story->selectedAnchoredObject(), item);
	QCOMPARE(doc->m_Selection->itemAt(0), static_cast<PageItem*>(story));
	window->contentPalette->handleSelectionChanged();
	if (table)
	{
		auto* panel = window->contentPalette->findChild<QWidget*>("PropertiesPalette_TableBase");
		QVERIFY(panel && !panel->isHidden());
	}
	else
	{
		auto* panel = window->contentPalette->findChild<QWidget*>("PropertiesPalette_ImageBase");
		QVERIFY(panel && !panel->isHidden());
		auto* offset = panel->findChild<ScrSpinBox*>("imageXOffsetSpinBox");
		QVERIFY(offset && offset->isEnabled());
		const double frameOffset = story->imageXOffset();
		offset->setValue(offset->value() + 5);
		QVERIFY(item->imageXOffset() != 0);
		QCOMPARE(story->imageXOffset(), frameOffset);
	}
	const QPointF delta(26, 19);
	item->setLocked(true);
	drag(view, initial.center(), initial.center() + delta);
	QVERIFY2(QLineF(canvasRect(story, item).topLeft(), initial.topLeft()).length() < 0.1,
		"A locked anchored object moved on the canvas");
	item->setLocked(false);
	position = item->anchorPosition();
	position.preventManualPositioning = true;
	item->setAnchorPosition(position);
	drag(view, initial.center(), initial.center() + delta);
	QVERIFY(QLineF(canvasRect(story, item).topLeft(), initial.topLeft()).length() < 0.1);
	position.preventManualPositioning = false;
	item->setAnchorPosition(position);
	drag(view, initial.center(), initial.center() + delta);
	const QRectF moved = canvasRect(story, item);
	QVERIFY2(QLineF(moved.topLeft(), initial.topLeft() + delta).length() < 1.5,
		qPrintable(QString("Drag failed: initial %1,%2, actual %3,%4").arg(initial.x()).arg(initial.y()).arg(moved.x()).arg(moved.y())));
	QCOMPARE(story->selectedAnchoredObject(), item);
	UndoManager::instance()->undo(1);
	const QRectF undone = canvasRect(story, item);
	QVERIFY(QLineF(undone.topLeft(), initial.topLeft()).length() < 1.5);
	UndoManager::instance()->redo(1);
	QVERIFY(QLineF(canvasRect(story, item).topLeft(), moved.topLeft()).length() < 1.5);
	const QSizeF beforeSize(item->width(), item->height());
	const QTransform frameTransform = anchorTransform(story);
	const QRectF localBefore = story->resolvedAnchoredObjectRect(id);
	const QPointF resizeFrom = frameTransform.map(localBefore.bottomRight());
	const QPointF resizeTo = frameTransform.map(localBefore.bottomRight() + QPointF(24, 18));
	item->setSizeLocked(true);
	const AnchorPosition beforeLockedDrag = item->anchorPosition();
	drag(view, resizeFrom, resizeTo);
	QCOMPARE(QSizeF(item->width(), item->height()), beforeSize);
	// A size lock still permits moving the object. Restore that move before
	// testing the same resize handle with the size lock removed.
	if (!(item->anchorPosition() == beforeLockedDrag))
		UndoManager::instance()->undo(1);
	story->layout();
	QVERIFY(QLineF(story->resolvedAnchoredObjectRect(id).topLeft(), localBefore.topLeft()).length() < 1.5);
	item->setSizeLocked(false);
	view->requestMode(modeEdit);
	story->itemText.deselectAll();
	story->itemText.setCursorPosition(11);
	story->itemText.select(11, 1, true);
	drag(view, resizeFrom, resizeTo);
	const QSizeF afterSize(item->width(), item->height());
	QVERIFY(afterSize.width() > beforeSize.width() + 15);
	QVERIFY(afterSize.height() > beforeSize.height() + 10);
	const QRectF resized = canvasRect(story, item);
	QVERIFY(QLineF(story->resolvedAnchoredObjectRect(id).topLeft(), localBefore.topLeft()).length() < 1.5);
	UndoManager::instance()->undo(1);
	QVERIFY(qAbs(item->width() - beforeSize.width()) < 0.1);
	QVERIFY(qAbs(item->height() - beforeSize.height()) < 0.1);
	UndoManager::instance()->redo(1);
	QVERIFY(qAbs(item->width() - afterSize.width()) < 0.1);
	QVERIFY(qAbs(item->height() - afterSize.height()) < 0.1);
	if (table)
		QCOMPARE(item->asTable()->cellAt(0, 0).textFrame()->itemText.text(0, 12), QString("Cell content"));
	view->requestMode(modeNormal);
	const QString sla = directory.filePath("anchored.sla");
	QVERIFY(window->DoFileSave(sla));
	doc->setModified(false);
	QVERIFY(window->slotFileClose());
	QVERIFY(window->loadDoc(sla));
	doc = window->doc;
	story = doc->Items->at(0)->asTextFrame();
	QVERIFY(story);
	QVERIFY(story->itemText.hasObject(11));
	item = doc->FrameItems.value(story->itemText.object(11).getInlineCharID());
	QVERIFY(item);
	QCOMPARE(item->isTable(), table);
	QVERIFY(QLineF(canvasRect(story, item).topLeft(), resized.topLeft()).length() < 1.5);
	QVERIFY(qAbs(item->width() - afterSize.width()) < 0.1);
}
