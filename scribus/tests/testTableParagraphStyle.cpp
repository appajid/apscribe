/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QTemporaryDir>
#include <QtTest>
#include "ui_propertywidget_paragraphstyle.h"

#include "testTableParagraphStyle.h"
#include "appmodes.h"
#include "commonstrings.h"
#include "pageitem_table.h"
#include "pageitem_textframe.h"
#include "scribus.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "selection.h"
#include "ui/propertywidget_paragraphstyle.h"
#include "ui/contentpalette.h"
#include "ui/stylecombos.h"
#include "undomanager.h"

void TestTableParagraphStyle::cleanup()
{
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	if (window->HaveDoc)
	{
		window->doc->appMode = modeNormal;
		window->doc->setModified(false);
		window->slotFileClose();
	}
}

void TestTableParagraphStyle::selectedCellsAndTextCursor()
{
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	ScribusDoc* doc = window->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, 0, 0, 0, 0, 1, QSizeF(), true);
	QVERIFY(doc);
	ParagraphStyle body;
	body.setName("Table body");
	body.setParent(CommonStrings::DefaultParagraphStyle);
	body.setAlignment(ParagraphStyle::LeftAligned);
	ParagraphStyle heading;
	heading.setName("Table heading");
	heading.setParent(CommonStrings::DefaultParagraphStyle);
	heading.setAlignment(ParagraphStyle::Centered);
	StyleSet<ParagraphStyle> styles;
	styles.create(body);
	styles.create(heading);
	doc->redefineStyles(styles, false);

	const int index = doc->itemAdd(PageItem::Table, PageItem::Unspecified,
		50, 50, 300, 180, 0, CommonStrings::None, CommonStrings::None);
	QVERIFY(index >= 0);
	PageItem_Table* table = doc->Items->at(index)->asTable();
	QVERIFY(table);
	table->setItemName("Style test table");
	table->insertRows(0, 1);
	table->insertColumns(0, 2);
	for (int row = 0; row < 2; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			StoryText& story = table->cellAt(row, column).textFrame()->itemText;
			story.insertChars(0, QStringLiteral("First") + SpecialChars::PARSEP + "Second");
			ParagraphStyle initial;
			initial.setParent(body.name());
			if (row == 1 && column == 0)
				initial.setGapAfter(9.0);
			story.setDefaultStyle(initial);
			story.applyStyle(0, initial);
			story.applyStyle(story.length(), initial);
			CharStyle emphasis;
			emphasis.setFontSize(140.0);
			story.applyCharStyle(0, 4, emphasis);
		}
	}
	// An empty selected cell also needs the style for subsequently typed text.
	table->cellAt(1, 1).textFrame()->itemText.clear();
	ParagraphStyle emptyStyle;
	emptyStyle.setParent(body.name());
	table->cellAt(1, 1).textFrame()->itemText.setDefaultStyle(emptyStyle);
	doc->m_Selection->clear();
	doc->m_Selection->addItem(table);
	doc->appMode = modeEditTable;
	table->moveTo(table->cellAt(0, 0));
	table->selectCells(0, 0, 1, 1);

	PropertyWidget_ParagraphStyle inspector;
	inspector.setMainWindow(window);
	inspector.setDoc(doc);
	inspector.handleSelectionChanged();
	ParaStyleComboBox* combo = inspector.findChild<ParaStyleComboBox*>("paraStyleCombo");
	QVERIFY(combo);
	window->contentPalette->handleSelectionChanged();
	ParaStyleComboBox* tableCombo = window->contentPalette->findChild<ParaStyleComboBox*>("cellParagraphStyleCombo");
	QVERIFY(tableCombo);
	QVERIFY(tableCombo->isEnabled());
	QVERIFY(!tableCombo->parentWidget()->isHidden());
	doc->setModified(false);
	// Exercise the inspector's real signal-to-command connection, not a mock.
	tableCombo->newStyle(heading.name());
	for (int row = 0; row < 2; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			const StoryText& story = table->cellAt(row, column).textFrame()->itemText;
			const QString expected = column < 2 ? heading.name() : body.name();
			QCOMPARE(story.defaultStyle().parent(), expected);
			QCOMPARE(story.length(), row == 1 && column == 1 ? 0 : 12);
			if (story.length() > 0)
			{
				QCOMPARE(story.paragraphStyle(0).parent(), expected);
				QCOMPARE(story.paragraphStyle(story.length()).parent(), expected);
				QCOMPARE(story.charStyle(0).fontSize(), 140.0);
			}
			else
				QCOMPARE(story.paragraphStyle(0).alignment(), int(ParagraphStyle::Centered));
			if (row == 1 && column == 0)
				// Named styles replace paragraph overrides, as in ordinary frames.
				QCOMPARE(story.paragraphStyle(0).gapAfter(), heading.gapAfter());
		}
	}
	QCOMPARE(table->selectedCells().size(), 4);
	QCOMPARE(doc->m_Selection->count(), 1);
	QCOMPARE(doc->m_Selection->itemAt(0), static_cast<PageItem*>(table));
	QCOMPARE(doc->appMode, int(modeEditTable));
	QVERIFY(doc->isModified());
	UndoManager::instance()->undo(1);
	for (const TableCell& cell : table->selectedCells())
	{
		QCOMPARE(cell.textFrame()->itemText.defaultStyle().parent(), body.name());
		if (cell.textFrame()->itemText.length() > 0)
		{
			QCOMPARE(cell.textFrame()->itemText.paragraphStyle(0).parent(), body.name());
			QCOMPARE(cell.textFrame()->itemText.paragraphStyle(cell.textFrame()->itemText.length()).parent(), body.name());
		}
	}
	QCOMPARE(table->cellAt(1, 0).textFrame()->itemText.paragraphStyle(0).gapAfter(), 9.0);
	UndoManager::instance()->redo(1);
	for (const TableCell& cell : table->selectedCells())
		QCOMPARE(cell.textFrame()->itemText.paragraphStyle(0).alignment(), int(ParagraphStyle::Centered));

	// Without a cell-range selection, retain the ordinary current-paragraph
	// behavior and leave the second paragraph and the default style unchanged.
	table->clearSelection();
	table->moveTo(table->cellAt(0, 0));
	table->activeCell().textFrame()->itemText.setCursorPosition(0);
	combo->newStyle(body.name());
	const StoryText& active = table->activeCell().textFrame()->itemText;
	QCOMPARE(active.paragraphStyle(0).parent(), body.name());
	QCOMPARE(active.paragraphStyle(active.length()).parent(), heading.name());
	QCOMPARE(active.defaultStyle().parent(), heading.name());
	QVERIFY(tableCombo->parentWidget()->isHidden());
	table->selectCells(0, 0, 0, 1);
	QVERIFY(!tableCombo->parentWidget()->isHidden());
	QCOMPARE(tableCombo->currentIndex(), -1);
	QCOMPARE(tableCombo->placeholderText(), QString("Mixed paragraph styles"));
	table->clearSelection();

	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	const QString path = directory.filePath("table-styles.sla");
	doc->appMode = modeNormal;
	QVERIFY(window->DoFileSave(path));
	inspector.setDoc(nullptr);
	doc->setModified(false);
	QVERIFY(window->slotFileClose());
	QVERIFY(window->loadDoc(path));
	table = window->doc->Items->at(0)->asTable();
	QVERIFY(table);
	for (int row = 0; row < 2; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			const StoryText& story = table->cellAt(row, column).textFrame()->itemText;
			const QString expected = column < 2 ? heading.name() : body.name();
			QCOMPARE(story.defaultStyle().parent(), expected);
			if (story.length() > 0)
			{
				QCOMPARE(story.paragraphStyle(story.length()).parent(), expected);
				QCOMPARE(story.paragraphStyle(0).parent(), row == 0 && column == 0 ? body.name() : expected);
			}
			else
				QCOMPARE(story.paragraphStyle(0).alignment(), int(ParagraphStyle::Centered));
		}
	}

	// A merged range resolves to one story and must not be applied repeatedly.
	doc = window->doc;
	table->mergeCells(1, 0, 1, 2);
	doc->m_Selection->clear();
	doc->m_Selection->addItem(table);
	doc->appMode = modeEditTable;
	table->moveTo(table->cellAt(1, 0));
	table->selectCells(1, 0, 1, 1);
	QCOMPARE(table->selectedCells().size(), 1);
	window->setNewParStyle(body.name());
	const StoryText& merged = table->cellAt(1, 0).textFrame()->itemText;
	QCOMPARE(merged.defaultStyle().parent(), body.name());
	QCOMPARE(merged.paragraphStyle(0).parent(), body.name());
	QCOMPARE(merged.paragraphStyle(merged.length()).parent(), body.name());
	QCOMPARE(table->cellAt(0, 1).textFrame()->itemText.defaultStyle().parent(), heading.name());
	UndoManager::instance()->undo(1);
	QCOMPARE(table->cellAt(1, 0).columnSpan(), 2);
	QCOMPARE(merged.defaultStyle().parent(), heading.name());
}
