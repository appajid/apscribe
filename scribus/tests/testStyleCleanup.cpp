/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QTemporaryDir>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QtTest>
#include "ui_stylemanager.h"

#include "testStyleCleanup.h"
#include "appmodes.h"
#include "commonstrings.h"
#include "grepstylerules.h"
#include "notesstyles.h"
#include "pageitem_textframe.h"
#include "scribus.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "ui/stylemanager.h"

namespace
{
ScribusDoc* newDocument()
{
	return ScCore->primaryMainWindow()->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, 0, 0, 0, 0, 1, QSizeF(), true);
}

void addParagraphs(ScribusDoc* doc, const QStringList& names, const QString& parent)
{
	StyleSet<ParagraphStyle> styles;
	for (const QString& name : names)
	{
		ParagraphStyle style;
		style.setName(name);
		style.setParent(parent);
		styles.create(style);
	}
	doc->redefineStyles(styles, false);
}
}

void TestStyleCleanup::cleanup()
{
	ScCore->init(false, {});
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	if (window->HaveDoc)
	{
		window->doc->appMode = modeNormal;
		window->doc->setModified(false);
		window->slotFileClose();
	}
}

void TestStyleCleanup::configuredGenerators()
{
	ScribusDoc* doc = newDocument();
	QVERIFY(doc);
	addParagraphs(doc, {"Generator Base", "Unused Base"}, CommonStrings::DefaultParagraphStyle);
	const QStringList referenced {
		"TOC Source", "TOC Entry", "Attribute TOC", "Index Heading",
		"Index Separator", "Index Level 1", "Index Level 2", "Index Level 3", "Note Body"
	};
	addParagraphs(doc, referenced, "Generator Base");
	addParagraphs(doc, {"Unused Child"}, "Unused Base");
	StyleSet<CharStyle> characters;
	CharStyle noteMark;
	noteMark.setName("Note Mark");
	noteMark.setParent(CommonStrings::DefaultCharacterStyle);
	noteMark.setFontSize(180);
	characters.create(noteMark);
	CharStyle unused(noteMark);
	unused.setName("Unused Character");
	characters.create(unused);
	doc->redefineCharStyles(characters, false);

	ToCSetup toc {};
	toc.name = "Prepared TOC";
	toc.tocSource = "Style";
	toc.frameName = "Future TOC Frame";
	toc.entryData.append({"TOC Source", "TOC Entry", false, End});
	doc->appendToTocSetups(toc);
	ToCSetup attributeToc {};
	attributeToc.name = "Attribute Contents";
	attributeToc.tocSource = "Attribute";
	attributeToc.textStyle = "Attribute TOC";
	doc->appendToTocSetups(attributeToc);
	IndexSetup index;
	index.name = "Prepared Index";
	index.headingStyle = "Index Heading";
	index.separatorStyle = "Index Separator";
	index.level1Style = "Index Level 1";
	index.level2Style = "Index Level 2";
	index.level3Style = "Index Level 3";
	doc->appendToIndexSetups(index);
	NotesStyle notes;
	notes.setName("Prepared Notes");
	notes.setNotesParStyle("Note Body");
	notes.setMarksCharStyle("Note Mark");
	QVERIFY(doc->newNotesStyle(notes));

	// No generated text exists yet. These are still live document references.
	doc->setModified(false);
	QVERIFY(doc->removeUnusedStyles() >= 3);
	for (const QString& name : referenced)
		QVERIFY2(doc->paragraphStyles().contains(name), qPrintable(name));
	QVERIFY(doc->paragraphStyles().contains("Generator Base"));
	QVERIFY(doc->charStyles().contains("Note Mark"));
	QVERIFY(!doc->paragraphStyles().contains("Unused Base"));
	QVERIFY(!doc->paragraphStyles().contains("Unused Child"));
	QVERIFY(!doc->charStyles().contains("Unused Character"));
	QVERIFY(doc->isModified());
	doc->setModified(false);
	QCOMPARE(doc->removeUnusedStyles(), 0);
	QVERIFY(!doc->isModified());

	QTemporaryDir output;
	QVERIFY(output.isValid());
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	const QString path = output.filePath("cleanup.sla.gz");
	QVERIFY(window->DoFileSave(path));
	doc->setModified(false);
	QVERIFY(window->slotFileClose());
	QVERIFY(window->loadDoc(path));
	doc = window->doc;
	for (const QString& name : referenced)
		QVERIFY2(doc->paragraphStyles().contains(name), qPrintable(name));
	QCOMPARE(doc->tocSetups().first().entryData.first().styleToFind, QString("TOC Source"));
	QCOMPARE(doc->tocSetups().first().entryData.first().styleForText, QString("TOC Entry"));
	QCOMPARE(doc->indexSetups().first().headingStyle, QString("Index Heading"));
	QCOMPARE(doc->getNotesStyle("Prepared Notes")->notesParStyle(), QString("Note Body"));
	QCOMPARE(doc->charStyles().get("Note Mark").fontSize(), 180.0);
	QCOMPARE(doc->removeUnusedStyles(), 0);

	// Removing the settings releases their styles and entire orphaned chains.
	doc->clearTocSetups();
	doc->clearIndexSetups();
	doc->getNotesStyle("Prepared Notes")->setNotesParStyle("");
	doc->getNotesStyle("Prepared Notes")->setMarksCharStyle("");
	QVERIFY(doc->removeUnusedStyles() > 0);
	for (const QString& name : referenced)
		QVERIFY(!doc->paragraphStyles().contains(name));
	QVERIFY(!doc->paragraphStyles().contains("Generator Base"));
	QVERIFY(!doc->charStyles().contains("Note Mark"));
}

