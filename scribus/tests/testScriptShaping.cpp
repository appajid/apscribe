/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QTabBar>
#include <harfbuzz/hb.h>

#include "testScriptShaping.h"
#include "appmodes.h"
#include "commonstrings.h"
#include "pageitem_textframe.h"
#include "pageitem_table.h"
#include "prefsmanager.h"
#include "scribus.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "scribusview.h"
#include "selection.h"
#include "ui/contentpalette.h"
#include "ui/directionselect.h"
#include "undomanager.h"
#include "text/textshaper.h"
#include "text/textlayout.h"

namespace
{
QList<ScFace> scriptFaces(const QStringList& families, const QString& text)
{
	const auto& fonts = PrefsManager::instance().appPrefs.fontPrefs.AvailFonts;
	QList<ScFace> result;
	for (const QString& family : families)
	{
		QList<ScFace> matches;
		for (auto it = fonts.cbegin(); it != fonts.cend(); ++it)
		{
			const ScFace& face = it.value();
			if (face.family() != family || face.isReplacement())
				continue;
			bool coverage = true;
			for (const QChar ch : text)
				if (!ch.isSpace() && ch.category() != QChar::Other_Format && !face.canRender(ch))
					coverage = false;
			if (coverage)
				matches.append(face);
		}
		if (matches.isEmpty())
			continue;
		// Keep proof sheets readable, but exercise a second real face too. Do
		// not synthesize bold or let a similarly named family satisfy coverage.
		int regular = 0;
		for (int i = 0; i < matches.size(); ++i)
			if (matches[i].style().compare("Regular", Qt::CaseInsensitive) == 0)
			{
				regular = i;
				break;
			}
		result.append(matches[regular]);
		for (const auto& face : matches)
			if (face.scName() != matches[regular].scName())
			{
				result.append(face);
				break;
			}
	}
	return result;
}

ScFace scriptFont(const QStringList& families, const QString& text)
{
	const auto faces = scriptFaces(families, text);
	return faces.isEmpty() ? ScFace() : faces.constFirst();
}

bool requireFonts()
{
	return qEnvironmentVariableIntValue("APSCRIBE_REQUIRE_SCRIPT_FONTS") != 0;
}

ScribusDoc* newDocument()
{
	return ScCore->primaryMainWindow()->doFileNew(595, 842, 20, 20, 20, 20,
		0, 1, false, 0, 0, 0, 0, 1, QSizeF(), true);
}

PageItem_TextFrame* addFrame(ScribusDoc* doc, const QString& text, const ScFace& face, int direction)
{
	const int index = doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified,
		50, 50, 490, 200, 0, CommonStrings::None, CommonStrings::None);
	if (index < 0)
		return nullptr;
	auto* frame = doc->Items->at(index)->asTextFrame();
	ParagraphStyle paragraph;
	paragraph.setParent(CommonStrings::DefaultParagraphStyle);
	paragraph.setDirection(static_cast<ParagraphStyle::DirectionType>(direction));
	paragraph.setAlignment(direction == ParagraphStyle::RTL ? ParagraphStyle::RightAligned : ParagraphStyle::LeftAligned);
	paragraph.setLineSpacingMode(ParagraphStyle::FixedLineSpacing);
	paragraph.setLineSpacing(34);
	paragraph.charStyle().setFont(face);
	paragraph.charStyle().setFontSize(240);
	frame->itemText.setDefaultStyle(paragraph);
	frame->itemText.insertChars(0, text);
	frame->invalidateLayout();
	frame->layout();
	return frame;
}
}

void TestScriptShaping::cleanup()
{
	auto* window = ScCore->primaryMainWindow();
	if (window->HaveDoc)
	{
		window->doc->appMode = modeNormal;
		window->doc->setModified(false);
		window->slotFileClose();
	}
}

