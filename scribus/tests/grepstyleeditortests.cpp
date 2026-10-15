/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include "grepstylerules.h"
#include "ui/grepstyleeditor.h"

class GrepStyleEditorTests : public QObject
{
	Q_OBJECT

private slots:
	void explainsWhyRulesCannotBeAddedWithoutCharacterStyles();
	void editsOrderedRulesAndRejectsInvalidPatterns();
	void preservesInheritanceAndDisablesMultiSelection();
};

void GrepStyleEditorTests::explainsWhyRulesCannotBeAddedWithoutCharacterStyles()
{
	GrepStyleEditor editor;
	editor.setRules(QString(), false, false, true);
	auto* add = editor.findChild<QPushButton*>(QStringLiteral("grepAddRule"));
	auto* emptyState = editor.findChild<QLabel*>(QStringLiteral("grepEmptyState"));
	QVERIFY(add && emptyState);
	QVERIFY(!add->isEnabled());
	QVERIFY(!emptyState->isHidden());
	QVERIFY(emptyState->text().contains(QStringLiteral("Character Style")));
	const QString screenshot = qEnvironmentVariable("APSCRIBE_GREP_EMPTY_SCREENSHOT");
	if (!screenshot.isEmpty())
	{
		editor.resize(560, 420);
		editor.show();
		QCoreApplication::processEvents();
		QVERIFY(editor.grab().save(screenshot));
	}

	editor.setCharacterStyles({ QStringLiteral("Accent") });
	QVERIFY(add->isEnabled());
	QVERIFY(emptyState->isHidden());
}

void GrepStyleEditorTests::editsOrderedRulesAndRejectsInvalidPatterns()
{
	GrepStyleEditor editor;
	editor.setCharacterStyles({ QStringLiteral("Accent"), QStringLiteral("Heading") });
	editor.setRules(QString(), false, false, true);
	QSignalSpy changes(&editor, &GrepStyleEditor::rulesEdited);
	auto* add = editor.findChild<QPushButton*>(QStringLiteral("grepAddRule"));
	auto* up = editor.findChild<QPushButton*>(QStringLiteral("grepMoveUp"));
	auto* pattern = editor.findChild<QLineEdit*>(QStringLiteral("grepRulePattern"));
	auto* style = editor.findChild<QComboBox*>(QStringLiteral("grepRuleCharacterStyle"));
	auto* ignoreCase = editor.findChild<QCheckBox*>(QStringLiteral("grepRuleIgnoreCase"));
	auto* message = editor.findChild<QLabel*>(QStringLiteral("grepRuleMessage"));
	QVERIFY(add && up && pattern && style && ignoreCase && message);

	add->click();
	QCOMPARE(changes.count(), 0);
	QVERIFY(!message->text().isEmpty());
	pattern->setText(QStringLiteral("chapter\\s+\\d+"));
	QCOMPARE(changes.count(), 1);
	ignoreCase->setChecked(true);
	QCOMPARE(changes.count(), 2);
	add->click();
	pattern->setText(QStringLiteral("\\bFigure\\b"));
	style->setCurrentIndex(style->findData(QStringLiteral("Heading")));
	QList<GrepStyleRules::Rule> rules;
	QVERIFY(GrepStyleRules::parse(changes.last().at(0).toString(), &rules));
	QCOMPARE(rules.size(), 2);
	QCOMPARE(rules.at(1).characterStyle, QStringLiteral("Heading"));

	up->click();
	QVERIFY(GrepStyleRules::parse(changes.last().at(0).toString(), &rules));
	QCOMPARE(rules.at(0).pattern, QStringLiteral("\\bFigure\\b"));
	const QString screenshot = qEnvironmentVariable("APSCRIBE_GREP_UI_SCREENSHOT");
	if (!screenshot.isEmpty())
	{
		editor.resize(560, 420);
		editor.show();
		QCoreApplication::processEvents();
		QVERIFY(editor.grab().save(screenshot));
	}
	const int validChangeCount = changes.count();
	pattern->setText(QStringLiteral("("));
	QCOMPARE(changes.count(), validChangeCount);
	QVERIFY(!message->text().isEmpty());
	pattern->setText(QStringLiteral("Figure"));
	QCOMPARE(changes.count(), validChangeCount + 1);
	QVERIFY(message->text().isEmpty());
}

void GrepStyleEditorTests::preservesInheritanceAndDisablesMultiSelection()
{
	GrepStyleEditor editor;
	editor.setCharacterStyles({ QStringLiteral("Accent") });
	const QList<GrepStyleRules::Rule> expected { { QStringLiteral("title"), QStringLiteral("Accent"), false } };
	editor.setRules(GrepStyleRules::serialize(expected), true, true, true);
	auto* inherit = editor.findChild<QCheckBox*>(QStringLiteral("grepInheritRules"));
	auto* add = editor.findChild<QPushButton*>(QStringLiteral("grepAddRule"));
	auto* list = editor.findChild<QListWidget*>(QStringLiteral("grepRuleList"));
	QVERIFY(inherit && add && list);
	QVERIFY(inherit->isChecked());
	QVERIFY(!add->isEnabled());
	QSignalSpy changes(&editor, &GrepStyleEditor::rulesEdited);
	inherit->setChecked(false);
	QCOMPARE(changes.count(), 1);
	QVERIFY(!changes.last().at(1).toBool());
	QList<GrepStyleRules::Rule> copied;
	QVERIFY(GrepStyleRules::parse(changes.last().at(0).toString(), &copied));
	QCOMPARE(copied, expected);
	inherit->setChecked(true);
	QVERIFY(changes.last().at(1).toBool());

	editor.setRules(GrepStyleRules::serialize(expected), false, true, false);
	QVERIFY(!add->isEnabled());
	QVERIFY(!list->isEnabled());
	QVERIFY(!inherit->isEnabled());
}

QTEST_MAIN(GrepStyleEditorTests)
#include "grepstyleeditortests.moc"