void TestStyleCleanup::transitiveGrepDependencies()
{
	ScribusDoc* doc = newDocument();
	QVERIFY(doc);
	StyleSet<CharStyle> characters;
	CharStyle base;
	base.setName("GREP Base");
	base.setParent(CommonStrings::DefaultCharacterStyle);
	base.setFontSize(210);
	characters.create(base);
	CharStyle child;
	child.setName("GREP Emphasis");
	child.setParent(base.name());
	characters.create(child);
	doc->redefineCharStyles(characters, false);
	StyleSet<ParagraphStyle> paragraphs;
	ParagraphStyle parent;
	parent.setName("Used Parent");
	parent.setParent(CommonStrings::DefaultParagraphStyle);
	parent.setGrepStyleRulesJson(GrepStyleRules::serialize({{"NEVER_MATCH", child.name(), false}}));
	paragraphs.create(parent);
	ParagraphStyle body;
	body.setName("Used Child");
	body.setParent(parent.name());
	// The parent remains a valid named style even though this child overrides
	// its rules. Its GREP character style and that style's ancestor must survive.
	body.setGrepStyleRulesJson("");
	paragraphs.create(body);
	doc->redefineStyles(paragraphs, false);
	addParagraphs(doc, {"Unused"}, CommonStrings::DefaultParagraphStyle);
	const int index = doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified,
		50, 50, 300, 100, 0, CommonStrings::None, CommonStrings::None);
	QVERIFY(index >= 0);
	PageItem_TextFrame* frame = doc->Items->at(index)->asTextFrame();
	QVERIFY(frame);
	frame->itemText.insertChars(0, QStringLiteral("Plain body text"));
	ParagraphStyle assigned;
	assigned.setParent(body.name());
	frame->itemText.setDefaultStyle(assigned);
	doc->removeUnusedStyles();
	QVERIFY(doc->paragraphStyles().contains(parent.name()));
	QVERIFY(doc->paragraphStyles().contains(body.name()));
	QVERIFY(doc->charStyles().contains(child.name()));
	QVERIFY(doc->charStyles().contains(base.name()));
	QCOMPARE(doc->charStyles().get(child.name()).fontSize(), 210.0);
	QCOMPARE(frame->itemText.text(0, frame->itemText.length()), QString("Plain body text"));
	QCOMPARE(frame->itemText.defaultStyle().parent(), body.name());
	QVERIFY(!doc->paragraphStyles().contains("Unused"));
}

