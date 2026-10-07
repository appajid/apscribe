/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QTemporaryDir>
#include <QMessageBox>
#include <QComboBox>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextEdit>
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

QString pythonString(const QString& value)
{
	// JSON string escaping is also valid Python string escaping. Avoid path
	// quoting assumptions in the real embedded interpreter on any platform.
	const QByteArray json = QJsonDocument(QJsonArray {value}).toJson(QJsonDocument::Compact);
	return QString::fromUtf8(json.mid(1, json.size() - 2));
}
}

void TestStyleCleanup::cleanup()
{
	ScCore->init(false, {});
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	if (window->HaveDoc)
	{
		// Also restore the editor if a pending-edit assertion failed early.
		QPushButton* reset = window->styleMgr()->findChild<QPushButton*>("resetButton");
		if (reset && reset->isEnabled())
			reset->click();
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

void TestStyleCleanup::pendingEdits_data()
{
	QTest::addColumn<QString>("edit");
	QTest::addColumn<bool>("apply");
	for (const QString& edit : {QString("rename"), QString("parent")})
	{
		QTest::newRow(qPrintable(edit + "-apply")) << edit << true;
		QTest::newRow(qPrintable(edit + "-reset")) << edit << false;
	}
}

void TestStyleCleanup::pendingEdits()
{
	QFETCH(QString, edit);
	QFETCH(bool, apply);
	ScribusDoc* doc = newDocument();
	QVERIFY(doc);
	addParagraphs(doc, {"Used Body", "Potential Parent", "Unused"}, CommonStrings::DefaultParagraphStyle);
	const int index = doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified,
		50, 50, 300, 100, 0, CommonStrings::None, CommonStrings::None);
	QVERIFY(index >= 0);
	PageItem_TextFrame* frame = doc->Items->at(index)->asTextFrame();
	QVERIFY(frame);
	frame->itemText.insertChars(0, QStringLiteral("Body text survives cleanup"));
	ParagraphStyle assigned;
	assigned.setParent("Used Body");
	frame->itemText.setDefaultStyle(assigned);
	StyleManager* manager = ScCore->primaryMainWindow()->styleMgr();
	manager->reloadStyles();
	manager->showAsEditParagraphStyle("Used Body");
	QPushButton* remove = manager->findChild<QPushButton*>("deleteUnusedButton");
	QPushButton* applyButton = manager->findChild<QPushButton*>("applyButton");
	QPushButton* reset = manager->findChild<QPushButton*>("resetButton");
	QVERIFY(remove && applyButton && reset);
	QLineEdit* name = manager->findChild<QLineEdit*>("nameEdit");
	QVERIFY(name);
	QComboBox* parent = nullptr;
	if (edit == "rename")
		name->setText("Renamed Body");
	else
	{
		for (QComboBox* candidate : manager->findChildren<QComboBox*>("parentCombo"))
		{
			if (candidate->isVisible() && candidate->findText("Potential Parent") >= 0)
				parent = candidate;
		}
		QVERIFY(parent);
		parent->setCurrentText("Potential Parent");
	}
	QVERIFY(applyButton->isEnabled());
	QVERIFY(reset->isEnabled());
	doc->setModified(false);
	ScCore->init(true, {});
	bool seen = false;
	QString summary;
	QTimer response;
	connect(&response, &QTimer::timeout, this, [&]() {
		auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
		if (!dialog)
			return;
		seen = true;
		summary = dialog->text();
		// Answer Yes to the unsafe old implementation's deletion question;
		// acknowledge the new implementation's pending-edit warning with OK.
		const auto answer = dialog->button(QMessageBox::Yes) ? QMessageBox::Yes : QMessageBox::Ok;
		dialog->button(answer)->click();
	});
	response.start(0);
	remove->click();
	response.stop();
	QVERIFY(seen);
	QVERIFY2(summary.contains("Apply or reset"), qPrintable(summary));
	QVERIFY(doc->paragraphStyles().contains("Used Body"));
	QVERIFY(doc->paragraphStyles().contains("Potential Parent"));
	QVERIFY(doc->paragraphStyles().contains("Unused"));
	QVERIFY(!doc->paragraphStyles().contains("Renamed Body"));
	QCOMPARE(doc->paragraphStyles().get("Used Body").parent(), CommonStrings::DefaultParagraphStyle);
	QCOMPARE(frame->itemText.defaultStyle().parent(), QString("Used Body"));
	QVERIFY(!doc->isModified());
	QVERIFY(applyButton->isEnabled());
	QVERIFY(reset->isEnabled());
	if (edit == "rename")
		QCOMPARE(name->text(), QString("Renamed Body"));
	else
		QCOMPARE(parent->currentText(), QString("Potential Parent"));

	// Only the user's explicit Apply/Reset resolves the draft. Cleanup must
	// then use the resulting document, keeping newly used dependencies.
	(apply ? applyButton : reset)->click();
	QVERIFY(!applyButton->isEnabled());
	QVERIFY(!reset->isEnabled());
	seen = false;
	response.start(0);
	remove->click();
	response.stop();
	QVERIFY(seen);
	QVERIFY(!doc->paragraphStyles().contains("Unused"));
	const QString body = apply && edit == "rename" ? "Renamed Body" : "Used Body";
	QVERIFY(doc->paragraphStyles().contains(body));
	QCOMPARE(frame->itemText.defaultStyle().parent(), body);
	QCOMPARE(doc->paragraphStyles().contains("Potential Parent"), apply && edit == "parent");
	QCOMPARE(doc->paragraphStyles().get(body).parent(),
		apply && edit == "parent" ? QString("Potential Parent") : CommonStrings::DefaultParagraphStyle);
	QCOMPARE(frame->itemText.text(0, frame->itemText.length()), QString("Body text survives cleanup"));
	ScCore->init(false, {});
	QTemporaryDir output;
	QVERIFY(output.isValid());
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	const QString path = output.filePath("resolved-styles.sla.gz");
	QVERIFY(window->DoFileSave(path));
	doc->setModified(false);
	QVERIFY(window->slotFileClose());
	QVERIFY(window->loadDoc(path));
	doc = window->doc;
	QVERIFY(doc->paragraphStyles().contains(body));
	QCOMPARE(doc->paragraphStyles().contains("Potential Parent"), apply && edit == "parent");
	frame = doc->Items->first()->asTextFrame();
	QVERIFY(frame);
	QCOMPARE(frame->itemText.defaultStyle().parent(), body);
	QCOMPARE(frame->itemText.text(0, frame->itemText.length()), QString("Body text survives cleanup"));
	QCOMPARE(doc->removeUnusedStyles(), 0);
}

void TestStyleCleanup::pendingScriptEdits_data()
{
	QTest::addColumn<bool>("apply");
	QTest::newRow("character-rename-apply") << true;
	QTest::newRow("character-rename-reset") << false;
}

void TestStyleCleanup::pendingScriptEdits()
{
	QFETCH(bool, apply);
	ScribusMainWindow* window = ScCore->primaryMainWindow();
	ScribusDoc* doc = newDocument();
	QVERIFY(doc);
	StyleSet<CharStyle> characters;
	CharStyle emphasis;
	emphasis.setName("Used Emphasis");
	emphasis.setParent(CommonStrings::DefaultCharacterStyle);
	emphasis.setFontSize(190);
	characters.create(emphasis);
	doc->redefineCharStyles(characters, false);
	addParagraphs(doc, {"Unused"}, CommonStrings::DefaultParagraphStyle);
	const int index = doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified,
		50, 50, 300, 100, 0, CommonStrings::None, CommonStrings::None);
	QVERIFY(index >= 0);
	PageItem_TextFrame* frame = doc->Items->at(index)->asTextFrame();
	QVERIFY(frame);
	frame->itemText.insertChars(0, QStringLiteral("Emphasized text"));
	CharStyle assigned;
	assigned.setParent(emphasis.name());
	frame->itemText.applyCharStyle(0, frame->itemText.length(), assigned);
	StyleManager* manager = window->styleMgr();
	manager->reloadStyles();
	manager->showAsEditCharacterStyle(emphasis.name());
	QLineEdit* name = manager->findChild<QLineEdit*>("nameEdit");
	QPushButton* applyButton = manager->findChild<QPushButton*>("applyButton");
	QPushButton* reset = manager->findChild<QPushButton*>("resetButton");
	QVERIFY(name && applyButton && reset);
	name->setText("Renamed Emphasis");
	QVERIFY(manager->hasPendingChanges());
	doc->setModified(false);

	QObject* console = nullptr;
	for (QObject* child : window->findChildren<QObject*>())
	{
		if (QByteArray(child->metaObject()->className()) == "PythonConsole")
			console = child;
	}
	QVERIFY2(console, "The actual Python Scripter plugin must be loaded for this integration test.");
	QTextEdit* commandEdit = console->findChild<QTextEdit*>("commandEdit");
	QVERIFY(commandEdit);
	QTemporaryDir output;
	QVERIFY(output.isValid());
	const QString resultPath = output.filePath("cleanup-result.txt");
	const QString script = "import scribus\n"
		"try:\n"
		"    removed = scribus.removeUnusedStyles()\n"
		"    result = 'removed:' + str(removed)\n"
		"except RuntimeError as error:\n"
		"    result = str(error)\n"
		"with open(" + pythonString(resultPath) + ", 'w', encoding='utf-8') as output:\n"
		"    output.write(result)\n";
	const QString command = "exec(" + pythonString(script) + ")";
	commandEdit->setPlainText(command);
	QVERIFY(QMetaObject::invokeMethod(console, "slot_runScript", Qt::DirectConnection));
	QFile result(resultPath);
	QVERIFY(result.open(QIODevice::ReadOnly));
	QVERIFY(QString::fromUtf8(result.readAll()).contains("Apply or reset"));
	result.close();
	QVERIFY(doc->charStyles().contains("Used Emphasis"));
	QVERIFY(!doc->charStyles().contains("Renamed Emphasis"));
	QVERIFY(doc->paragraphStyles().contains("Unused"));
	QCOMPARE(frame->itemText.charStyle(0).parent(), QString("Used Emphasis"));
	QVERIFY(!doc->isModified());
	QCOMPARE(name->text(), QString("Renamed Emphasis"));
	QVERIFY(manager->hasPendingChanges());
	QVERIFY(applyButton->isEnabled());
	QVERIFY(reset->isEnabled());
	(apply ? applyButton : reset)->click();
	QVERIFY(!manager->hasPendingChanges());
	commandEdit->setPlainText(command);
	QVERIFY(QMetaObject::invokeMethod(console, "slot_runScript", Qt::DirectConnection));
	QVERIFY(result.open(QIODevice::ReadOnly));
	QCOMPARE(result.readAll(), QByteArray("removed:1"));
	result.close();
	QVERIFY(!doc->paragraphStyles().contains("Unused"));
	const QString kept = apply ? "Renamed Emphasis" : "Used Emphasis";
	QVERIFY(doc->charStyles().contains(kept));
	QCOMPARE(frame->itemText.charStyle(0).parent(), kept);
	QCOMPARE(frame->itemText.charStyle(0).fontSize(), 190.0);
	QCOMPARE(frame->itemText.text(0, frame->itemText.length()), QString("Emphasized text"));
	const QString path = output.filePath("resolved-character-styles.sla.gz");
	QVERIFY(window->DoFileSave(path));
	doc->setModified(false);
	QVERIFY(window->slotFileClose());
	QVERIFY(window->loadDoc(path));
	doc = window->doc;
	QVERIFY(doc->charStyles().contains(kept));
	frame = doc->Items->first()->asTextFrame();
	QVERIFY(frame);
	QCOMPARE(frame->itemText.charStyle(0).parent(), kept);
	QCOMPARE(frame->itemText.charStyle(0).fontSize(), 190.0);
	QCOMPARE(frame->itemText.text(0, frame->itemText.length()), QString("Emphasized text"));
	QCOMPARE(doc->removeUnusedStyles(), 0);
}

