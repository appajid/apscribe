/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QCheckBox>
#include <QDomDocument>
#include <QFile>
#include <QPushButton>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include "ui_prefs_tableofcontentsbase.h"

#include "testTOCAlignment.h"
#include "appmodes.h"
#include "commonstrings.h"
#include "plugins/formatidlist.h"
#include "pageitem_textframe.h"
#include "prefsmanager.h"
#include "scribus.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "tocgenerator.h"
#include "ui/preferences/prefs_tableofcontents.h"

namespace
{
ScribusDoc* newDocument()
{
	return ScCore->primaryMainWindow()->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, 0, 0, 0, 0, 1, QSizeF(), true);
}

PageItem_TextFrame* addFrame(ScribusDoc* doc, const QString& name, double y)
{
	int index = doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified,
		50, y, 320, 120, 0, CommonStrings::None, CommonStrings::None);
	if (index < 0)
		return nullptr;
	auto* frame = doc->Items->at(index)->asTextFrame();
	frame->setItemName(name);
	frame->OwnPage = 0;
	return frame;
}

void addStyles(ScribusDoc* doc, bool customTabs)
{
	StyleSet<ParagraphStyle> styles;
	ParagraphStyle source;
	source.setName("Heading");
	source.setParent(CommonStrings::DefaultParagraphStyle);
	styles.create(source);
	ParagraphStyle base;
	base.setName("TOC Base");
	base.setParent(CommonStrings::DefaultParagraphStyle);
	base.setRightMargin(9);
	base.setAlignment(ParagraphStyle::LeftAligned);
	base.charStyle().setFontSize(110);
	if (customTabs)
		base.setTabValues({ParagraphStyle::TabRecord {120, ParagraphStyle::RightTab, QChar('.')}});
	styles.create(base);
	ParagraphStyle output;
	output.setName("TOC Entry");
	output.setParent(base.name());
	styles.create(output);
	doc->redefineStyles(styles, false);
}

ToCSetup setup(bool byStyle, bool automatic, TOCPageLocation placement)
{
	ToCSetup toc {};
	toc.name = "Contents";
	toc.tocSource = byStyle ? "Style" : "Attribute";
	toc.frameName = "Contents frame";
	toc.itemAttrName = "TOC";
	toc.textStyle = "TOC Entry";
	toc.pageLocation = placement;
	toc.autoRightAlignPageNumbers = automatic;
	toc.entryData.append({"Heading", "TOC Entry", false, placement});
	return toc;
}

void generate(ScribusDoc* doc, bool byStyle)
{
	TOCGenerator generator(nullptr, doc);
	if (byStyle)
		generator.generateByStyle();
	else
		generator.generateByAttribute();
}

void addHeading(PageItem_TextFrame* frame, const QString& text, const QString& style = "Heading")
{
	frame->itemText.insertChars(0, text + SpecialChars::PARSEP);
	ParagraphStyle paragraph;
	paragraph.setParent(style);
	frame->itemText.applyStyle(0, paragraph);
	frame->invalidateLayout();
	frame->layout();
	ObjectAttribute attribute;
	attribute.name = "TOC";
	attribute.value = text;
	ObjAttrVector attributes {attribute};
	frame->setObjectAttributes(&attributes);
}

bool removeAlignmentSetting(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	QDomDocument xml;
	if (!xml.setContent(file.readAll()))
		return false;
	file.close();
	auto entries = xml.elementsByTagName("TableOfContents");
	for (int i = 0; i < entries.count(); ++i)
		entries.at(i).toElement().removeAttribute("AutoRightAlignPageNumbers");
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	const QByteArray bytes = xml.toByteArray();
	return file.write(bytes) == bytes.size();
}
}

void TestTOCAlignment::cleanup()
{
	ScCore->init(false, {});
	auto* window = ScCore->primaryMainWindow();
	if (window->HaveDoc)
	{
		window->doc->appMode = modeNormal;
		window->doc->setModified(false);
		window->slotFileClose();
	}
}

