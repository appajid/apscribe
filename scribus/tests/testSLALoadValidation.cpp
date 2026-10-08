/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QDomDocument>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "testSLALoadValidation.h"
#include "appmodes.h"
#include "commonstrings.h"
#include "fileloader.h"
#include "loadsaveplugin.h"
#include "pageitem_textframe.h"
#include "qtiocompressor.h"
#include "scribus.h"
#include "scribuscore.h"
#include "scribusdoc.h"

namespace
{
ScribusDoc* fixtureDocument()
{
	auto* doc = ScCore->primaryMainWindow()->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, 0, 0, 0, 0, 1, QSizeF(), true, 2);
	if (!doc)
		return nullptr;
	ParagraphStyle heading;
	heading.setName("Import heading");
	heading.setParent(CommonStrings::DefaultParagraphStyle);
	heading.charStyle().setFontSize(180);
	StyleSet<ParagraphStyle> styles;
	styles.create(heading);
	doc->redefineStyles(styles, false);
	const int index = doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified,
		50, 50, 300, 120, 0, CommonStrings::None, CommonStrings::None);
	if (index < 0)
		return nullptr;
	auto* frame = doc->Items->at(index)->asTextFrame();
	frame->setItemName("Import text");
	frame->itemText.insertChars(0, QStringLiteral("Preserve this text") + SpecialChars::PARSEP);
	ParagraphStyle paragraph;
	paragraph.setParent(heading.name());
	frame->itemText.applyStyle(0, paragraph);
	return doc;
}

bool writeFixture(const QString& path, const QByteArray& bytes, bool compressed = false)
{
	QFile file(path);
	if (compressed)
	{
		QtIOCompressor compressor(&file);
		compressor.setStreamFormat(QtIOCompressor::GzipFormat);
		if (!compressor.open(QIODevice::WriteOnly))
			return false;
		const bool success = compressor.write(bytes) == bytes.size();
		compressor.close();
		return success;
	}
	return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return {};
	return file.readAll();
}

PageItem_TextFrame* textFrame(ScribusDoc* doc)
{
	for (auto* item : doc->DocItems)
		if (item->itemName() == "Import text")
			return item->asTextFrame();
	return nullptr;
}

void closeDocuments()
{
	auto* window = ScCore->primaryMainWindow();
	while (window->HaveDoc)
	{
		window->doc->appMode = modeNormal;
		window->doc->setModified(false);
		if (!window->slotFileClose())
			break;
	}
}
}

void TestSLALoadValidation::cleanup()
{
	ScCore->init(false, {});
	closeDocuments();
}

void TestSLALoadValidation::incompleteDocument_data()
{
	QTest::addColumn<int>("format");
	QTest::addColumn<QByteArray>("xml");
	for (bool old : {true, false})
	{
		const int format = old ? FORMATID_SLA170IMPORT : FORMATID_SLA171IMPORT;
		const QByteArray prefix = old ? "170-" : "171-";
		const QByteArray root = old ? "<SCRIBUSUTF8NEW Version=\"1.7.0\">" : "<SCRIBUSUTF8NEW Version=\"1.7.4\">";
		const QByteArray document = old ? "DOCUMENT" : "Document";
		QTest::newRow((prefix + "empty-root").constData()) << format << root + "</SCRIBUSUTF8NEW>";
		QTest::newRow((prefix + "unknown-document").constData()) << format << root + "<Unknown/></SCRIBUSUTF8NEW>";
		QTest::newRow((prefix + "no-pages").constData()) << format << root + "<" + document + "/></SCRIBUSUTF8NEW>";
		QTest::newRow((prefix + "wrong-root").constData()) << format << "<WrongRoot>" + root + "</SCRIBUSUTF8NEW></WrongRoot>";
		QTest::newRow((prefix + "broken-xml").constData()) << format << root + "<" + document + "></SCRIBUSUTF8NEW>";
	}
	QTest::newRow("170-newer-document-spelling") << int(FORMATID_SLA170IMPORT)
		<< QByteArray("<SCRIBUSUTF8NEW Version=\"1.7.0\"><Document/></SCRIBUSUTF8NEW>");
}

