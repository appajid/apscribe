/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest>

#include <QAbstractItemView>
#include <QAccessible>
#include <QComboBox>
#include <QSignalSpy>

#include "ui/stylecomboitems.h"

class StyleComboItemsTests : public QObject
{
	Q_OBJECT

private slots:
	void preservesAccessibleRowsWhenLabelsChange();
	void preservesSharedRowsWhenCountChanges();
};

void StyleComboItemsTests::preservesAccessibleRowsWhenLabelsChange()
{
	QAccessible::setActive(true);
	QComboBox combo;
	combo.addItems({ QStringLiteral("Default"), QStringLiteral("Accent") });
	auto* accessible = QAccessible::queryAccessibleInterface(combo.view());
	QVERIFY(accessible);
	const QAccessible::Id defaultId = QAccessible::uniqueId(accessible->child(0));
	const QAccessible::Id accentId = QAccessible::uniqueId(accessible->child(1));
	QSignalSpy removed(combo.model(), &QAbstractItemModel::rowsRemoved);
	QSignalSpy inserted(combo.model(), &QAbstractItemModel::rowsInserted);

	StyleComboItems::update(&combo, { QStringLiteral("Default"), QStringLiteral("Emphasis") });
	QCOMPARE(combo.itemText(1), QStringLiteral("Emphasis"));
	QCOMPARE(removed.count(), 0);
	QCOMPARE(inserted.count(), 0);
	QCOMPARE(QAccessible::uniqueId(accessible->child(0)), defaultId);
	QCOMPARE(QAccessible::uniqueId(accessible->child(1)), accentId);
}

void StyleComboItemsTests::preservesSharedRowsWhenCountChanges()
{
	QAccessible::setActive(true);
	QComboBox combo;
	combo.addItems({ QStringLiteral("Default"), QStringLiteral("Accent") });
	auto* accessible = QAccessible::queryAccessibleInterface(combo.view());
	QVERIFY(accessible);
	const QAccessible::Id defaultId = QAccessible::uniqueId(accessible->child(0));
	const QAccessible::Id accentId = QAccessible::uniqueId(accessible->child(1));
	QSignalSpy removed(combo.model(), &QAbstractItemModel::rowsRemoved);
	QSignalSpy inserted(combo.model(), &QAbstractItemModel::rowsInserted);

	StyleComboItems::update(&combo, { QStringLiteral("Default"), QStringLiteral("Accent"), QStringLiteral("Heading") });
	QCOMPARE(combo.count(), 3);
	QCOMPARE(inserted.count(), 1);
	QCOMPARE(removed.count(), 0);
	QCOMPARE(QAccessible::uniqueId(accessible->child(0)), defaultId);
	QCOMPARE(QAccessible::uniqueId(accessible->child(1)), accentId);

	StyleComboItems::update(&combo, { QStringLiteral("Default"), QStringLiteral("Accent") });
	QCOMPARE(combo.count(), 2);
	QCOMPARE(removed.count(), 1);
	QCOMPARE(QAccessible::uniqueId(accessible->child(0)), defaultId);
	QCOMPARE(QAccessible::uniqueId(accessible->child(1)), accentId);
}

QTEST_MAIN(StyleComboItemsTests)
#include "stylecomboitemstests.moc"
