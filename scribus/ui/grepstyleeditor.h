/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef GREPSTYLEEDITOR_H
#define GREPSTYLEEDITOR_H

#include <QWidget>
#include <QStringList>

#include "grepstylerules.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class GrepStyleEditor : public QWidget
{
	Q_OBJECT

public:
	explicit GrepStyleEditor(QWidget* parent = nullptr);
	void setCharacterStyles(const QStringList& names);
	void setRules(const QString& json, bool inherited, bool canInherit, bool editable);

signals:
	void rulesEdited(const QString& json, bool inherit);

private:
	void refreshList();
	void refreshSelection();
	void validateAndEmit();
	void moveRule(int offset);

	QList<GrepStyleRules::Rule> m_rules;
	QStringList m_styleNames;
	bool m_loading { false };
	bool m_editable { true };
	QListWidget* m_list { nullptr };
	QLineEdit* m_pattern { nullptr };
	QComboBox* m_characterStyle { nullptr };
	QCheckBox* m_ignoreCase { nullptr };
	QCheckBox* m_inherit { nullptr };
	QLabel* m_message { nullptr };
	QPushButton* m_add { nullptr };
	QPushButton* m_remove { nullptr };
	QPushButton* m_up { nullptr };
	QPushButton* m_down { nullptr };
};

#endif