void TestTOCAlignment::generation_data()
{
	QTest::addColumn<bool>("byStyle");
	QTest::addColumn<bool>("automatic");
	QTest::addColumn<int>("placement");
	QTest::addColumn<bool>("customTabs");
	QTest::addColumn<int>("columns");
	QTest::addColumn<bool>("noOutputStyle");
	for (bool style : {false, true})
	{
		const QByteArray prefix = style ? "style-" : "attribute-";
		QTest::newRow((prefix + "automatic").constData()) << style << true << int(End) << false << 1 << false;
		QTest::newRow((prefix + "columns").constData()) << style << true << int(End) << false << 2 << false;
		QTest::newRow((prefix + "legacy").constData()) << style << false << int(End) << false << 1 << false;
		QTest::newRow((prefix + "custom-inherited-tabs").constData()) << style << true << int(End) << true << 1 << false;
		QTest::newRow((prefix + "beginning").constData()) << style << true << int(Beginning) << false << 1 << false;
		QTest::newRow((prefix + "hidden").constData()) << style << true << int(NotShown) << false << 1 << false;
		QTest::newRow((prefix + "no-output-style").constData()) << style << true << int(End) << false << 1 << true;
	}
}

void TestTOCAlignment::generation()
{
	QFETCH(bool, byStyle);
	QFETCH(bool, automatic);
	QFETCH(int, placement);
	QFETCH(bool, customTabs);
	QFETCH(int, columns);
	QFETCH(bool, noOutputStyle);
	auto* doc = newDocument();
	QVERIFY(doc);
	addStyles(doc, customTabs);
	auto* source = addFrame(doc, "Source", 50);
	auto* target = addFrame(doc, "Contents frame", 240);
	QVERIFY(source && target);
	addHeading(source, "Chapter");
	target->setTextToFrameDist(12, 18, 6, 6);
	target->setColumns(columns);
	target->setColumnGap(14);
	target->itemText.insertChars(0, QStringLiteral("Old contents must be replaced"));
	ToCSetup toc = setup(byStyle, automatic, TOCPageLocation(placement));
	if (noOutputStyle)
	{
		toc.textStyle = CommonStrings::None;
		toc.entryData[0].styleForText.clear();
	}
	doc->appendToTocSetups(toc);
	const ParagraphStyle original = doc->paragraphStyles().get("TOC Entry");
	for (double width : {320.0, 370.0})
	{
		doc->sizeItem(width, target->height(), target);
		generate(doc, byStyle);
		target->invalidateLayout();
		target->layout();
		const QString expected = placement == End ? "Chapter\t1" : placement == Beginning ? "1\tChapter" : "Chapter\t";
		QCOMPARE(target->itemText.text(0, target->itemText.length()), expected + SpecialChars::PARSEP);
		const ParagraphStyle& paragraph = target->itemText.paragraphStyle(0);
		if (!noOutputStyle)
		{
			QCOMPARE(paragraph.parent(), QString("TOC Entry"));
			QCOMPARE(paragraph.charStyle().fontSize(), 110.0);
			QCOMPARE(paragraph.rightMargin(), 9.0);
		}
		const auto tabs = paragraph.tabValues();
		if (customTabs)
			QCOMPARE(tabs, doc->paragraphStyles().get("TOC Base").tabValues());
		else if (!automatic || placement != End)
			QVERIFY(tabs.isEmpty());
		else
		{
			QCOMPARE(tabs.size(), 1);
			QCOMPARE(tabs.first().tabType, int(ParagraphStyle::RightTab));
			QCOMPARE(tabs.first().tabPosition, target->columnWidth() - paragraph.rightMargin());
			QVERIFY(tabs.first().tabFillChar.isNull());
			QVERIFY(!target->frameOverflows());
			// Test the real layouter, not only the stored tab coordinate.
			const QLineF afterNumber = target->textLayout.positionToPoint(target->itemText.length() - 1);
			QVERIFY(!afterNumber.isNull());
			const double edge = 12 + target->columnWidth() - paragraph.rightMargin();
			// The existing legacy text renderer subtracts one point from tabs.
			QVERIFY2(qAbs(afterNumber.x1() - edge) <= 1.01,
				qPrintable(QString("Page number edge %1, expected %2").arg(afterNumber.x1()).arg(edge)));
		}
		QVERIFY(doc->paragraphStyles().get("TOC Entry").equiv(original));
		QCOMPARE(source->itemText.text(0, source->itemText.length()), QString("Chapter") + SpecialChars::PARSEP);
	}
}

