/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include "grepstyleeditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

GrepStyleEditor::GrepStyleEditor(QWidget* parent) : QWidget(parent)
{
	auto* layout = new QVBoxLayout(this);
	auto* description = new QLabel(tr("Apply Character Styles to matching paragraph text. Rules run in order; later rules take precedence. Manual formatting remains unchanged."), this);
	description->setWordWrap(true);
	layout->addWidget(description);

	m_inherit = new QCheckBox(tr("Use parent paragraph style's GREP rules"), this);
	m_inherit->setObjectName(QStringLiteral("grepInheritRules"));
	layout->addWidget(m_inherit);

	m_list = new QListWidget(this);
	m_list->setObjectName(QStringLiteral("grepRuleList"));
	m_list->setMinimumHeight(110);
	layout->addWidget(m_list);

	auto* buttons = new QHBoxLayout();
	m_add = new QPushButton(tr("Add Rule"), this);
	m_add->setObjectName(QStringLiteral("grepAddRule"));
	m_remove = new QPushButton(tr("Remove"), this);
	m_remove->setObjectName(QStringLiteral("grepRemoveRule"));
	m_up = new QPushButton(tr("Move Up"), this);
	m_up->setObjectName(QStringLiteral("grepMoveUp"));
	m_down = new QPushButton(tr("Move Down"), this);
	m_down->setObjectName(QStringLiteral("grepMoveDown"));
	buttons->addWidget(m_add);
	buttons->addWidget(m_remove);
	buttons->addStretch();
	buttons->addWidget(m_up);
	buttons->addWidget(m_down);
	layout->addLayout(buttons);

	auto* form = new QFormLayout();
	m_pattern = new QLineEdit(this);
	m_pattern->setObjectName(QStringLiteral("grepRulePattern"));
	m_pattern->setPlaceholderText(tr("Regular expression"));
	form->addRow(tr("Pattern:"), m_pattern);
	m_characterStyle = new QComboBox(this);
	m_characterStyle->setObjectName(QStringLiteral("grepRuleCharacterStyle"));
	form->addRow(tr("Character Style:"), m_characterStyle);
	m_ignoreCase = new QCheckBox(tr("Ignore case"), this);
	m_ignoreCase->setObjectName(QStringLiteral("grepRuleIgnoreCase"));
	form->addRow(QString(), m_ignoreCase);
	layout->addLayout(form);

	m_message = new QLabel(this);
	m_message->setObjectName(QStringLiteral("grepRuleMessage"));
	m_message->setWordWrap(true);
	layout->addWidget(m_message);
	layout->addStretch();

	connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { refreshSelection(); });
	connect(m_add, &QPushButton::clicked, this, [this]() {
		m_rules.append({ QString(), m_styleNames.value(0), false });
		refreshList();
		m_list->setCurrentRow(m_rules.size() - 1);
		m_pattern->setFocus();
		validateAndEmit();
	});
	connect(m_remove, &QPushButton::clicked, this, [this]() {
		const int row = m_list->currentRow();
		if (row < 0)
			return;
		m_rules.removeAt(row);
		refreshList();
		m_list->setCurrentRow(qMin(row, m_rules.size() - 1));
		validateAndEmit();
	});
	connect(m_up, &QPushButton::clicked, this, [this]() { moveRule(-1); });
	connect(m_down, &QPushButton::clicked, this, [this]() { moveRule(1); });
	connect(m_pattern, &QLineEdit::textChanged, this, [this](const QString& pattern) {
		const int row = m_list->currentRow();
		if (m_loading || row < 0)
			return;
		m_rules[row].pattern = pattern;
		refreshList();
		validateAndEmit();
	});
	connect(m_characterStyle, &QComboBox::currentIndexChanged, this, [this](int index) {
		const int row = m_list->currentRow();
		if (m_loading || row < 0 || index < 0)
			return;
		m_rules[row].characterStyle = m_characterStyle->itemData(index).toString();
		refreshList();
		validateAndEmit();
	});
	connect(m_ignoreCase, &QCheckBox::toggled, this, [this](bool checked) {
		const int row = m_list->currentRow();
		if (m_loading || row < 0)
			return;
		m_rules[row].ignoreCase = checked;
		refreshList();
		validateAndEmit();
	});
	connect(m_inherit, &QCheckBox::toggled, this, [this](bool inherited) {
		if (m_loading)
			return;
		refreshSelection();
		if (inherited)
		{
			m_message->clear();
			emit rulesEdited(QString(), true);
		}
		else
			validateAndEmit();
	});
	refreshSelection();
}