void TestScriptShaping::shaping_data()
{
	QTest::addColumn<QString>("text");
	QTest::addColumn<QStringList>("families");
	QTest::addColumn<QString>("fontName");
	QTest::addColumn<QString>("language");
	QTest::addColumn<QString>("script");
	QTest::addColumn<bool>("rtl");
	auto sample = [](const char* tag, const QString& text, const QStringList& families,
	                 const QString& language, const QString& script, bool rtl) {
		const auto faces = scriptFaces(families, text);
		if (faces.isEmpty())
		{
			// Do not silently drop a script when expanding the font matrix.
			QTest::newRow(tag) << text << families << QString() << language << script << rtl;
			return;
		}
		for (const auto& face : faces)
		{
			const QByteArray name = QByteArray(tag) + "--" + face.scName().toUtf8();
			QTest::newRow(name.constData()) << text << families << face.scName() << language << script << rtl;
		}
	};
	sample("telugu-conjunct", QString::fromUtf8("క్షి"), {"Noto Sans Telugu", "Telugu Sangam MN", "Nirmala UI"}, "te", "Telu", false);
	sample("devanagari-reordering", QString::fromUtf8("क्षि"), {"Noto Sans Devanagari", "Devanagari Sangam MN", "Nirmala UI"}, "hi", "Deva", false);
	sample("bengali-reordering", QString::fromUtf8("ক্ষি"), {"Noto Sans Bengali", "Bangla Sangam MN", "Nirmala UI"}, "bn", "Beng", false);
	sample("tamil", QString::fromUtf8("ஸ்ரீ"), {"Noto Sans Tamil", "Tamil Sangam MN", "Nirmala UI"}, "ta", "Taml", false);
	sample("kannada", QString::fromUtf8("ಕ್ಷಿ"), {"Noto Sans Kannada", "Kannada Sangam MN", "Nirmala UI"}, "kn", "Knda", false);
	sample("malayalam", QString::fromUtf8("ക്ഷി"), {"Noto Sans Malayalam", "Malayalam Sangam MN", "Nirmala UI"}, "ml", "Mlym", false);
	sample("gujarati", QString::fromUtf8("ક્ષિ"), {"Noto Sans Gujarati", "Gujarati Sangam MN", "Nirmala UI"}, "gu", "Gujr", false);
	sample("gurmukhi", QString::fromUtf8("ਕ੍ਰਿ"), {"Noto Sans Gurmukhi", "Gurmukhi Sangam MN", "Nirmala UI"}, "pa", "Guru", false);
	sample("odia", QString::fromUtf8("କ୍ଷି"), {"Noto Sans Oriya", "Oriya Sangam MN", "Nirmala UI"}, "or", "Orya", false);
	sample("sinhala", QString::fromUtf8("ශ්‍රී"), {"Noto Sans Sinhala", "Sinhala Sangam MN", "Nirmala UI"}, "si", "Sinh", false);
	sample("arabic-marks", QString::fromUtf8("السَّلَامُ"), {"Noto Naskh Arabic", "Geeza Pro", "Arial", "DejaVu Sans"}, "ar", "Arab", true);
	sample("hebrew-marks", QString::fromUtf8("שָׁלוֹם"), {"Noto Sans Hebrew", "Arial Hebrew", "Arial", "DejaVu Sans"}, "he", "Hebr", true);
	sample("devanagari-zwj", QString::fromUtf8("क्\u200dषि"), {"Noto Sans Devanagari", "Devanagari Sangam MN", "Nirmala UI"}, "hi", "Deva", false);
	sample("devanagari-zwnj", QString::fromUtf8("क्\u200cषि"), {"Noto Sans Devanagari", "Devanagari Sangam MN", "Nirmala UI"}, "hi", "Deva", false);
}