void TestStyleCleanup::emptyDocumentDefaults()
{
	ScribusDoc* doc = newDocument();
	QVERIFY(doc);
	StyleSet<CharStyle> characters;
	CharStyle emphasis;
	emphasis.setName("Default Rule Emphasis");
	emphasis.setParent(CommonStrings::DefaultCharacterStyle);
	characters.create(emphasis);
	doc->redefineCharStyles(characters, false);
	StyleSet<ParagraphStyle> paragraphs;
	ParagraphStyle defaultStyle(doc->paragraphStyles().get(CommonStrings::DefaultParagraphStyle));
	defaultStyle.setGrepStyleRulesJson(GrepStyleRules::serialize({{"future", emphasis.name(), false}}));
	paragraphs.create(defaultStyle);
	doc->redefineStyles(paragraphs, false);
	doc->setModified(false);
	QCOMPARE(doc->removeUnusedStyles(), 0);
	QVERIFY(doc->charStyles().contains(emphasis.name()));
	QVERIFY(doc->docLineStyles.contains(CommonStrings::DefaultLineStyle));
	QVERIFY(doc->tableStyles().contains(CommonStrings::DefaultTableStyle));
	QVERIFY(doc->cellStyles().contains(CommonStrings::DefaultCellStyle));
	QVERIFY(!doc->isModified());
}

void TestStyleCleanup::managerConfirmation()
{
	ScribusDoc* doc = newDocument();
	QVERIFY(doc);
	addParagraphs(doc, {"Future TOC", "Unused A", "Unused B"}, CommonStrings::DefaultParagraphStyle);
	ToCSetup toc {};
	toc.name = "Future Contents";
	toc.tocSource = "Attribute";
	toc.textStyle = "Future TOC";
	doc->appendToTocSetups(toc);
	StyleManager* manager = ScCore->primaryMainWindow()->styleMgr();
	manager->reloadStyles();
	QPushButton* button = manager->findChild<QPushButton*>("deleteUnusedButton");
	QVERIFY(button);
	QVERIFY(button->isEnabled());
	doc->setModified(false);

	// Enable actual Qt dialogs only for this test, after the isolated app has
	// initialized. Drive the real confirmation buttons, not a stubbed result.
	ScCore->init(true, {});
	QMessageBox::StandardButton answer = QMessageBox::No;
	bool seen = false;
	bool defaultIsNo = false;
	QString summary;
	QTimer response;
	connect(&response, &QTimer::timeout, this, [&]() {
		auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
		if (!dialog)
			return;
		seen = true;
		defaultIsNo = dialog->defaultButton() == dialog->button(QMessageBox::No);
		summary = dialog->text();
		dialog->button(answer)->click();
	});
	response.start(0);
	button->click();
	response.stop();
	QVERIFY(seen);
	QVERIFY(defaultIsNo);
	QVERIFY(summary.contains("Unused A"));
	QVERIFY(summary.contains("Unused B"));
	QVERIFY(!summary.contains("Future TOC"));
	QVERIFY(doc->paragraphStyles().contains("Unused A"));
	QVERIFY(doc->paragraphStyles().contains("Unused B"));
	QVERIFY(!doc->isModified());

	answer = QMessageBox::Yes;
	seen = false;
	response.start(0);
	button->click();
	response.stop();
	QVERIFY(seen);
	QVERIFY(doc->paragraphStyles().contains("Future TOC"));
	QCOMPARE(doc->tocSetups().first().textStyle, QString("Future TOC"));
	QVERIFY(!doc->paragraphStyles().contains("Unused A"));
	QVERIFY(!doc->paragraphStyles().contains("Unused B"));
	QVERIFY(doc->isModified());
}