void TestTOCAlignment::mixedEntries()
{
	auto* doc = newDocument();
	QVERIFY(doc);
	addStyles(doc, false);
	StyleSet<ParagraphStyle> extra;
	ParagraphStyle heading;
	heading.setName("Second heading");
	heading.setParent("Heading");
	extra.create(heading);
	ParagraphStyle output;
	output.setName("Second TOC entry");
	output.setParent("TOC Entry");
	output.setRightMargin(22);
	extra.create(output);
	doc->redefineStyles(extra, false);
	auto* first = addFrame(doc, "First", 40);
	auto* second = addFrame(doc, "Second", 180);
	auto* target = addFrame(doc, "Contents frame", 340);
	QVERIFY(first && second && target);
	addHeading(first, "First");
	addHeading(second, "Second", "Second heading");
	ToCSetup toc = setup(true, true, End);
	toc.entryData.append({"Second heading", "Second TOC entry", false, End});
	doc->appendToTocSetups(toc);
	generate(doc, true);
	QVERIFY(!target->itemText.paragraphStyle(0).tabValues().isEmpty());
	QCOMPARE(target->itemText.paragraphStyle(0).tabValues().first().tabPosition, target->columnWidth() - 9);
	const int secondStart = target->itemText.startOfNextParagraph(0);
	QVERIFY(!target->itemText.paragraphStyle(secondStart).tabValues().isEmpty());
	QCOMPARE(target->itemText.paragraphStyle(secondStart).tabValues().first().tabPosition, target->columnWidth() - 22);
	// No generated tab may leak from an End entry into a Beginning entry.
	doc->tocSetups()[0].entryData[1].pageLocation = Beginning;
	generate(doc, true);
	QVERIFY(target->itemText.paragraphStyle(target->itemText.startOfNextParagraph(0)).tabValues().isEmpty());
}

