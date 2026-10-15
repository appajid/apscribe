/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include "ui_stylemanager.h"

#include "testStyleSettings.h"
#include "appmodes.h"
#include "commonstrings.h"
#include "notesstyles.h"
#include "scribus.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "ui/stylemanager.h"

namespace
{
ScribusDoc* settingsDocument()
{
	ScribusDoc* doc = ScCore->primaryMainWindow()->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, 0, 0, 0, 0, 1, QSizeF(), true);
	if (!doc)
		return nullptr;
	StyleSet<ParagraphStyle> paragraphs;
	for (const QString& name : {QString("Shared Style"), QString("Other Style"), QString("Replacement Style")})
	{
		ParagraphStyle style;
		style.setName(name);
		style.setParent(CommonStrings::DefaultParagraphStyle);
		paragraphs.create(style);
	}
	doc->redefineStyles(paragraphs, false);
	// Deliberately use the same name in both namespaces. Renaming a character
	// style must not rename any paragraph-style reference in the settings.
	StyleSet<CharStyle> characters;
	CharStyle mark;
	mark.setName("Shared Style");
	mark.setParent(CommonStrings::DefaultCharacterStyle);
	mark.setFontSize(180);
	characters.create(mark);
	doc->redefineCharStyles(characters, false);
	ToCSetup toc {};
	toc.name = "Shared Style";
	toc.frameName = "Shared Style";
	toc.itemAttrName = "Shared Style";
	toc.tocSource = "Style";
	toc.textStyle = "Shared Style";
	toc.pageLocation = Beginning;
	toc.entryData.append({"Shared Style", "Shared Style", false, End});
	toc.entryData.append({"Other Style", "Other Style", true, NotShown});
	// These obsolete lists are not persisted. If present in memory, they must
	// still remain untouched by drafts and be updated only at commit time.
	toc.styleListToFind = {"Shared Style", "Other Style", "Shared Style"};
	toc.styleListForTOC = {"Shared Style", "Other Style"};
	doc->appendToTocSetups(toc);
	ToCSetup attributeToc {};
	attributeToc.name = "Attribute Contents";
	attributeToc.tocSource = "Attribute";
	attributeToc.textStyle = "Shared Style";
	doc->appendToTocSetups(attributeToc);
	IndexSetup index;
	index.name = "Shared Style";
	index.frameName = "Shared Style";
	index.headingStyle = "Shared Style";
	index.separatorStyle = "Shared Style";
	index.level1Style = "Shared Style";
	index.level2Style = "Shared Style";
	index.level3Style = "Shared Style";
	index.listNonPrintingFrames = true;
	index.caseSensitiveCombination = true;
	doc->appendToIndexSetups(index);
	NotesStyle notes;
	notes.setName("Prepared Notes");
	notes.setNotesParStyle("Shared Style");
	notes.setMarksCharStyle("Shared Style");
	if (!doc->newNotesStyle(notes))
		return nullptr;
	return doc;
}

QStringList references(ScribusDoc* doc)
{
	const ToCSetup& toc = doc->tocSetups().at(0);
	const IndexSetup& index = doc->indexSetups().first();
	const NotesStyle* notes = doc->getNotesStyle("Prepared Notes");
	return {toc.textStyle, doc->tocSetups().at(1).textStyle,
		toc.entryData.at(0).styleToFind, toc.entryData.at(0).styleForText,
		toc.entryData.at(1).styleToFind, toc.entryData.at(1).styleForText,
		index.headingStyle, index.separatorStyle, index.level1Style, index.level2Style, index.level3Style,
		notes->notesParStyle(), notes->marksChStyle(),
		toc.name, toc.frameName, toc.itemAttrName, index.name, index.frameName};
}

QStringList expectedReferences(const QString& paragraph, const QString& character)
{
	return {paragraph, paragraph, paragraph, paragraph, "Other Style", "Other Style",
		paragraph, paragraph, paragraph, paragraph, paragraph, paragraph, character,
		"Shared Style", "Shared Style", "Shared Style", "Shared Style", "Shared Style"};
}

bool reopen(ScribusDoc* doc, const QString& path)
{
	ScCore->init(false, {});
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	if (!window->DoFileSave(path))
		return false;
	doc->setModified(false);
	return window->slotFileClose() && window->loadDoc(path);
}
}