void TestScriptShaping::shaping()
{
	QFETCH(QString, text);
	QFETCH(QStringList, families);
	QFETCH(QString, fontName);
	QFETCH(QString, language);
	QFETCH(QString, script);
	QFETCH(bool, rtl);
	const ScFace face = fontName.isEmpty() ? ScFace()
		: PrefsManager::instance().appPrefs.fontPrefs.AvailFonts.value(fontName);
	if (face.isNone())
	{
		QVERIFY2(!requireFonts(), qPrintable("Install a script font: " + families.join(", ")));
		QSKIP(qPrintable("Missing real font coverage: " + families.join(", ")));
	}
	qInfo().noquote() << "Script QA font:" << face.scName() << face.fontFilePath();
	auto* doc = newDocument();
	QVERIFY(doc);
	auto* frame = addFrame(doc, text, face, rtl ? ParagraphStyle::RTL : ParagraphStyle::LTR);
	QVERIFY(frame);
	// English is deliberately used for Indic samples: Apscribe must infer
	// the script language, without rewriting the user's stored style.
	CharStyle style;
	style.setLanguage(rtl ? language : "en");
	frame->itemText.applyCharStyle(0, text.length(), style);
	TextShaper shaper(frame, frame->itemText, 0);
	const ShapedText shaped = shaper.shape(0, text.length());
	QVERIFY(shaped.isValid());
	QVERIFY(!shaped.glyphs().isEmpty());
	QCOMPARE(frame->itemText.text(0, text.length()), text);
	QCOMPARE(frame->itemText.charStyle(0).language(), rtl ? language : QString("en"));

	// Independent HarfBuzz oracle with explicit script/language/direction.
	// This tests Apscribe's itemization, inferred language, cluster mapping,
	// offsets and glyph conversion, not just the presence of ink on a page.
	auto* font = static_cast<hb_font_t*>(face.hbFont());
	QVERIFY(font);
	hb_buffer_t* buffer = hb_buffer_create();
	const auto releaseBuffer = qScopeGuard([buffer] { hb_buffer_destroy(buffer); });
	hb_buffer_add_utf16(buffer, text.utf16(), text.length(), 0, text.length());
	hb_buffer_set_direction(buffer, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
	hb_buffer_set_script(buffer, hb_script_from_string(script.toUtf8().constData(), -1));
	hb_buffer_set_language(buffer, hb_language_from_string(language.toUtf8().constData(), -1));
	hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
	const char* shapers[] = {"ot", "fallback", nullptr};
	QVERIFY(hb_shape_full(font, buffer, nullptr, 0, shapers));
	unsigned int count = 0;
	const auto* infos = hb_buffer_get_glyph_infos(buffer, &count);
	const auto* positions = hb_buffer_get_glyph_positions(buffer, nullptr);
	QList<GlyphLayout> actual;
	QList<bool> covered(text.length(), false);
	for (const auto& cluster : shaped.glyphs())
	{
		QVERIFY(cluster.firstChar() >= 0 && cluster.lastChar() < text.length());
		QVERIFY(cluster.lastChar() >= cluster.firstChar());
		QCOMPARE(cluster.hasFlag(ScLayout_RightToLeft), rtl);
		for (int i = cluster.firstChar(); i <= cluster.lastChar(); ++i)
			covered[i] = true;
		actual.append(cluster.glyphs());
	}
	QVERIFY(!covered.contains(false));
	QCOMPARE(actual.size(), qsizetype(count));
	for (int i = 0; i < actual.size(); ++i)
	{
		QVERIFY(infos[i].codepoint != 0);
		QCOMPARE(actual[i].glyph, infos[i].codepoint);
		QVERIFY(qAbs(actual[i].xadvance - positions[i].x_advance / 10.0) < 0.001);
		QVERIFY(qAbs(actual[i].xoffset - positions[i].x_offset / 10.0) < 0.001);
		QVERIFY(qAbs(actual[i].yoffset + positions[i].y_offset / 10.0) < 0.001);
	}
	frame->invalidateLayout();
	frame->layout();
	QVERIFY(!frame->frameOverflows());
	const QString output = qEnvironmentVariable("APSCRIBE_SCRIPT_QA_OUTPUT");
	if (!output.isEmpty())
	{
		QVERIFY(QDir().mkpath(output));
		const QString tag = QString::fromUtf8(QTest::currentDataTag());
		QString filename = tag;
		filename.replace(QRegularExpression("[^A-Za-z0-9_-]"), "_");
		const QImage rendered = frame->DrawObj_toImage(1400);
		QVERIFY(!rendered.isNull());
		QImage proof(rendered.width(), rendered.height() + 64, QImage::Format_ARGB32_Premultiplied);
		proof.fill(Qt::white);
		{
			QPainter painter(&proof);
			painter.setPen(Qt::black);
			painter.drawText(QRect(12, 4, proof.width() - 24, 52), Qt::AlignLeft | Qt::TextWordWrap,
				tag + "\nHarfBuzz comparison passed; visual review pending");
			painter.drawImage(0, 64, rendered);
		}
		QVERIFY(proof.save(QDir(output).filePath(filename + ".png")));
		const QJsonObject metadata {
			{"sample", tag}, {"text", text}, {"font", face.scName()},
			{"family", face.family()}, {"style", face.style()},
			{"script", script}, {"language", language}, {"rtl", rtl},
			{"glyphCount", actual.size()}, {"platform", QApplication::platformName()},
			{"oracle", "HarfBuzz"}, {"visualReview", "pending"},
			{"image", filename + ".png"}
		};
		QSaveFile report(QDir(output).filePath(filename + ".json"));
		QVERIFY(report.open(QIODevice::WriteOnly));
		const QByteArray json = QJsonDocument(metadata).toJson();
		QCOMPARE(report.write(json), qint64(json.size()));
		QVERIFY(report.commit());
	}
}

void TestScriptShaping::mixedDirectionAndPersistence()
{
	const QString arabic = QString::fromUtf8("السَّلَامُ");
	// The Linux Noto Arabic package need not contain Latin glyphs. Require one
	// genuinely covering face rather than accepting fallback or skipping QA.
	const ScFace face = scriptFont({"Noto Naskh Arabic", "Geeza Pro", "Arial", "DejaVu Sans"}, arabic + "ABC 123()");
	if (face.isNone())
	{
		QVERIFY2(!requireFonts(), "Install a font covering Arabic and Latin");
		QSKIP("No real mixed Arabic/Latin font");
	}
	qInfo().noquote() << "Mixed-direction QA font:" << face.scName() << face.fontFilePath();
	auto* doc = newDocument();
	QVERIFY(doc);
	const QString text = arabic + " (ABC 123) " + arabic;
	auto* frame = addFrame(doc, text, face, ParagraphStyle::LTR);
	QVERIFY(frame);
	auto* window = ScCore->primaryMainWindow();
	doc->m_Selection->addItem(frame);
	window->view->requestMode(modeEdit);
	frame->itemText.setCursorPosition(0);
	window->contentPalette->handleSelectionChanged();
	auto* tabs = window->contentPalette->findChild<QTabBar*>("textInspectorTabs");
	QVERIFY(tabs);
	tabs->setCurrentIndex(1);
	auto* direction = window->contentPalette->findChild<DirectionSelect*>("textDirection");
	QVERIFY(direction && direction->isEnabled());
	direction->RTL->click();
	QCOMPARE(frame->itemText.paragraphStyle(0).direction(), ParagraphStyle::RTL);
	QCOMPARE(frame->itemText.paragraphStyle(0).alignment(), int(ParagraphStyle::RightAligned));
	QCOMPARE(frame->itemText.text(0, text.length()), text);
	UndoManager::instance()->undo(1);
	QCOMPARE(frame->itemText.paragraphStyle(0).direction(), ParagraphStyle::LTR);
	QCOMPARE(frame->itemText.paragraphStyle(0).alignment(), int(ParagraphStyle::LeftAligned));
	UndoManager::instance()->redo(1);
	QCOMPARE(frame->itemText.paragraphStyle(0).direction(), ParagraphStyle::RTL);
	TextShaper shaper(frame, frame->itemText, 0);
	const ShapedText shaped = shaper.shape(0, text.length());
	bool foundRTL = false;
	bool foundLTR = false;
	for (const auto& cluster : shaped.glyphs())
	{
		if (cluster.getText().contains('A'))
		{
			QVERIFY(!cluster.hasFlag(ScLayout_RightToLeft));
			foundLTR = true;
		}
		if (cluster.firstChar() == 0)
		{
			QVERIFY(cluster.hasFlag(ScLayout_RightToLeft));
			foundRTL = true;
		}
	}
	QVERIFY(foundRTL && foundLTR);
	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	const QString path = directory.filePath("mixed-script.sla");
	window->view->requestMode(modeNormal);
	QVERIFY(window->DoFileSave(path));
	doc->setModified(false);
	QVERIFY(window->slotFileClose());
	QVERIFY(window->loadDoc(path));
	frame = window->doc->Items->at(0)->asTextFrame();
	QVERIFY(frame);
	QCOMPARE(frame->itemText.text(0, frame->itemText.length()), text);
	QCOMPARE(frame->itemText.paragraphStyle(0).direction(), int(ParagraphStyle::RTL));
	QCOMPARE(frame->itemText.charStyle(0).font().scName(), face.scName());
	frame->layout();
	QVERIFY(!frame->frameOverflows());
}

void TestScriptShaping::directionInspector_data()
{
	QTest::addColumn<int>("alignment");
	QTest::addColumn<bool>("rtl");
	QTest::addColumn<bool>("table");
	for (bool table : {false, true})
		for (bool rtl : {false, true})
			for (int alignment = 0; alignment < 5; ++alignment)
				QTest::newRow(qPrintable(QString("%1-%2-align-%3").arg(table ? "cell" : "frame").arg(rtl ? "rtl" : "ltr").arg(alignment))) << alignment << rtl << table;
}

void TestScriptShaping::directionInspector()
{
	QFETCH(int, alignment);
	QFETCH(bool, rtl);
	QFETCH(bool, table);
	auto* doc = newDocument();
	QVERIFY(doc);
	const int index = doc->itemAdd(table ? PageItem::Table : PageItem::TextFrame,
		PageItem::Unspecified, 50, 50, 400, 200, 0, CommonStrings::None, CommonStrings::None);
	QVERIFY(index >= 0);
	auto* item = doc->Items->at(index);
	auto* frame = table ? item->asTable()->activeCell().textFrame() : item->asTextFrame();
	QVERIFY(frame);
	const QString text = QStringLiteral("First paragraph") + SpecialChars::PARSEP + "Second paragraph";
	frame->itemText.insertChars(0, text);
	ParagraphStyle initial;
	initial.setDirection(rtl ? ParagraphStyle::LTR : ParagraphStyle::RTL);
	initial.setAlignment(static_cast<ParagraphStyle::AlignmentType>(alignment));
	frame->itemText.applyStyle(0, initial);
	const ParagraphStyle otherParagraph = frame->itemText.paragraphStyle(text.length());
	doc->m_Selection->addItem(item);
	auto* window = ScCore->primaryMainWindow();
	window->view->requestMode(table ? modeEditTable : modeEdit);
	frame->itemText.setCursorPosition(0);
	window->contentPalette->handleSelectionChanged();
	auto* tabs = window->contentPalette->findChild<QTabBar*>("textInspectorTabs");
	QVERIFY(tabs);
	tabs->setCurrentIndex(1);
	auto* direction = window->contentPalette->findChild<DirectionSelect*>("textDirection");
	QVERIFY(direction && direction->isEnabled());
	(rtl ? direction->RTL : direction->LTR)->click();
	const int target = rtl ? ParagraphStyle::RTL : ParagraphStyle::LTR;
	int expectedAlignment = alignment;
	if (rtl && alignment == ParagraphStyle::LeftAligned)
		expectedAlignment = ParagraphStyle::RightAligned;
	if (!rtl && alignment == ParagraphStyle::RightAligned)
		expectedAlignment = ParagraphStyle::LeftAligned;
	QCOMPARE(frame->itemText.paragraphStyle(0).direction(), target);
	QCOMPARE(frame->itemText.paragraphStyle(0).alignment(), expectedAlignment);
	QCOMPARE(frame->itemText.paragraphStyle(text.length()), otherParagraph);
	QCOMPARE(frame->itemText.text(0, text.length()), text);
	UndoManager::instance()->undo(1);
	QCOMPARE(frame->itemText.paragraphStyle(0).direction(), initial.direction());
	QCOMPARE(frame->itemText.paragraphStyle(0).alignment(), alignment);
	UndoManager::instance()->redo(1);
	QCOMPARE(frame->itemText.paragraphStyle(0).direction(), target);
	QCOMPARE(frame->itemText.paragraphStyle(0).alignment(), expectedAlignment);
	QCOMPARE(frame->itemText.paragraphStyle(text.length()), otherParagraph);
}
