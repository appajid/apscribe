/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef STYLECOMBOITEMS_H
#define STYLECOMBOITEMS_H

#include <QComboBox>
#include <QStringList>

namespace StyleComboItems
{
inline void update(QComboBox* combo, const QStringList& items)
{
	// Retain existing rows so an accessibility client does not lose every
	// cached child whenever the number of styles changes.
	const int sharedCount = qMin(combo->count(), items.count());
	for (int i = 0; i < sharedCount; ++i)
	{
		if (combo->itemText(i) != items[i])
			combo->setItemText(i, items[i]);
	}
	while (combo->count() > items.count())
		combo->removeItem(combo->count() - 1);
	for (int i = sharedCount; i < items.count(); ++i)
		combo->addItem(items[i]);
}
}

#endif