void TestStyleSettings::cleanup()
{
	ScCore->init(false, {});
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	if (!window->HaveDoc)
		return;
	QPushButton* reset = window->styleMgr()->findChild<QPushButton*>("resetButton");
	if (reset && reset->isEnabled())
		reset->click();
	window->doc->appMode = modeNormal;
	window->doc->setModified(false);
	window->slotFileClose();
}

void TestStyleSettings::rename_data()
{
	QTest::addColumn<bool>("character");
	QTest::addColumn<bool>("apply");
	QTest::newRow("paragraph-apply") << false << true;
	QTest::newRow("paragraph-reset") << false << false;
	QTest::newRow("character-apply") << true << true;
	QTest::newRow("character-reset") << true << false;
}

void TestStyleSettings::rename()
{
	QFETCH(bool, character);
	QFETCH(bool, apply);
	ScribusDoc* doc = settingsDocument();
	QVERIFY(doc);
	StyleManager* manager = ScCore->primaryMainWindow()->styleMgr();
	manager->reloadStyles();
	if (character)
		manager->showAsEditCharacterStyle("Shared Style");
	else
		manager->showAsEditParagraphStyle("Shared Style");
	QLineEdit* name = manager->findChild<QLineEdit*>("nameEdit");
	QPushButton* resolution = manager->findChild<QPushButton*>(apply ? "applyButton" : "resetButton");
	QVERIFY(name && resolution);
	const QStringList originalFind = doc->tocSetups().first().styleListToFind;
	const QStringList originalOutput = doc->tocSetups().first().styleListForTOC;
	doc->setModified(false);
	name->setText("Draft Style");
	name->setText("Renamed Style");
	QVERIFY(manager->hasPendingChanges());
	QCOMPARE(references(doc), expectedReferences("Shared Style", "Shared Style"));
	QCOMPARE(doc->tocSetups().first().styleListToFind, originalFind);
	QCOMPARE(doc->tocSetups().first().styleListForTOC, originalOutput);
	QVERIFY(!doc->isModified());
	QVERIFY(resolution->isEnabled());
	resolution->click();
	QVERIFY(!manager->hasPendingChanges());
	const QString paragraph = apply && !character ? "Renamed Style" : "Shared Style";
	const QString charName = apply && character ? "Renamed Style" : "Shared Style";
	QCOMPARE(references(doc), expectedReferences(paragraph, charName));
	QCOMPARE(doc->tocSetups().first().styleListToFind, (QStringList {paragraph, "Other Style", paragraph}));
	QCOMPARE(doc->tocSetups().first().styleListForTOC, (QStringList {paragraph, "Other Style"}));
	QVERIFY(doc->paragraphStyles().contains(paragraph));
	QVERIFY(doc->charStyles().contains(charName));
	QCOMPARE(doc->isModified(), apply);
	QVERIFY(!doc->paragraphStyles().contains("Draft Style"));
	QVERIFY(!doc->charStyles().contains("Draft Style"));
	QTemporaryDir output;
	QVERIFY(output.isValid());
	QVERIFY(reopen(doc, output.filePath("renamed-settings.sla.gz")));
	doc = ScCore->primaryMainWindow()->doc;
	QCOMPARE(references(doc), expectedReferences(paragraph, charName));
	QCOMPARE(doc->charStyles().get(charName).fontSize(), 180.0);
	QVERIFY(doc->tocSetups().first().entryData.at(1).removeLineBreaks);
	QCOMPARE(doc->tocSetups().first().entryData.at(1).pageLocation, NotShown);
	QVERIFY(doc->indexSetups().first().caseSensitiveCombination);
	QVERIFY(doc->indexSetups().first().listNonPrintingFrames);
	doc->removeUnusedStyles();
	QVERIFY(doc->paragraphStyles().contains(paragraph));
	QVERIFY(doc->charStyles().contains(charName));
}

void TestStyleSettings::replacement_data()
{
	QTest::addColumn<QString>("replacement");
	QTest::addColumn<bool>("cancel");
	QTest::newRow("replace") << QString("Replacement Style") << false;
	QTest::newRow("cancel") << QString("Replacement Style") << true;
	QTest::newRow("no-style") << QString() << false;
}