void GrepStyleEditor::setCharacterStyles(const QStringList& names)
{
	m_styleNames = names;
	m_styleNames.removeDuplicates();
	refreshSelection();
}

void GrepStyleEditor::setRules(const QString& json, bool inherited, bool canInherit, bool editable)
{
	m_loading = true;
	m_editable = editable;
	QString error;
	if (!GrepStyleRules::parse(json, &m_rules, &error))
	{
		m_rules.clear();
		m_message->setText(tr("Stored GREP rules are invalid: %1").arg(error));
	}
	else
		m_message->setText(editable ? QString() : tr("Select one paragraph style to edit GREP rules."));
	m_inherit->setVisible(canInherit);
	m_inherit->setChecked(canInherit && inherited);
	refreshList();
	m_list->setCurrentRow(m_rules.isEmpty() ? -1 : 0);
	refreshSelection();
	m_loading = false;
}

void GrepStyleEditor::refreshList()
{
	const int row = m_list->currentRow();
	const auto labelForRule = [this](const GrepStyleRules::Rule& rule) {
		return tr("%1  →  %2%3")
			.arg(rule.pattern.isEmpty() ? tr("(enter a pattern)") : rule.pattern,
				rule.characterStyle.isEmpty() ? tr("(choose a style)") : rule.characterStyle,
				rule.ignoreCase ? tr("  (ignore case)") : QString());
	};
	if (m_list->count() == m_rules.size())
	{
		for (int i = 0; i < m_rules.size(); ++i)
			m_list->item(i)->setText(labelForRule(m_rules.at(i)));
		return;
	}
	QSignalBlocker blocker(m_list);
	m_list->clear();
	for (const auto& rule : m_rules)
		m_list->addItem(labelForRule(rule));
	m_list->setCurrentRow(qMin(row, m_rules.size() - 1));
	refreshSelection();
}

void GrepStyleEditor::refreshSelection()
{
	const int row = m_list->currentRow();
	const bool active = m_editable && !m_inherit->isChecked();
	const bool selected = active && row >= 0 && row < m_rules.size();
	m_list->setEnabled(active);
	m_add->setEnabled(active && !m_styleNames.isEmpty());
	m_remove->setEnabled(selected);
	m_up->setEnabled(selected && row > 0);
	m_down->setEnabled(selected && row < m_rules.size() - 1);
	m_pattern->setEnabled(selected);
	m_characterStyle->setEnabled(selected);
	m_ignoreCase->setEnabled(selected);
	m_inherit->setEnabled(m_editable);

	QSignalBlocker patternBlocker(m_pattern);
	QSignalBlocker styleBlocker(m_characterStyle);
	QSignalBlocker caseBlocker(m_ignoreCase);
	m_pattern->setText(selected ? m_rules.at(row).pattern : QString());
	m_characterStyle->clear();
	for (const QString& name : m_styleNames)
		m_characterStyle->addItem(name, name);
	if (selected)
	{
		const auto& rule = m_rules.at(row);
		int index = m_characterStyle->findData(rule.characterStyle);
		if (index < 0 && !rule.characterStyle.isEmpty())
		{
			m_characterStyle->addItem(tr("Missing: %1").arg(rule.characterStyle), rule.characterStyle);
			index = m_characterStyle->count() - 1;
		}
		m_characterStyle->setCurrentIndex(index);
		m_ignoreCase->setChecked(rule.ignoreCase);
	}
	else
		m_ignoreCase->setChecked(false);
}

void GrepStyleEditor::validateAndEmit()
{
	if (m_loading || m_inherit->isChecked())
		return;
	QList<GrepStyleRules::Rule> parsed;
	QString error;
	const QString json = GrepStyleRules::serialize(m_rules);
	if (!GrepStyleRules::parse(json, &parsed, &error))
	{
		m_message->setText(tr("Fix the rule before it can be applied: %1").arg(error));
		return;
	}
	m_message->clear();
	emit rulesEdited(json, false);
}

void GrepStyleEditor::moveRule(int offset)
{
	const int row = m_list->currentRow();
	const int next = row + offset;
	if (row < 0 || next < 0 || next >= m_rules.size())
		return;
	m_rules.swapItemsAt(row, next);
	refreshList();
	m_list->setCurrentRow(next);
	validateAndEmit();
}