void TestSLALoadValidation::incompleteDocument()
{
	QFETCH(int, format);
	QFETCH(QByteArray, xml);
	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	const QString path = directory.filePath("incomplete.sla");
	QVERIFY(writeFixture(path, xml));
	FileLoader loader(path);
	QCOMPARE(loader.testFile(), format);
	// Match the loader's fresh, empty destination independently of the UI's
	// successful-open path. The unguarded loader crashes during finalization.
	ScribusDoc probe;
	probe.setGUI(false, ScCore->primaryMainWindow(), nullptr);
	probe.setLoading(true);
	QVERIFY(probe.DocPages.isEmpty());
	const bool loaded = loader.loadFile(&probe);
	QVERIFY2(!loaded, qPrintable(QString("Loader reported success with %1 normal pages").arg(probe.DocPages.size())));
	const auto* fileFormat = LoadSavePlugin::getFormatById(format);
	QVERIFY(fileFormat && fileFormat->plug);
	QVERIFY(!fileFormat->plug->lastError().isEmpty());
	QCOMPARE(readBytes(path), xml);
}

void TestSLALoadValidation::validDocuments_data()
{
	QTest::addColumn<bool>("old");
	QTest::addColumn<bool>("compressed");
	QTest::newRow("170") << true << false;
	QTest::newRow("170-gzip") << true << true;
	QTest::newRow("171") << false << false;
	QTest::newRow("171-gzip") << false << true;
}

void TestSLALoadValidation::validDocuments()
{
	QFETCH(bool, old);
	QFETCH(bool, compressed);
	auto* doc = fixtureDocument();
	QVERIFY(doc);
	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	const QString path = directory.filePath(compressed ? "valid.sla.gz" : "valid.sla");
	auto* window = ScCore->primaryMainWindow();
	QVERIFY(window->DoFileSave(path, nullptr, old ? FORMATID_SLA170EXPORT : FORMATID_CURRENTEXPORT));
	const QByteArray original = readBytes(path);
	QVERIFY(!original.isEmpty());
	closeDocuments();
	QVERIFY(window->loadDoc(path));
	QCOMPARE(window->doc->DocPages.size(), 2);
	QVERIFY(window->doc->currentPage());
	auto* frame = textFrame(window->doc);
	QVERIFY(frame);
	QCOMPARE(frame->itemText.text(0, frame->itemText.length()), QString("Preserve this text") + SpecialChars::PARSEP);
	QCOMPARE(frame->itemText.paragraphStyle(0).parent(), QString("Import heading"));
	frame->layout();
	QCOMPARE(frame->itemText.charStyle(0).fontSize(), 180.0);
	QCOMPARE(readBytes(path), original);
}

void TestSLALoadValidation::rejectsAndRecovers_data()
{
	QTest::addColumn<bool>("existingDocument");
	QTest::addColumn<bool>("compressed");
	QTest::newRow("empty-session") << false << false;
	QTest::newRow("empty-session-gzip") << false << true;
	QTest::newRow("existing-document") << true << false;
	QTest::newRow("existing-document-gzip") << true << true;
}

void TestSLALoadValidation::rejectsAndRecovers()
{
	QFETCH(bool, existingDocument);
	QFETCH(bool, compressed);
	auto* doc = fixtureDocument();
	QVERIFY(doc);
	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	const QString validPath = directory.filePath("valid.sla");
	const QString invalidPath = directory.filePath(compressed ? "mislabelled.sla.gz" : "mislabelled.sla");
	auto* window = ScCore->primaryMainWindow();
	QVERIFY(window->DoFileSave(validPath));
	QDomDocument xml;
	QVERIFY(xml.setContent(readBytes(validPath)));
	// Reproduce the exact failure discovered during the TOC tests: newer
	// document structure with only its version marker changed to 1.7.0.
	xml.documentElement().setAttribute("Version", "1.7.0");
	QVERIFY(writeFixture(invalidPath, xml.toByteArray(), compressed));
	const QByteArray original = readBytes(invalidPath);
	if (existingDocument)
		doc->setModified(true);
	else
		closeDocuments();
	QVERIFY(!window->loadDoc(invalidPath));
	QVERIFY(!window->scriptIsRunning());
	QCOMPARE(int(window->HaveDoc), existingDocument ? 1 : 0);
	QCOMPARE(readBytes(invalidPath), original);
	if (existingDocument)
	{
		QCOMPARE(window->doc, doc);
		QVERIFY(doc->isModified());
		auto* frame = textFrame(doc);
		QVERIFY(frame);
		QCOMPARE(frame->itemText.text(0, frame->itemText.length()), QString("Preserve this text") + SpecialChars::PARSEP);
		QCOMPARE(frame->itemText.paragraphStyle(0).parent(), QString("Import heading"));
	}
	// Failure must not poison subsequent loading or leave a partial window.
	QVERIFY(window->loadDoc(validPath));
	QCOMPARE(window->doc->DocPages.size(), 2);
	QVERIFY(textFrame(window->doc));
}