void TestTOCAlignment::preferences()
{
	auto* doc = newDocument();
	QVERIFY(doc);
	ApplicationPrefs prefs = PrefsManager::instance().appPrefs;
	doc->setModified(false);
	prefs.tocPrefs.defaultToCSetups = {setup(true, false, End)};
	{
		Prefs_TableOfContents pane(nullptr, doc);
		pane.restoreDefaults(&prefs);
		auto* check = pane.findChild<QCheckBox*>("autoRightAlignCheckBox");
		QVERIFY(check && check->isEnabled());
		QVERIFY(!check->isChecked());
		check->setChecked(true);
		QVERIFY(!prefs.tocPrefs.defaultToCSetups.first().autoRightAlignPageNumbers);
		// Destroying a canceled pane must not mutate the settings or document.
	}
	QVERIFY(doc->tocSetups().isEmpty());
	QVERIFY(!doc->isModified());
	QVERIFY(!prefs.tocPrefs.defaultToCSetups.first().autoRightAlignPageNumbers);
	Prefs_TableOfContents pane(nullptr, doc);
	pane.restoreDefaults(&prefs);
	pane.findChild<QPushButton*>("tocAddButton")->click();
	auto* check = pane.findChild<QCheckBox*>("autoRightAlignCheckBox");
	QVERIFY(check->isChecked());
	pane.saveGuiToPrefs(&prefs);
	QCOMPARE(prefs.tocPrefs.defaultToCSetups.size(), 2);
	QVERIFY(!prefs.tocPrefs.defaultToCSetups.first().autoRightAlignPageNumbers);
	QVERIFY(prefs.tocPrefs.defaultToCSetups.last().autoRightAlignPageNumbers);
	check->setChecked(false);
	pane.saveGuiToPrefs(&prefs);
	QVERIFY(!prefs.tocPrefs.defaultToCSetups.last().autoRightAlignPageNumbers);

	// Exercise the application's preference serializer in an isolated file.
	auto& manager = PrefsManager::instance();
	const ApplicationPrefs saved = manager.appPrefs;
	QScopeGuard restore([&]() { manager.appPrefs = saved; });
	ToCSetup styleDefault = setup(true, true, End);
	styleDefault.entryData.append({"Other heading", "Other entry", true, NotShown});
	manager.appPrefs.tocPrefs.defaultToCSetups = {styleDefault, setup(false, false, Beginning)};
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath("preferences.xml");
	QVERIFY(manager.writePref(path));
	manager.appPrefs.tocPrefs.defaultToCSetups.clear();
	QVERIFY(manager.readPref(path));
	QCOMPARE(manager.appPrefs.tocPrefs.defaultToCSetups.size(), 2);
	QVERIFY(manager.appPrefs.tocPrefs.defaultToCSetups.first().autoRightAlignPageNumbers);
	const auto& loaded = manager.appPrefs.tocPrefs.defaultToCSetups.first();
	QCOMPARE(loaded.tocSource, QString("Style"));
	QCOMPARE(loaded.pageLocation, End);
	QCOMPARE(loaded.entryData.size(), 2);
	QCOMPARE(loaded.entryData.first().styleToFind, QString("Heading"));
	QCOMPARE(loaded.entryData.first().styleForText, QString("TOC Entry"));
	QCOMPARE(loaded.entryData.last().pageLocation, NotShown);
	QVERIFY(loaded.entryData.last().removeLineBreaks);
	QCOMPARE(manager.appPrefs.tocPrefs.defaultToCSetups.last().pageLocation, Beginning);
	QVERIFY(!doc->isModified());
	QVERIFY(removeAlignmentSetting(path));
	QVERIFY(manager.readPref(path));
	QVERIFY(!manager.appPrefs.tocPrefs.defaultToCSetups.first().autoRightAlignPageNumbers);
	// Integer placements written by older application preferences are valid too.
	QFile file(path);
	QVERIFY(file.open(QIODevice::ReadOnly));
	QDomDocument xml;
	QVERIFY(xml.setContent(file.readAll()));
	file.close();
	auto entries = xml.elementsByTagName("TableOfContents");
	entries.at(0).toElement().setAttribute("NumberPlacement", 1);
	entries.at(1).toElement().setAttribute("NumberPlacement", 0);
	QDomElement hidden = entries.at(1).cloneNode().toElement();
	hidden.setAttribute("NumberPlacement", 2);
	entries.at(1).parentNode().appendChild(hidden);
	for (int i = 0; i < entries.count(); ++i)
		entries.at(i).toElement().removeAttribute("ToCSource");
	QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
	const auto bytes = xml.toByteArray();
	QCOMPARE(file.write(bytes), bytes.size());
	file.close();
	QVERIFY(manager.readPref(path));
	QCOMPARE(manager.appPrefs.tocPrefs.defaultToCSetups.size(), 3);
	QCOMPARE(manager.appPrefs.tocPrefs.defaultToCSetups.at(0).pageLocation, End);
	QCOMPARE(manager.appPrefs.tocPrefs.defaultToCSetups.at(1).pageLocation, Beginning);
	QCOMPARE(manager.appPrefs.tocPrefs.defaultToCSetups.at(2).pageLocation, NotShown);
	QCOMPARE(manager.appPrefs.tocPrefs.defaultToCSetups.first().tocSource, QString("Attribute"));
}

void TestTOCAlignment::wrappedAndColumnFlow_data()
{
	QTest::addColumn<bool>("byStyle");
	QTest::newRow("style") << true;
	QTest::newRow("attribute") << false;
}