void TestStyleCleanup::invalidName_data()
{
	QTest::addColumn<QString>("invalid");
	QTest::newRow("blank") << QString();
	QTest::newRow("duplicate") << QString("Another Style");
}

void TestStyleCleanup::invalidName()
{
	QFETCH(QString, invalid);
	ScribusDoc* doc = newDocument();
	QVERIFY(doc);
	addParagraphs(doc, {"Body", "Another Style"}, CommonStrings::DefaultParagraphStyle);
	StyleManager* manager = ScCore->primaryMainWindow()->styleMgr();
	manager->reloadStyles();
	manager->showAsEditParagraphStyle("Body");
	QLineEdit* name = manager->findChild<QLineEdit*>("nameEdit");
	QPushButton* apply = manager->findChild<QPushButton*>("applyButton");
	QPushButton* reset = manager->findChild<QPushButton*>("resetButton");
	QPushButton* done = manager->findChild<QPushButton*>("okButton");
	QVERIFY(name && apply && reset && done);
	name->setText(invalid);
	QVERIFY(!apply->isEnabled());
	QVERIFY(!done->isEnabled());
	QVERIFY(manager->hasPendingChanges());
	QVERIFY2(reset->isEnabled(), "Invalid drafts must still offer the Reset required by the cleanup warning.");
	reset->click();
	QCOMPARE(name->text(), QString("Body"));
	QVERIFY(done->isEnabled());
	QVERIFY(!manager->hasPendingChanges());
	QVERIFY(doc->paragraphStyles().contains("Body"));
	QVERIFY(doc->paragraphStyles().contains("Another Style"));
	// Correcting a blank/duplicate draft without Reset must also recover Done.
	name->setText(invalid);
	name->setText("Corrected Body");
	QVERIFY(done->isEnabled());
	QVERIFY(apply->isEnabled());
	reset->click();
	QVERIFY(doc->paragraphStyles().contains("Body"));
	QVERIFY(!doc->paragraphStyles().contains("Corrected Body"));
	QVERIFY(!manager->hasPendingChanges());
}