void TestStyleSettings::replacement()
{
	QFETCH(QString, replacement);
	QFETCH(bool, cancel);
	ScribusDoc* doc = settingsDocument();
	QVERIFY(doc);
	StyleManager* manager = ScCore->primaryMainWindow()->styleMgr();
	manager->reloadStyles();
	manager->showAsEditParagraphStyle("Shared Style");
	QPushButton* remove = manager->findChild<QPushButton*>("deleteButton");
	QVERIFY(remove && remove->isEnabled());
	doc->setModified(false);
	ScCore->init(true, {});
	bool seen = false;
	bool validOptions = false;
	QTimer response;
	connect(&response, &QTimer::timeout, this, [&]() {
		auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
		if (!dialog || QByteArray(dialog->metaObject()->className()) != "SMReplaceDia")
			return;
		seen = true;
		const auto combos = dialog->findChildren<QComboBox*>();
		validOptions = combos.count() == 1;
		for (QComboBox* combo : combos)
		{
			const int index = replacement.isEmpty() ? 0 : combo->findText(replacement);
			validOptions = validOptions && index >= 0 && combo->findText("Shared Style") < 0;
			combo->setCurrentIndex(index);
		}
		QPushButton* answer = dialog->findChild<QPushButton*>(cancel ? "buttonCancel" : "buttonOk");
		if (answer)
			answer->click();
		else
			dialog->reject();
	});
	response.start(0);
	remove->click();
	response.stop();
	QVERIFY(seen);
	QVERIFY(validOptions);
	const QString paragraph = cancel ? "Shared Style" : replacement;
	QCOMPARE(references(doc), expectedReferences(paragraph, "Shared Style"));
	QCOMPARE(doc->tocSetups().first().styleListToFind, (QStringList {paragraph, "Other Style", paragraph}));
	QCOMPARE(doc->tocSetups().first().styleListForTOC, (QStringList {paragraph, "Other Style"}));
	QCOMPARE(doc->paragraphStyles().contains("Shared Style"), cancel);
	QVERIFY(doc->charStyles().contains("Shared Style"));
	QCOMPARE(doc->isModified(), !cancel);
	QTemporaryDir output;
	QVERIFY(output.isValid());
	QVERIFY(reopen(doc, output.filePath("replaced-settings.sla.gz")));
	doc = ScCore->primaryMainWindow()->doc;
	QCOMPARE(references(doc), expectedReferences(paragraph, "Shared Style"));
	QCOMPARE(doc->paragraphStyles().contains("Shared Style"), cancel);
}

void TestStyleSettings::resetCharacterThenApply_data()
{
	QTest::addColumn<bool>("switchDocument");
	QTest::newRow("same-document") << false;
	QTest::newRow("next-document") << true;
}

void TestStyleSettings::resetCharacterThenApply()
{
	QFETCH(bool, switchDocument);
	ScribusDoc* doc = settingsDocument();
	QVERIFY(doc);
	StyleManager* manager = ScCore->primaryMainWindow()->styleMgr();
	manager->reloadStyles();
	manager->showAsEditCharacterStyle("Shared Style");
	QLineEdit* name = manager->findChild<QLineEdit*>("nameEdit");
	QPushButton* reset = manager->findChild<QPushButton*>("resetButton");
	QPushButton* apply = manager->findChild<QPushButton*>("applyButton");
	QVERIFY(name && reset && apply);
	name->setText("Canceled Mark");
	QVERIFY(reset->isEnabled());
	reset->click();
	QCOMPARE(references(doc), expectedReferences("Shared Style", "Shared Style"));
	QVERIFY(!manager->hasPendingChanges());
	if (switchDocument)
	{
		doc->setModified(false);
		QVERIFY(ScCore->primaryMainWindow()->slotFileClose());
		doc = settingsDocument();
		QVERIFY(doc);
		manager->reloadStyles();
	}
	manager->showAsEditParagraphStyle("Shared Style");
	name->setText("Committed Body");
	QVERIFY(apply->isEnabled());
	apply->click();
	// Apply processes all style families. A canceled character rename must not
	// remain queued and remove its definition or rewrite the note mark here.
	QCOMPARE(references(doc), expectedReferences("Committed Body", "Shared Style"));
	QVERIFY(doc->charStyles().contains("Shared Style"));
	QVERIFY(!doc->charStyles().contains("Canceled Mark"));
	QTemporaryDir output;
	QVERIFY(output.isValid());
	QVERIFY(reopen(doc, output.filePath("reset-then-apply.sla.gz")));
	doc = ScCore->primaryMainWindow()->doc;
	QCOMPARE(references(doc), expectedReferences("Committed Body", "Shared Style"));
	QCOMPARE(doc->charStyles().get("Shared Style").fontSize(), 180.0);
}