void TestTOCAlignment::wrappedAndColumnFlow()
{
	QFETCH(bool, byStyle);
	auto* doc = newDocument();
	QVERIFY(doc);
	addStyles(doc, false);
	for (int i = 0; i < 10; ++i)
	{
		auto* source = addFrame(doc, QString("Source %1").arg(i), 40 + i * 24);
		QVERIFY(source);
		addHeading(source, i == 0 ? QString("Long chapter heading ").repeated(6) : QString("Chapter %1").arg(i));
	}
	auto* target = addFrame(doc, "Contents frame", 420);
	QVERIFY(target);
	doc->sizeItem(target->width(), 200, target);
	target->setColumns(2);
	target->setColumnGap(14);
	target->setTextToFrameDist(12, 18, 6, 6);
	doc->appendToTocSetups(setup(byStyle, true, End));
	generate(doc, byStyle);
	target->invalidateLayout();
	target->layout();
	QVERIFY(!target->frameOverflows());
	QVERIFY(target->textLayout.lines() > 10);
	bool secondColumn = false;
	int paragraphs = 0;
	for (int pos = 0; pos < target->itemText.length(); ++pos)
	{
		if (target->itemText.text(pos) != SpecialChars::PARSEP)
			continue;
		++paragraphs;
		const double x = target->textLayout.positionToPoint(pos).x1();
		const double firstEdge = 12 + target->columnWidth() - 9;
		const double secondEdge = firstEdge + target->columnWidth() + 14;
		QVERIFY2(qAbs(x - firstEdge) <= 1.01 || qAbs(x - secondEdge) <= 1.01,
			qPrintable(QString("Page number at %1 instead of column edges %2 / %3").arg(x).arg(firstEdge).arg(secondEdge)));
		secondColumn |= qAbs(x - secondEdge) <= 1.01;
	}
	QCOMPARE(paragraphs, 10);
	QVERIFY(secondColumn);
}

void TestTOCAlignment::persistence_data()
{
	QTest::addColumn<bool>("legacy");
	QTest::addColumn<bool>("oldFormat");
	QTest::newRow("171-automatic") << false << false;
	QTest::newRow("171-legacy") << true << false;
	QTest::newRow("170-automatic") << false << true;
	QTest::newRow("170-legacy") << true << true;
}

void TestTOCAlignment::persistence()
{
	QFETCH(bool, legacy);
	QFETCH(bool, oldFormat);
	auto* doc = newDocument();
	QVERIFY(doc);
	addStyles(doc, false);
	auto* source = addFrame(doc, "Source", 40);
	auto* target = addFrame(doc, "Contents frame", 240);
	QVERIFY(source && target);
	addHeading(source, "Chapter");
	doc->appendToTocSetups(setup(true, true, End));
	doc->appendToTocSetups(setup(false, false, Beginning));
	generate(doc, true);
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath("toc.sla");
	auto* window = ScCore->primaryMainWindow();
	QVERIFY(window->DoFileSave(path, nullptr, oldFormat ? FORMATID_SLA170EXPORT : FORMATID_CURRENTEXPORT));
	doc->setModified(false);
	QVERIFY(window->slotFileClose());
	if (legacy)
		QVERIFY(removeAlignmentSetting(path));
	QVERIFY(window->loadDoc(path));
	doc = window->doc;
	QCOMPARE(doc->tocSetups().size(), 2);
	QCOMPARE(doc->tocSetups().first().autoRightAlignPageNumbers, !legacy);
	QVERIFY(!doc->tocSetups().last().autoRightAlignPageNumbers);
	PageItem_TextFrame* reopened = nullptr;
	for (auto* item : doc->DocItems)
		if (item->itemName() == "Contents frame")
			reopened = item->asTextFrame();
	QVERIFY(reopened);
	QCOMPARE(reopened->itemText.paragraphStyle(0).tabValues().size(), 1);
	generate(doc, true);
	QCOMPARE(reopened->itemText.paragraphStyle(0).tabValues().isEmpty(), legacy);
}
