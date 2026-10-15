/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
#include "search.h"


#include <QApplication>
#include <QCursor>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QListView>
#include <QMessageBox>
#include <QScopedValueRollback>
#include <QScreen>
#include <QSizePolicy>
#include <QTimer>

#include "appmodes.h"
#include "canvas.h"
#include "colorcombo.h"
#include "colorlistbox.h"
#include "commonstrings.h"
#include "fontcombo.h"
#include "iconmanager.h"
#include "modernui.h"
#include "prefscontext.h"
#include "prefsfile.h"
#include "prefsmanager.h"
#include "regexsearch.h"
#include "scpage.h"
#include "scribus.h"
#include "scribusview.h"
#include "scrspinbox.h"
#include "text/specialchars.h"
#include "selection.h"
#include "shadebutton.h"
#include "styles/charstyle.h"
#include "styles/paragraphstyle.h"
#include "styleselect.h"
#include "ui/storyeditor.h"
#include "undomanager.h"
#include "undotransaction.h"
#include "util.h"
#include "util_text.h"

namespace
{
bool storyEditorRangeMatches(SEditor* editor, const StoryText& storyText, int start, int length)
{
	if (start < 0 || length <= 0 || start + length >= editor->document()->characterCount())
		return false;
	QTextCursor cursor(editor->document());
	cursor.setPosition(start);
	cursor.setPosition(start + length, QTextCursor::KeepAnchor);
	return cursor.selectedText() == storyText.text(start, length);
}
}

SearchReplace::SearchReplace(QWidget* parent, ScribusDoc *doc)
	: QDialog(parent)
{
	m_doc = doc;

	setupUi(this);
	ModernUI::applySurfaceStyle(this, "findReplace");
	ModernUI::markSections(this);
	ModernUI::setPrimaryAction(searchButton);
	searchTextValue->setAccessibleName(tr("Find text"));
	searchTextValue->setAccessibleDescription(tr("Text to find in the current story or selected text frames"));
	replaceTextValue->setAccessibleName(tr("Replacement text"));
	messageLabel->setAccessibleName(tr("Find and replace status"));
	// Native, non-destructive text-query presets inspired by GREPS (MIT).
	// They only restore dialog controls; matching and replacement stay in Apscribe.
	auto* queryRow = new QHBoxLayout();
	auto* queryLabel = new QLabel(tr("Saved text query:"), this);
	m_queryCombo = new QComboBox(this);
	m_queryCombo->setAccessibleName(tr("Saved find and replace query"));
	m_queryCombo->setToolTip(tr("Saved queries include text and text-matching options, not formatting criteria."));
	auto* saveQueryButton = new QPushButton(tr("Save..."), this);
	auto* deleteQueryButton = new QPushButton(tr("Delete"), this);
	queryRow->addWidget(queryLabel);
	queryRow->addWidget(m_queryCombo, 1);
	queryRow->addWidget(saveQueryButton);
	queryRow->addWidget(deleteQueryButton);
	searchReplaceLayout->insertLayout(0, queryRow);
	connect(saveQueryButton, &QPushButton::clicked, this, &SearchReplace::saveQuery);
	connect(deleteQueryButton, &QPushButton::clicked, this, &SearchReplace::deleteQuery);
	connect(m_queryCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
		if (index <= 0 || m_loadingQuery)
			return;
		const auto query = m_savedQueries.value(m_queryCombo->currentText());
		m_loadingQuery = true;
		clear();
		searchTextValue->setText(query.find);
		replaceTextValue->setText(query.replace);
		regexCheckBox->setChecked(query.regex);
		ignoreCaseCheckBox->setChecked(query.ignoreCase);
		wholeWordsCheckBox->setChecked(query.wholeWords);
		m_loadingQuery = false;
		updateButtonState();
	});
	auto markQueryModified = [this]() {
		if (!m_loadingQuery && m_queryCombo->currentIndex() > 0)
			m_queryCombo->setCurrentIndex(0);
	};
	connect(searchTextValue, &QLineEdit::textChanged, this, markQueryModified);
	connect(replaceTextValue, &QLineEdit::textChanged, this, markQueryModified);
	connect(regexCheckBox, &QCheckBox::toggled, this, markQueryModified);
	connect(ignoreCaseCheckBox, &QCheckBox::toggled, this, markQueryModified);
	connect(wholeWordsCheckBox, &QCheckBox::toggled, this, markQueryModified);

	QSizePolicy sp = messageLabel->sizePolicy();
	sp.setRetainSizeWhenHidden(true);
	messageLabel->setSizePolicy(sp);
	hideMessage();

	stopButton->setVisible(false);

	for (int i = 0; i < doc->paragraphStyles().count(); ++i)
	{
		const auto name = doc->paragraphStyles()[i].name();
		searchStyleComboBox->addItem(name);
		replaceStyleComboBox->addItem(name);
	}
	searchStyleComboBox->setCurrentIndex(findParagraphStyle(doc, doc->currentStyle));
	replaceStyleComboBox->setCurrentIndex(findParagraphStyle(doc, doc->currentStyle));
	for (int i = 0; i < doc->charStyles().count(); ++i)
		regexStyleComboBox->addItem(doc->charStyles()[i].name());
	regexStyleComboBox->setAccessibleName(tr("Character style for regular-expression matches"));
	applyRegexStyleButton->setToolTip(tr("Apply the selected character style to all matching canvas text without changing the text"));

	QStringList alignment{tr("Left"), tr("Center"), tr("Right"), tr("Block"), tr("Forced")};
	searchAlignmentComboBox->addItems(alignment);
	replaceAlignmentComboBox->addItems(alignment);

	searchFillColorComboBox->setPixmapType(ColorCombo::fancyPixmaps);
	searchFillColorComboBox->setColors(doc->PageColors, true);
	searchFillColorComboBox->setMinimumWidth(searchFillColorComboBox->view()->maximumViewportSize().width() + 24);
	setCurrentComboItem(searchFillColorComboBox, doc->currentStyle.charStyle().fillColor());

	searchStrokeColorComboBox->setPixmapType(ColorCombo::fancyPixmaps);
	searchStrokeColorComboBox->setColors(doc->PageColors, true);
	searchStrokeColorComboBox->view()->setMinimumWidth(searchStrokeColorComboBox->view()->maximumViewportSize().width() + 24);
	setCurrentComboItem(searchStrokeColorComboBox, doc->currentStyle.charStyle().strokeColor());

	replaceFillColorComboBox->setPixmapType(ColorCombo::fancyPixmaps);
	replaceFillColorComboBox->setColors(doc->PageColors, true);
	replaceFillColorComboBox->view()->setMinimumWidth(replaceFillColorComboBox->view()->maximumViewportSize().width() + 24);
	setCurrentComboItem(replaceFillColorComboBox, doc->currentStyle.charStyle().fillColor());
	replaceStrokeColorComboBox->setPixmapType(ColorCombo::fancyPixmaps);
	replaceStrokeColorComboBox->setColors(doc->PageColors, true);
	replaceStrokeColorComboBox->view()->setMinimumWidth(replaceStrokeColorComboBox->view()->maximumViewportSize().width() + 24);
	setCurrentComboItem(replaceStrokeColorComboBox, doc->currentStyle.charStyle().strokeColor());

	connect(collapseButton, &QPushButton::clicked, this, &SearchReplace::slotCollapseFormat);

	connect(clearButton, &QPushButton::clicked, this, &SearchReplace::clear);
	connect(closeButton, &QPushButton::clicked, this, &SearchReplace::accept);
	connect(replaceAllButton, &QPushButton::clicked, this, &SearchReplace::slotReplaceAll);
	connect(applyRegexStyleButton, &QPushButton::clicked, this, &SearchReplace::applyRegexCharacterStyle);
	connect(replaceButton, &QPushButton::clicked, this, &SearchReplace::slotReplace);
	connect(searchButton, &QPushButton::clicked, this, &SearchReplace::slotSearch);
	connect(previousButton, &QPushButton::clicked, this, &SearchReplace::slotSearchPrevious);
	connect(stopButton, &QPushButton::clicked, this, &SearchReplace::slotStop);

	connect(searchAlignmentCheckBox, &QCheckBox::clicked, [this](){enableAlignmentSearch(); updateButtonState();});
	connect(searchFillColorCheckBox, &QCheckBox::clicked, [this](){enableFillColorSearch(); updateButtonState();});
	connect(searchFillShadeCheckBox, &QCheckBox::clicked, [this](){enableFillShadeSearch(); updateButtonState();});
	connect(searchFontCheckBox, &QCheckBox::clicked, [this](){enableFontSearch(); updateButtonState();});
	connect(searchFontEffectsCheckBox, &QCheckBox::clicked, [this](){enableFontEffectsSearch(); updateButtonState();});
	connect(searchFontSizeCheckBox, &QCheckBox::clicked, [this](){enableFontSizeSearch(); updateButtonState();});
	connect(searchStrokeColorCheckBox, &QCheckBox::clicked, [this](){enableStrokeColorSearch(); updateButtonState();});
	connect(searchStrokeShadeCheckBox, &QCheckBox::clicked, [this](){enableStrokeShadeSearch(); updateButtonState();});
	connect(searchStyleCheckBox, &QCheckBox::clicked, [this](){enableStyleSearch(); updateButtonState();});
	connect(searchTextValue, &QLineEdit::textChanged, [this]() {hideMessage(); enableTextSearch(); updateButtonState();});
	connect(regexCheckBox, &QCheckBox::toggled, [this]() {hideMessage(); enableTextSearch(); updateButtonState();});

	connect(replaceAlignmentCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableAlignmentReplace);
	connect(replaceFillColorCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableFillColorReplace);
	connect(replaceFillShadeCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableFillShadeReplace);
	connect(replaceFontCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableFontReplace);
	connect(replaceFontEffectsCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableFontEffectsReplace);
	connect(replaceFontSizeCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableFontSizeReplace);
	connect(replaceStrokeColorCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableStrokeColorReplace);
	connect(replaceStrokeShadeCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableStrokeShadeReplace);
	connect(replaceStyleCheckBox, &QCheckBox::clicked, this, &SearchReplace::enableStyleReplace);
	connect(replaceTextValue, &QLineEdit::textChanged, this, &SearchReplace::updateButtonState);

	collapseButton->setToolTip( tr( "Show or hide the formatting options" ) );
	searchButton->setToolTip( tr( "Search for text or formatting in the current text" ) );
	replaceButton->setToolTip( tr( "Replace the searched for formatting with the replacement values" ) );
	replaceAllButton->setToolTip( tr( "Replace all found instances" ) );
	clearButton->setToolTip( tr( "Clear all search and replace options" ) );
	closeButton->setToolTip( tr( "Close search and replace" ) );

	m_prefs = PrefsManager::instance().prefsFile->getContext("SearchReplace");
	m_savedQueries = SearchQueryPresets::fromJson(m_prefs->get("savedTextQueries"));
	refreshQueryList();
	readPrefs();

	collapseFormat();
}

void SearchReplace::refreshQueryList(const QString& selected)
{
	m_loadingQuery = true;
	m_queryCombo->clear();
	m_queryCombo->addItem(tr("Custom"));
	m_queryCombo->addItems(m_savedQueries.keys());
	int index = 0;
	if (!selected.isEmpty())
	{
		for (int i = 1; i < m_queryCombo->count(); ++i)
		{
			if (m_queryCombo->itemText(i) == selected)
			{
				index = i;
				break;
			}
		}
	}
	m_queryCombo->setCurrentIndex(index);
	m_loadingQuery = false;
}

void SearchReplace::saveQuery()
{
	if (searchTextValue->text().isEmpty())
	{
		QMessageBox::information(this, tr("Save Query"), tr("Enter text to find before saving a query."));
		return;
	}
	bool accepted = false;
	const QString name = QInputDialog::getText(this, tr("Save Query"), tr("Query name:"),
		QLineEdit::Normal, QString(), &accepted).trimmed();
	if (!accepted || name.isEmpty())
		return;
	if (m_savedQueries.contains(name) && QMessageBox::question(this, tr("Replace Saved Query"),
		tr("Replace the existing query named “%1”?").arg(name),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;
	SearchTextQuery query;
	query.find = searchTextValue->text();
	query.replace = replaceTextValue->text();
	query.regex = regexCheckBox->isChecked();
	query.ignoreCase = ignoreCaseCheckBox->isChecked();
	query.wholeWords = wholeWordsCheckBox->isChecked();
	m_savedQueries.insert(name, query);
	m_prefs->set("savedTextQueries", SearchQueryPresets::toJson(m_savedQueries));
	refreshQueryList(name);
}

void SearchReplace::deleteQuery()
{
	const QString name = m_queryCombo->currentIndex() > 0 ? m_queryCombo->currentText() : QString();
	if (name.isEmpty())
		return;
	if (QMessageBox::question(this, tr("Delete Saved Query"),
		tr("Delete the saved query named “%1”?").arg(name),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;
	m_savedQueries.remove(name);
	m_prefs->set("savedTextQueries", SearchQueryPresets::toJson(m_savedQueries));
	refreshQueryList();
}

void SearchReplace::processCurrentSelection(QString selection)
{
	selection = selection.trimmed();
	if (!selection.isEmpty())
	{
		// \r newlines from the frames and QChar::ParagraphSeparator for the story editor
		if (selection.contains("\r") || selection.contains(QChar::ParagraphSeparator))
			// TODO: add a "Current selection only" checkbox
			/* enableSelectionSearch()*/ ;
		else
			searchTextValue->setText(selection);
	}
}

QPair<int, int> SearchReplace::cursorPosition()
{
	return { m_selectionStart, m_selectionEnd };
}

void SearchReplace::slotSearch()
{
	m_searching = true;
	updateButtonState();

	if (m_storyEditorMode)
		searchInStoryEditor();
	else
		searchOnCanvas();

	m_searching = false;
	updateButtonState();
}

void SearchReplace::searchInStoryEditor()
{
	hideMessage();

	auto options = getSearchOptions();

	if (!m_doc || !m_doc->scMW()->CurrStED)
		return;

	SEditor* storyTextEdit = m_doc->scMW()->CurrStED->Editor;
	StoryText& storyText = storyTextEdit->StyledText;
	QTextCursor cursor = storyTextEdit->textCursor();
	// TODO: as soon as we have them, use optional and const auto [start, end] = ...
	auto pos = searchStory(storyText, cursor.position(), storyText.length(), options);

	if (pos.first < 0)
	{
		showMessage(tr(messageEndOfSelection));
		pos = searchStory(storyText, 0, storyText.length(), options);
	}

	if (pos.first >= 0)
	{
		m_selectionStart = pos.first;
		m_selectionEnd = pos.second + 1;
		cursor.setPosition(pos.first);
		cursor.setPosition(pos.second + 1, QTextCursor::KeepAnchor);
		storyTextEdit->setTextCursor(cursor);
	}
	else
		showMessage(tr(messageNoMatch));
}

void SearchReplace::searchOnCanvas()
{
	hideMessage();
	auto options = getSearchOptions();
	if (!initCanvasSelection())
		return;

	PageItem* pageItem;
	int itemsToBeProcessed = m_pageItems.size();
	bool matchFound{false};

	while ((pageItem = currentPageItem()))
	{
		if (itemsToBeProcessed < 0)
			break;
		--itemsToBeProcessed;

		StoryText& storyText = pageItem->itemText;

		if (m_doc->appMode != modeEdit)
		{
			m_doc->view()->requestMode(modeEdit);
			pageItem->itemText.setCursorPosition(0);
		}

		auto pos = searchStory(storyText, storyText.cursorPosition(), storyText.length(), options);

		if (pos.first < 0)
		{
			nextPageItem();
			//If we're swtiching text chains, ensure we've deselected selected text
			if (pageItem != currentPageItem())
				storyText.deselectAll();
			pageItem = currentPageItem();

			if (m_endReached)
				showMessage(tr(messageEndOfSelection));

			pos = searchStory(storyText, storyText.cursorPosition(), storyText.length(), options);
		}

		if (pos.first >= 0)
		{
			storyText.select(pos.first, pos.second + 1 - pos.first);
			storyText.setCursorPosition(pos.second + 1);
			matchFound = true;
			break;
		}
	}
	updatePageItemSelection(pageItem);

	if (!matchFound)
		showMessage(tr(messageNoMatch));
}

QPair<int, int> SearchReplace::searchStoryBackward(const StoryText& storyText, const int beforePos, const int length, const Options& options)
{
	if (length == 0 || beforePos <= 0)
		return { -1, -1 };

	// Text path: jump straight to the previous occurrence via lastIndexOf,
	// then reuse getMatchRange() for wholeWords / format validation.
	if (options.textEnabled)
	{
		if (options.regularExpression)
		{
			const QString text = storyText.plainText();
			int before = beforePos;
			while (before > 0)
			{
				const auto candidate = RegexSearch::findPrevious(text, options.compiledRegex, before, options.wholeWords);
				if (candidate.start < 0)
					break;
				const auto match = getMatchRange(storyText, candidate.start, length, options, &text);
				if (match.start == candidate.start)
					return { match.start, match.end };
				before = candidate.start;
			}
			return { -1, -1 };
		}
		const Qt::CaseSensitivity cs = options.ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
		int searchFrom = beforePos - 1;
		while (searchFrom >= 0)
		{
			if (!m_searching)
				break;
			const int candidate = storyText.lastIndexOf(options.text, searchFrom, cs);
			if (candidate < 0)
				break;
			const auto match = getMatchRange(storyText, candidate, length, options);
			if (match.start == candidate)
				return { match.start, match.end };
			searchFrom = candidate - 1;   // first char matched but wholeWords/format rejected it
		}
		return { -1, -1 };
	}

	// Format-only path (no search text): the contiguous-region logic doesn't
	// reverse cleanly, so keep the collect-and-keep-last sweep here.
	QPair<int, int> result { -1, -1 };
	int from = 0;
	while (from < beforePos)
	{
		if (!m_searching)
			break;
		const auto pos = searchStory(storyText, from, length, options);
		if (pos.first < 0 || pos.first >= beforePos)
			break;
		result = pos;
		from = pos.second + 1;
	}
	return result;
}

void SearchReplace::searchInStoryEditorBackward()
{
	hideMessage();
	auto options = getSearchOptions();
	if (!m_doc || !m_doc->scMW()->CurrStED)
		return;

	SEditor* storyTextEdit = m_doc->scMW()->CurrStED->Editor;
	StoryText& storyText = storyTextEdit->StyledText;
	QTextCursor cursor = storyTextEdit->textCursor();

	auto pos = searchStoryBackward(storyText, cursor.selectionStart(), storyText.length(), options);
	if (pos.first < 0)
	{
		showMessage(tr(messageEndOfSelection));
		pos = searchStoryBackward(storyText, storyText.length(), storyText.length(), options); // wrap to end
	}

	if (pos.first >= 0)
	{
		m_selectionStart = pos.first;
		m_selectionEnd = pos.second + 1;
		cursor.setPosition(pos.first);
		cursor.setPosition(pos.second + 1, QTextCursor::KeepAnchor);
		storyTextEdit->setTextCursor(cursor);
	}
	else
		showMessage(tr(messageNoMatch));
}

void SearchReplace::searchOnCanvasBackward()
{
	hideMessage();
	auto options = getSearchOptions();
	if (!initCanvasSelection())
		return;

	PageItem* pageItem;
	int itemsToBeProcessed = m_pageItems.size();
	bool matchFound{false};

	while ((pageItem = currentPageItem()))
	{
		if (itemsToBeProcessed < 0)
			break;
		--itemsToBeProcessed;

		StoryText& storyText = pageItem->itemText;

		if (m_doc->appMode != modeEdit)
		{
			m_doc->view()->requestMode(modeEdit);
			storyText.setCursorPosition(storyText.length());
		}

		const int beforePos = storyText.selectionLength() > 0 ? storyText.startOfSelection() : storyText.cursorPosition();
		auto pos = searchStoryBackward(storyText, beforePos, storyText.length(), options);

		if (pos.first < 0)
		{
			previousPageItem();
			if (pageItem != currentPageItem())
				storyText.deselectAll();
			if (m_endReached)
				showMessage(tr(messageEndOfSelection));

			// search the previous frame fully, from its end
			if (PageItem* prev = currentPageItem())
			{
				prev->itemText.deselectAll();
				prev->itemText.setCursorPosition(prev->itemText.length());
			}
			continue;
		}

		storyText.select(pos.first, pos.second + 1 - pos.first);
		storyText.setCursorPosition(pos.first);   // cursor at match start so the next Previous goes further back
		matchFound = true;
		break;
	}
	updatePageItemSelection(pageItem);

	if (!matchFound)
		showMessage(tr(messageNoMatch));
}

void SearchReplace::previousPageItem()
{
	if (m_pageItems.empty())
		return;

	m_endReached = false;
	if (m_currentPageItem == 0)
	{
		m_currentPageItem = m_pageItems.size() - 1;   // wrap to last
		m_endReached = true;
	}
	else
		--m_currentPageItem;

	auto pageItem = m_pageItems.at(m_currentPageItem);
	if (!m_endReached && pageItem == m_doc->m_Selection->itemAt(0))
		return;

	selectPageItem(pageItem);
}

bool SearchReplace::initCanvasSelection()
{
	if (!m_doc || m_doc->Items->isEmpty())
		return false;

	if (m_doc->m_Selection->isEmpty())
	{
		// TODO: search the whole document or the whole page?
		readAllPageItems();
		if (m_pageItems.empty())
			return false;
		selectPageItem(m_pageItems.at(0));
	}
	else
		readSelectedPageItems();
	return !m_pageItems.empty();
}

void SearchReplace::readAllPageItems()
{
	if (!m_pageItems.empty())
		return;

	QMap<int, std::vector<PageItem*>> pages;
	PageItem* pageItem;
	for (int i = 0; i < m_doc->Items->count(); i++)
	{
		pageItem = m_doc->Items->at(i);

		if (!(pageItem->isTextFrame()))
			continue;

		// text chains are automatically searched until the end
		if (pageItem->prevInChain())
			continue;

		pages[pageItem->OwnPage].push_back(pageItem);
	}

	// TODO: add it as compareCoordinates to util.h, or pageItem.h... or implement the < operator for PageItem.
	auto sortCoordinates = [](const PageItem* a, const PageItem* b) {
		return a->yPos() == b->yPos() ? (a->xPos() < b->xPos()) : (a->yPos() < b->yPos());
	};

	for (int i = m_doc->currentPageNumber(); i < m_doc->DocPages.count(); i++)
	{
		auto pageItems = pages.value(i, {});
		if (pageItems.empty())
			continue;

		std::sort(pageItems.begin(), pageItems.end(), sortCoordinates);

		m_pageItems.insert(m_pageItems.end(), pageItems.begin(), pageItems.end() );
	}

	for (int i = 0; i < m_doc->currentPageNumber(); i++)
	{
		auto pageItems = pages.value(i, {});
		if (pageItems.empty())
			continue;

		std::sort(pageItems.begin(), pageItems.end(), sortCoordinates);

		m_pageItems.insert(m_pageItems.end(), pageItems.begin(), pageItems.end() );
	}
}

// TODO: if only one frame is selected: add an option to search for the whole story (automatically enabled; currently it just follows the story)
void SearchReplace::readSelectedPageItems()
{
	// TODO: sort by position?
	// only read once
	if (!m_pageItems.empty())
		return;

	m_currentPageItem = 0;

	if (m_doc->m_Selection->count() == 0)
		return;

	for (int i = 0 ; i < m_doc->m_Selection->count(); i++)
	{
		auto pageItem = m_doc->m_Selection->itemAt(i);

		if (!pageItem->isTextFrame())
			continue;

		m_pageItems.push_back(pageItem);
	}
}

// TODO: as soon as available, use std::optional
PageItem* SearchReplace::currentPageItem()
{
	if (m_pageItems.empty() || m_currentPageItem >= m_pageItems.size())
		return nullptr;
	return m_pageItems.at(m_currentPageItem);
}

void SearchReplace::nextPageItem()
{
	m_endReached = false;
	++m_currentPageItem;
	if (m_currentPageItem >= m_pageItems.size())
	{
		m_currentPageItem = 0;
		m_endReached = true;
	}

	auto pageItem = m_pageItems.at(m_currentPageItem);

	if (!m_endReached && pageItem == m_doc->m_Selection->itemAt(0))
		return;

	selectPageItem(pageItem);
}

void SearchReplace::selectPageItem(PageItem* pageItem)
{
	m_doc->m_Selection->clear();
	m_doc->m_Selection->addItem(pageItem);
	m_doc->view()->requestMode(modeNormal);
}

SearchReplace::MatchRange SearchReplace::getMatchRange(const StoryText& storyText, int position, const int length, const Options& options, const QString* plainText)
{
	if (length == 0)
		return { -1, 0, 1 };

	int matchStart = position;
	int matchEnd = position;

	if (options.textEnabled)
	{
		// match the text
		int matchLength = 0;
		if (options.regularExpression)
		{
			const QString fallback = plainText ? QString() : storyText.plainText();
			const auto match = RegexSearch::findNext(plainText ? *plainText : fallback, options.compiledRegex, matchStart, options.wholeWords);
			matchStart = match.start;
			matchLength = match.length;
		}
		else
		{
			Qt::CaseSensitivity cs = options.ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
			matchStart = storyText.indexOf(options.text, matchStart, cs, &matchLength);
		}

		if (matchStart < 0)
			// there is no match after position
			return {-1, 0, length + 1};

		// TODO: can it be longer than length?
		// if yes qMin(i + matchLength, length[ -1])
		matchEnd = matchStart + matchLength - 1;

		if (options.wholeWords && !options.regularExpression)
		{
			if (matchStart > 0 && storyText.text(matchStart - 1).isLetterOrNumber())
				return {-1, 0, matchEnd + 1};
			if (matchEnd + 1 < length && storyText.text(matchEnd + 1).isLetterOrNumber())
				return {-1, 0, matchEnd + 1};
		}
	}

	// check the first char or the matched text
	// TODO: is it possible/worth to include all contiguous matching when not textEnabled? (move in here the loop below)
	bool formatMatching{true};
	for (int i = matchStart; i <= matchEnd; ++i)
	{
		const auto& parStyle = storyText.paragraphStyle(i);
		const auto& charStyle = storyText.charStyle(i);
		formatMatching = isFormatMatching(parStyle, charStyle, options);
		if (!formatMatching)
			break;
	}
	if (!formatMatching)
	{
		// skip all contiguous non matching chars
		for (int i = matchEnd; i <= length; ++i)
		{
			const auto& parStyle = storyText.paragraphStyle(i);
			const auto& charStyle = storyText.charStyle(i);
			formatMatching = isFormatMatching(parStyle, charStyle, options);
			if (formatMatching)
				break;
			matchEnd = i;
		}
		return {-1, 0, matchEnd + 1};
	}

	if (!options.textEnabled)
	{
		// include all contiguous chars with matching format
		for (int i = matchEnd + 1; i < length; ++i)
		{
			const auto& parStyle = storyText.paragraphStyle(i);
			const auto& charStyle = storyText.charStyle(i);
			if (!(formatMatching = isFormatMatching(parStyle, charStyle, options)))
				break;
			matchEnd = i;
		}
	}

	return { matchStart, matchEnd, matchEnd + 1 };
}

QPair<int, int> SearchReplace::searchStory(const StoryText& storyText, const int start, const int length, const Options& options)
{
	if (length == 0)
		return { -1, -1 };
	const QString plainText = options.regularExpression ? storyText.plainText() : QString();

	int matchStart = start;
	int matchEnd = start;
	bool found = false;

	while (!found && matchStart < length)
	{
		// QApplication::processEvents();
		if (!m_searching)
			break;

		const auto match = getMatchRange(storyText, matchStart, length, options, options.regularExpression ? &plainText : nullptr);
		if (match.start == -1)
			matchStart = match.nextPosition;
		else
		{
			matchStart = match.start;
			matchEnd = match.end;
			found = true;
		}
	}

	if (!found)
		return { -1, -1 };

	return { matchStart, matchEnd };
}

void SearchReplace::updatePageItemSelection(PageItem* pageItem)
{
	if (!pageItem)
		return;
	StoryText& storyText = pageItem->itemText;

	int foundPos = storyText.cursorPosition();
	if (storyText.selectionLength() > 0)
		foundPos = storyText.startOfSelection();

	QPointF textCanvasPos;
	if (!m_doc->textCanvasPosition(pageItem, foundPos, textCanvasPos))
		return;

	QRectF updateRect;
	PageItem* textItem = pageItem->frameOfChar(foundPos);
	if (textItem != pageItem)
	{
		updateRect = pageItem->getVisualBoundingRect();
		updateRect = updateRect.united(textItem->getVisualBoundingRect());
		int selLength = pageItem->itemText.selectionLength();
		pageItem->itemText.deselectAll();
		pageItem->HasSel = false;
		m_doc->m_Selection->delaySignalsOn();
		m_doc->m_Selection->removeItem(pageItem);
		m_doc->m_Selection->addItem(textItem);
		pageItem = textItem;
		pageItem->itemText.deselectAll();
		if (selLength > 0)
			pageItem->itemText.select(foundPos, selLength);
		pageItem->itemText.setCursorPosition(foundPos + selLength);
		pageItem->HasSel = true;
		m_doc->m_Selection->delaySignalsOff();
	}
	QRectF visibleCanvasRect = m_doc->view()->visibleCanvasRect();
	if (!visibleCanvasRect.contains(textCanvasPos))
		m_doc->view()->setCanvasCenterPos(textCanvasPos.x(), textCanvasPos.y());
	if (!updateRect.isEmpty())
		m_doc->regionsChanged()->update(updateRect.adjusted(-10.0, -10.0, 10.0, 10.0));
	pageItem->update();
}

bool SearchReplace::isFormatMatching(const ParagraphStyle& parStyle, const CharStyle& charStyle, const Options& options)
{
	if (options.paragraphStyleEnabled && parStyle.parent() != m_doc->paragraphStyles()[options.paragraphStyle].name())
		return false;
	if (options.alignmentEnabled && parStyle.alignment() != options.alignment)
		return false;
	if (options.fontEnabled && charStyle.font().scName() != options.font)
		return false;
	if (options.fontSizeEnabled && charStyle.fontSize() != options.fontSize)
		return false;
	if (options.fontEffectsEnabled && (charStyle.effects() & ScStyle_UserStyles) != options.fontEffects)
		return false;
	if (options.fillColorEnabled && charStyle.fillColor() != options.fillColor)
		return false;
	if (options.fillShadeEnabled && charStyle.fillShade() != options.fillShade)
		return false;
	if (options.strokeColorEnabled && charStyle.strokeColor() != options.strokeColor)
		return false;
	if (options.strokeShadeEnabled && charStyle.strokeShade() != options.strokeShade)
		return false;
	return true;
}

void SearchReplace::slotReplace()
{
	m_searching = true;
	updateButtonState();

	if (m_storyEditorMode)
		replaceInStoryEditor();
	else
		replaceOnCanvas();

	m_searching = false;
	updateButtonState();
}

void SearchReplace::replaceInStoryEditor()
{
	auto options = getSearchOptions();

	SEditor* storyTextEdit = m_doc->scMW()->CurrStED->Editor;
	StoryText& storyText =  storyTextEdit->StyledText;

	int selectionStart = storyTextEdit->textCursor().selectionStart();
	int selectionEnd = storyTextEdit->textCursor().selectionEnd();

	MatchRange match = getMatchRange(storyText, selectionStart, storyText.length(), options);

	if (selectionStart == match.start && selectionEnd - 1 == match.end)
	{
		if (options.regularExpression)
		{
			QString replacement;
			QString error;
			if (!regexReplacementForMatch(storyText, selectionStart, selectionEnd - selectionStart,
				options, &replacement, &error))
			{
				showMessage(error);
				return;
			}
			if (!storyEditorRangeMatches(storyTextEdit, storyText, selectionStart, selectionEnd - selectionStart))
			{
				showMessage(tr("The Story Editor display does not match the story at this position."));
				return;
			}
			QTextCursor cursor = storyTextEdit->textCursor();
			storyTextEdit->beginRegexEdit();
			cursor.beginEditBlock();
			cursor.insertText(replacement);
			cursor.endEditBlock();
			storyTextEdit->endRegexEdit();
			storyTextEdit->setTextCursor(cursor);
		}
		else
			replaceSelectionInStoryEditor();
	}

	searchInStoryEditor();
}

void SearchReplace::replaceOnCanvas()
{
	auto options = getSearchOptions();

	if (!initCanvasSelection())
		return;

	auto pageItem = currentPageItem();
	StoryText& storyText = pageItem->itemText;

	int selectionStart = storyText.startOfSelection();
	int selectionEnd = storyText.endOfSelection();

	if (selectionEnd < 0)
	{
		searchOnCanvas();
		return;
	}

	MatchRange match = getMatchRange(storyText, selectionStart, storyText.length(), options);

	if (selectionStart == match.start && selectionEnd - 1 == match.end)
	{
		if (options.regularExpression)
		{
			QString replacement;
			QString error;
			if (!regexReplacementForMatch(storyText, selectionStart, selectionEnd - selectionStart,
				options, &replacement, &error))
			{
				showMessage(error);
				return;
			}
			replaceSelectionOnCanvas(&replacement);
		}
		else
			replaceSelectionOnCanvas();
		updatePageItemSelection(pageItem);
	}

	searchOnCanvas();
}


void SearchReplace::replaceSelectionInStoryEditor()
{
	StoryEditor* se = m_doc->scMW()->CurrStED;
	auto options = getReplaceOptions();

	// TODO: undo does not work
	if (options.textEnabled)
	{
		disconnect(se->Editor, SIGNAL(cursorPositionChanged()), se, SLOT(updateProps()));
		int SelStart = se->Editor->textCursor().selectionStart();
		int SelEnd = se->Editor->textCursor().selectionEnd();
		se->Editor->textCursor().setPosition(SelStart);
		se->Editor->textCursor().setPosition(SelEnd, QTextCursor::KeepAnchor);
		se->Editor->textCursor().removeSelectedText();
		QString newText = options.text;
		se->Editor->insertPlainText(newText);
		if (newText.length() > 0)
		{
			QTextCursor textCursor = se->Editor->textCursor();
			textCursor.setPosition(SelStart);
			textCursor.setPosition(SelStart + newText.length(), QTextCursor::KeepAnchor);
			se->Editor->setTextCursor(textCursor);
		}
		connect(se->Editor, SIGNAL(cursorPositionChanged()), se, SLOT(updateProps()));
	}
	if (options.paragraphStyleEnabled)
		se->newStyle(m_doc->paragraphStyles()[options.paragraphStyle].name());
	if (options.alignmentEnabled)
		se->newAlign(options.alignment);
	if (options.fillColorEnabled)
		se->newTxFill(options.fillColorId, -1);
	if (options.fillShadeEnabled)
		se->newTxFill(-1, options.fillShade);
	if (options.strokeColorEnabled)
		se->newTxStroke(options.strokeColorId, -1);
	if (options.strokeShadeEnabled)
		se->newTxStroke(-1, options.strokeShade);
	if (options.fontEnabled)
		se->newTxFont(options.font);
	if (options.fontSizeEnabled)
		se->newTxSize(options.fontSize / 10);
	if (options.fontEffectsEnabled)
		se->newTxStyle(options.fontEffects);

	// TODO: not sure that this is needed, since we then jump away...
	QTextCursor textCursor = se->Editor->textCursor();
	int selStart = textCursor.selectionStart();
	int selEnd = textCursor.selectionEnd();
	int selPos = qMax(selStart, selEnd);
	textCursor.setPosition(selPos);
	se->Editor->setTextCursor(textCursor);
}

void SearchReplace::replaceSelectionOnCanvas(const QString* replacementText)
{
	auto options = getReplaceOptions();
	if (replacementText)
	{
		options.textEnabled = true;
		options.text = *replacementText;
	}
	auto pageItem = currentPageItem();
	UndoTransaction transaction;
	if (UndoManager::undoEnabled())
		transaction = UndoManager::instance()->beginTransaction(pageItem->getUName(), pageItem->getUPixmap());

	if (options.textEnabled)
	{
		if (UndoManager::undoEnabled())
		{
			UndoObject* undoTarget = pageItem->isNoteFrame() ? dynamic_cast<UndoObject*>(pageItem->doc()) : dynamic_cast<UndoObject*>(pageItem);
			if (pageItem->itemText.selectionLength() > 0)
			{
				auto is = new ScItemState<CharStyle>(Um::DeleteText, "", Um::IDelete);
				int selStart =  pageItem->itemText.startOfSelection();
				is->set("DELETE_FRAMETEXT");
				is->set("ETEA",  QString("delete_frametext"));
				is->set("TEXT_STR", pageItem->itemText.selectedText());
				is->set("START", pageItem->itemText.startOfSelection());
				is->setItem(pageItem->itemText.charStyle(selStart));
				if (pageItem->isNoteFrame())
					is->set("noteframeName", pageItem->getUName());
				UndoManager::instance()->action(undoTarget, is);
			}
			if (options.text.length() > 0)
			{
				int selStart = qMin(pageItem->itemText.cursorPosition(), pageItem->itemText.startOfSelection());
				auto ss = new SimpleState(Um::InsertText, "", Um::ICreate);
				ss->set("INSERT_FRAMETEXT");
				ss->set("ETEA", QString("insert_frametext"));
				ss->set("TEXT_STR", options.text);
				ss->set("START", selStart);
				UndoObject * undoTarget = pageItem;
				if (pageItem->isNoteFrame())
					ss->set("noteframeName", pageItem->getUName());
				UndoManager::instance()->action(undoTarget, ss);
			}
		}
		pageItem->itemText.replaceSelection(options.text);
	}

	if (options.paragraphStyleEnabled)
	{
		int oldMode = m_doc->appMode;
		m_doc->appMode = modeEdit;
		m_doc->itemSelection_SetNamedParagraphStyle(m_doc->paragraphStyles()[options.paragraphStyle].name());
		m_doc->appMode = oldMode;
	}

	if (options.alignmentEnabled)
	{
		int oldMode = m_doc->appMode;
		m_doc->appMode = modeEdit;
		m_doc->itemSelection_SetAlignment(options.alignment);
		m_doc->appMode = oldMode;
	}

	if (options.fontEnabled)
		m_doc->itemSelection_SetFont(options.font);

	if (options.fontSizeEnabled)
		m_doc->itemSelection_SetFontSize(options.fontSize);

	// TODO: undo does not work
	if (options.fontEffectsEnabled && (pageItem->itemText.hasSelection()))
	{
		int start = pageItem->itemText.startOfSelection();
		int end = pageItem->itemText.endOfSelection();
		int s = options.fontEffects & ScStyle_UserStyles;
		m_doc->currentStyle.charStyle().setFeatures(static_cast<StyleFlag>(s).featureList()); // ???
		for (int i = start; i < end; ++i)
		{
			CharStyle newFeatures;
			newFeatures.setFeatures(static_cast<StyleFlag>(s).featureList());
			pageItem->itemText.applyCharStyle(i, 1, newFeatures);
		}
	}

	if (options.fillColorEnabled)
		m_doc->itemSelection_SetFillColor(options.fillColor);

	if (options.fillShadeEnabled)
		m_doc->itemSelection_SetFillShade(options.fillShade);

	if (options.strokeColorEnabled)
		m_doc->itemSelection_SetStrokeColor(options.strokeColor);

	if (options.strokeShadeEnabled)
		m_doc->itemSelection_SetStrokeShade(options.strokeShade);

	if (transaction)
		transaction.commit();
}

bool SearchReplace::regexReplacementForMatch(const StoryText& storyText, int start, int length,
	const Options& searchOptions, QString* replacement, QString* error) const
{
	if (anyReplaceChecked())
	{
		*error = tr("Regex Replace currently supports text replacement only; clear the Replace Formatting options.");
		return false;
	}
	if (length <= 0 || start < 0 || start + length > storyText.length())
	{
		*error = tr("The selected text is no longer a regular-expression match.");
		return false;
	}

	// The existing canvas undo stores one character style for deleted text.
	// Restrict this first slice to a uniform, ordinary text run so Undo can
	// restore its formatting and cannot delete an anchored object or mark.
	const CharStyle& firstStyle = storyText.charStyle(start);
	for (int i = start; i < start + length; ++i)
	{
		if (storyText.text(i) == SpecialChars::OBJECT || storyText.text(i) == SpecialChars::PARSEP
			|| !storyText.charStyle(i).equiv(firstStyle))
		{
			*error = tr("Regex Replace currently requires one text style within a single paragraph, with no inline objects.");
			return false;
		}
	}

	const auto match = searchOptions.compiledRegex.match(storyText.plainText(), start,
		QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
	if (!match.hasMatch() || match.capturedStart() != start || match.capturedLength() != length)
	{
		*error = tr("The selected text is no longer a regular-expression match.");
		return false;
	}
	return RegexSearch::expandReplacement(replaceTextValue->text(), match, replacement, error);
}

void SearchReplace::slotReplaceAll()
{
	if (regexCheckBox->isChecked() && !searchTextValue->text().isEmpty())
	{
		m_searching = true;
		updateButtonState();
		if (m_storyEditorMode)
			replaceAllInStoryEditorRegex();
		else
			replaceAllOnCanvasRegex();
		m_searching = false;
		updateButtonState();
		return;
	}
	m_searching = true;
	updateButtonState();
	showMessage();

	auto showOccurrences = [this](){showMessage(QString(messageOccurrences).arg(m_matchesFound));};
	QTimer timer;
	connect(&timer, &QTimer::timeout, showOccurrences);
	timer.start(1000);

	if (m_storyEditorMode)
		replaceAllInStoryEditor();
	else
		replaceAllOnCanvas();

	timer.stop();
	showOccurrences();
	m_searching = false;
	updateButtonState();
}

void SearchReplace::replaceAllInStoryEditor()
{
	auto options = getSearchOptions();

	SEditor* storyTextEdit = m_doc->scMW()->CurrStED->Editor;
	StoryText& storyText = storyTextEdit->StyledText;

	bool found = false;
	m_matchesFound = 0;

	MatchRange match = getMatchRange(storyText, 0, storyText.length(), options);

	while (match.start >= 0)
	{
		found = true;

		// QApplication::processEvents();
		if (!m_searching)
			break;

		QTextCursor cursor = storyTextEdit->textCursor();
		cursor.setPosition(match.start);
		cursor.setPosition(match.end + 1, QTextCursor::KeepAnchor);
		storyTextEdit->setTextCursor(cursor);

		++m_matchesFound;
		replaceSelectionInStoryEditor();

		match = getMatchRange(storyText, match.start + 1, storyText.length(), options);
	}

	if (!found)
		showMessage(tr(messageNoMatch));
}

void SearchReplace::replaceAllInStoryEditorRegex()
{
	if (!m_doc || !m_doc->scMW()->CurrStED)
		return;
	SEditor* editor = m_doc->scMW()->CurrStED->Editor;
	const StoryText& storyText = editor->StyledText;
	const auto options = getSearchOptions();
	struct PendingReplacement
	{
		int start;
		int length;
		QString text;
	};
	std::vector<PendingReplacement> pending;
	int from = 0;
	while (from < storyText.length())
	{
		const auto found = searchStory(storyText, from, storyText.length(), options);
		if (found.first < 0)
			break;
		QString replacement;
		QString error;
		const int length = found.second + 1 - found.first;
		if (!regexReplacementForMatch(storyText, found.first, length, options, &replacement, &error))
		{
			showMessage(tr("Regex Replace All made no changes: %1").arg(error));
			return;
		}
		if (!storyEditorRangeMatches(editor, storyText, found.first, length))
		{
			showMessage(tr("Regex Replace All made no changes: the Story Editor display does not match the story at a match position."));
			return;
		}
		pending.push_back({ found.first, length, replacement });
		from = found.second + 1;
	}
	if (pending.empty())
	{
		showMessage(tr(messageNoMatch));
		return;
	}

	// A single QTextDocument edit block makes the complete operation one Story
	// Editor Undo action. The editor's contentsChange hook keeps StyledText in
	// sync as each replacement is applied from the end of the story.
	QTextCursor cursor = editor->textCursor();
	editor->beginRegexEdit();
	cursor.beginEditBlock();
	for (auto it = pending.rbegin(); it != pending.rend(); ++it)
	{
		cursor.setPosition(it->start);
		cursor.setPosition(it->start + it->length, QTextCursor::KeepAnchor);
		cursor.insertText(it->text);
	}
	cursor.endEditBlock();
	editor->endRegexEdit();
	editor->setTextCursor(cursor);
	m_matchesFound = static_cast<int>(pending.size());
	showMessage(QString(tr(messageOccurrences)).arg(m_matchesFound));
}

void SearchReplace::replaceAllOnCanvas()
{
	// TODO: store the current page and go back to it when finished
	// auto currentPage = m_doc->currentPage();
	m_doc->DoDrawing = false;

	auto options = getSearchOptions();

	if (!initCanvasSelection())
		return;

	bool found = false;
	m_matchesFound = 0;
	m_currentPageItem = 0;
	for (auto pageItem: m_pageItems)
	{
		selectPageItem(pageItem);
		// TODO: createcreate  resetEditSelection() ?
		if (m_doc->appMode != modeEdit)
		{
			m_doc->view()->requestMode(modeEdit);
			pageItem->itemText.setCursorPosition(0);
		}

		StoryText& storyText = pageItem->itemText;

		auto pos = searchStory(storyText, 0, storyText.length(), options);

		while (pos.first >= 0)
		{
			// QApplication::processEvents();
			if (!m_searching)
				break;

			found = true;
			++m_matchesFound;

			storyText.select(pos.first, pos.second + 1 - pos.first);
			storyText.setCursorPosition(pos.second + 1);

			replaceSelectionOnCanvas();

			pos = searchStory(storyText, pos.first + 1, storyText.length(), options);
		}

		if (!m_searching)
			break;
		++m_currentPageItem;
	}

	if (!found)
		showMessage(tr(messageNoMatch));

	m_doc->view()->requestMode(modeNormal);

	m_doc->DoDrawing = true;
	// m_doc->setCurrentPage(currentPage);
	// TODO: deselect the current item or select the first item on page
	// updatePageItemSelection(m_pageItems[currentPage]);

	for (auto pageItem: m_pageItems)
		pageItem->asTextFrame()->invalidateLayout(true);
	m_doc->regionsChanged()->update(QRectF());
}

void SearchReplace::replaceAllOnCanvasRegex()
{
	if (!initCanvasSelection())
		return;

	struct PendingReplacement
	{
		size_t pageItemIndex;
		int start;
		int length;
		QString text;
	};
	std::vector<PendingReplacement> pending;
	const auto options = getSearchOptions();
	for (size_t index = 0; index < m_pageItems.size(); ++index)
	{
		const StoryText& storyText = m_pageItems.at(index)->itemText;
		int from = 0;
		while (from < storyText.length())
		{
			const auto found = searchStory(storyText, from, storyText.length(), options);
			if (found.first < 0)
				break;
			QString replacement;
			QString error;
			if (!regexReplacementForMatch(storyText, found.first, found.second + 1 - found.first,
				options, &replacement, &error))
			{
				showMessage(tr("Regex Replace All made no changes: %1").arg(error));
				return;
			}
			pending.push_back({ index, found.first, found.second + 1 - found.first, replacement });
			from = found.second + 1;
		}
	}

	if (pending.empty())
	{
		showMessage(tr(messageNoMatch));
		return;
	}

	// Enter text-edit mode before starting the grouped undo action. The mode
	// transition can change undo availability, which would otherwise leave
	// each replacement as a separate history entry.
	m_currentPageItem = pending.back().pageItemIndex;
	selectPageItem(m_pageItems.at(m_currentPageItem));
	m_doc->view()->requestMode(modeEdit);

	UndoTransaction allTransaction;
	if (UndoManager::undoEnabled())
		allTransaction = UndoManager::instance()->beginTransaction(
			Um::Selection, Um::ITextFrame, tr("Regex Replace All"));
	{
		QScopedValueRollback<bool> drawingGuard(m_doc->DoDrawing, false);
		size_t selectedIndex = m_currentPageItem;
		for (auto it = pending.rbegin(); it != pending.rend(); ++it)
		{
			m_currentPageItem = it->pageItemIndex;
			PageItem* pageItem = m_pageItems.at(it->pageItemIndex);
			if (selectedIndex != it->pageItemIndex)
			{
				selectPageItem(pageItem);
				m_doc->view()->requestMode(modeEdit);
				selectedIndex = it->pageItemIndex;
			}
			pageItem->itemText.select(it->start, it->length);
			pageItem->itemText.setCursorPosition(it->start + it->length);
			replaceSelectionOnCanvas(&it->text);
		}
	}
	if (allTransaction)
		allTransaction.commit();
	m_doc->view()->requestMode(modeNormal);
	m_matchesFound = static_cast<int>(pending.size());
	for (auto pageItem : m_pageItems)
		pageItem->asTextFrame()->invalidateLayout(true);
	m_doc->regionsChanged()->update(QRectF());
	showMessage(QString(tr(messageOccurrences)).arg(m_matchesFound));
}

void SearchReplace::applyRegexCharacterStyle()
{
	if (m_storyEditorMode || !regexCheckBox->isChecked() || searchTextValue->text().isEmpty()
		|| regexStyleComboBox->currentText().isEmpty())
		return;

	const QString styleName = regexStyleComboBox->currentText();
	if (m_doc->charStyles().find(styleName) < 0 || !initCanvasSelection())
		return;

	struct Match
	{
		size_t pageItemIndex;
		int start;
		int length;
	};
	std::vector<Match> matches;
	const auto options = getSearchOptions();
	if (!options.compiledRegex.isValid())
		return;
	QScopedValueRollback<bool> searchingGuard(m_searching, true);
	for (size_t index = 0; index < m_pageItems.size(); ++index)
	{
		const StoryText& storyText = m_pageItems.at(index)->itemText;
		int from = 0;
		while (from < storyText.length())
		{
			const auto found = searchStory(storyText, from, storyText.length(), options);
			if (found.first < 0)
				break;
			const int length = found.second + 1 - found.first;
			for (int i = found.first; i <= found.second; ++i)
			{
				if (storyText.text(i) == SpecialChars::OBJECT || storyText.text(i) == SpecialChars::PARSEP)
				{
					showMessage(tr("No styles applied: matches cannot include an inline object or cross a paragraph boundary."));
					return;
				}
			}
			matches.push_back({ index, found.first, length });
			from = found.second + 1;
		}
	}
	if (matches.empty())
	{
		showMessage(tr(messageNoMatch));
		return;
	}

	// Enter edit mode before the outer transaction; switching modes otherwise
	// can split a multi-match style change into separate Undo actions.
	m_currentPageItem = matches.back().pageItemIndex;
	selectPageItem(m_pageItems.at(m_currentPageItem));
	m_doc->view()->requestMode(modeEdit);
	UndoTransaction allTransaction;
	if (UndoManager::undoEnabled())
		allTransaction = UndoManager::instance()->beginTransaction(
			Um::Selection, Um::ITextFrame, tr("Apply Character Style to Regex Matches"));
	size_t selectedIndex = m_currentPageItem;
	for (auto it = matches.rbegin(); it != matches.rend(); ++it)
	{
		PageItem* pageItem = m_pageItems.at(it->pageItemIndex);
		if (selectedIndex != it->pageItemIndex)
		{
			selectPageItem(pageItem);
			m_doc->view()->requestMode(modeEdit);
			selectedIndex = it->pageItemIndex;
		}
		pageItem->itemText.select(it->start, it->length);
		pageItem->itemText.setCursorPosition(it->start + it->length);
		m_doc->itemSelection_SetNamedCharStyle(styleName);
	}
	if (allTransaction)
		allTransaction.commit();
	m_doc->view()->requestMode(modeNormal);
	m_matchesFound = static_cast<int>(matches.size());
	showMessage(tr("%1 matches styled").arg(m_matchesFound));
}

void SearchReplace::slotSearchPrevious()
{
	m_searching = true;
	updateButtonState();

	if (m_storyEditorMode)
		searchInStoryEditorBackward();
	else
		searchOnCanvasBackward();

	m_searching = false;
	updateButtonState();
}

void SearchReplace::slotCollapseFormat()
{
	m_stateCollapsed = !m_stateCollapsed;
	collapseFormat();
}

void SearchReplace::collapseFormat()
{
	searchGroupBox->setVisible(!m_stateCollapsed);
	replaceGroupBox->setVisible(!m_stateCollapsed);
	// move the following line to setCollapseLabel() if we have to catch languageChange()
	collapseButton->setText(m_stateCollapsed ? tr("Show Formatting") : tr("Hide Formatting"));
	collapseButton->setAccessibleName(collapseButton->text());
	// It's important to call QApplication::processEvents() before calling adjustSize()
	// https://stackoverflow.com/questions/1675499/how-do-i-auto-adjust-the-size-of-a-qdialog-depending-on-the-text-length-of-one-o#comment82074153_1679399
	QTimer::singleShot(0, [this](){adjustSize();});
}

void SearchReplace::enableTextSearch()
{
	bool enabled = !searchTextValue->text().isEmpty();
	wholeWordsCheckBox->setEnabled(enabled);
	ignoreCaseCheckBox->setEnabled(enabled);
	regexCheckBox->setEnabled(enabled);
	ignoreCaseCheckBox->setText(regexCheckBox->isChecked()
		? tr("Ignore Case") : tr("Ignore Case, Diacritics and Kashida"));
}

void SearchReplace::enableStyleSearch()
{
	searchStyleComboBox->setEnabled(searchStyleCheckBox->isChecked());
}

void SearchReplace::enableAlignmentSearch()
{
	searchAlignmentComboBox->setEnabled(searchAlignmentCheckBox->isChecked());
}

void SearchReplace::enableFontSearch()
{
	searchFontComboBox->setEnabled(searchFontCheckBox->isChecked());
}

void SearchReplace::enableFontSizeSearch()
{
	searchFontSizeSpinBox->setEnabled(searchFontSizeCheckBox->isChecked());
}

void SearchReplace::enableFontEffectsSearch()
{
	searchFontEffects->setEnabled(searchFontEffectsCheckBox->isChecked());
}

void SearchReplace::enableFillColorSearch()
{
	searchFillColorComboBox->setEnabled(searchFillColorCheckBox->isChecked());
}

void SearchReplace::enableFillShadeSearch()
{
	searchFillShadeToolButton->setEnabled(searchFillShadeCheckBox->isChecked());
}

void SearchReplace::enableStrokeColorSearch()
{
	searchStrokeColorComboBox->setEnabled(searchStrokeColorCheckBox->isChecked());
}

void SearchReplace::enableStrokeShadeSearch()
{
	searchStrokeShadeToolButton->setEnabled(searchStrokeShadeCheckBox->isChecked());
}

void SearchReplace::enableStyleReplace()
{
	replaceStyleComboBox->setEnabled(replaceStyleCheckBox->isChecked());
}

void SearchReplace::enableAlignmentReplace()
{
	replaceAlignmentComboBox->setEnabled(replaceAlignmentCheckBox->isChecked());
}

void SearchReplace::enableFontReplace()
{
	replaceFontComboBox->setEnabled(replaceFontCheckBox->isChecked());
}

void SearchReplace::enableFontSizeReplace()
{
	replaceFontSizeSpinBox->setEnabled(replaceFontSizeCheckBox->isChecked());
}

void SearchReplace::enableFontEffectsReplace()
{
	replaceFontEffects->setEnabled(replaceFontEffectsCheckBox->isChecked());
}

void SearchReplace::enableFillColorReplace()
{
	replaceFillColorComboBox->setEnabled(replaceFillColorCheckBox->isChecked());
}

void SearchReplace::enableFillShadeReplace()
{
	replaceFillShadeToolButton->setEnabled(replaceFillShadeCheckBox->isChecked());
}

void SearchReplace::enableStrokeColorReplace()
{
	replaceStrokeColorComboBox->setEnabled(replaceStrokeColorCheckBox->isChecked());
}

void SearchReplace::enableStrokeShadeReplace()
{
	replaceStrokeShadeToolButton->setEnabled(replaceStrokeShadeCheckBox->isChecked());
}

void SearchReplace::slotStop()
{
	m_searching = false;
}

void SearchReplace::clear()
{
	replaceAlignmentCheckBox->setChecked(false);
	replaceFillColorCheckBox->setChecked(false);
	replaceFillShadeCheckBox->setChecked(false);
	replaceFontCheckBox->setChecked(false);
	replaceFontEffectsCheckBox->setChecked(false);
	replaceFontSizeCheckBox->setChecked(false);
	replaceStrokeColorCheckBox->setChecked(false);
	replaceStrokeShadeCheckBox->setChecked(false);
	replaceStyleCheckBox->setChecked(false);

	searchAlignmentCheckBox->setChecked(false);
	searchFillColorCheckBox->setChecked(false);
	searchFillShadeCheckBox->setChecked(false);
	searchFontCheckBox->setChecked(false);
	searchFontEffectsCheckBox->setChecked(false);
	searchFontSizeCheckBox->setChecked(false);
	searchStrokeColorCheckBox->setChecked(false);
	searchStrokeShadeCheckBox->setChecked(false);
	searchStyleCheckBox->setChecked(false);

	auto currentParaStyle = findParagraphStyle(m_doc, m_doc->currentStyle);

	searchTextValue->setText("");
	searchStyleComboBox->setCurrentIndex(currentParaStyle);
	searchAlignmentComboBox->setCurrentIndex(m_doc->currentStyle.alignment());
	setCurrentComboItem(searchFontComboBox, m_doc->currentStyle.charStyle().font().scName());
	searchFontSizeSpinBox->setValue(m_doc->currentStyle.charStyle().fontSize() / 10.0);
	setCurrentComboItem(searchFillColorComboBox, m_doc->currentStyle.charStyle().fillColor());
	setCurrentComboItem(searchStrokeColorComboBox, m_doc->currentStyle.charStyle().strokeColor());

	replaceTextValue->setText("");
	replaceStyleComboBox->setCurrentIndex(currentParaStyle);
	replaceAlignmentComboBox->setCurrentIndex(m_doc->currentStyle.alignment());
	setCurrentComboItem(replaceFontComboBox, m_doc->currentStyle.charStyle().font().scName());
	replaceFontSizeSpinBox->setValue(m_doc->currentStyle.charStyle().fontSize() / 10.0);
	setCurrentComboItem(replaceFillColorComboBox, m_doc->currentStyle.charStyle().fillColor());
	setCurrentComboItem(replaceStrokeColorComboBox, m_doc->currentStyle.charStyle().strokeColor());


	ignoreCaseCheckBox->setChecked(false);
	wholeWordsCheckBox->setChecked(false);
	regexCheckBox->setChecked(false);
	
	replaceAlignmentComboBox->setEnabled(false);
	replaceFillColorComboBox->setEnabled(false);
	replaceFillShadeToolButton->setEnabled(false);
	replaceFontComboBox->setEnabled(false);
	replaceFontEffects->setEnabled(false);
	replaceFontSizeSpinBox->setEnabled(false);
	replaceStrokeColorComboBox->setEnabled(false);
	replaceStrokeShadeToolButton->setEnabled(false);
	replaceStyleComboBox->setEnabled(false);

	searchAlignmentComboBox->setEnabled(false);
	searchFillColorComboBox->setEnabled(false);
	searchFillShadeToolButton->setEnabled(false);
	searchFontComboBox->setEnabled(false);
	searchFontEffects->setEnabled(false);
	searchFontSizeSpinBox->setEnabled(false);
	searchStrokeColorComboBox->setEnabled(false);
	searchStrokeShadeToolButton->setEnabled(false);
	searchStyleComboBox->setEnabled(false);

	replaceAllButton->setEnabled(false);
	replaceButton->setEnabled(false);
	searchButton->setEnabled(false);
}

bool SearchReplace::anySearchChecked()
{
	return
		!searchTextValue->text().isEmpty() ||
		searchAlignmentCheckBox->isChecked() ||
		searchFillColorCheckBox->isChecked() ||
		searchFillShadeCheckBox->isChecked() ||
		searchFontCheckBox->isChecked() ||
		searchFontEffectsCheckBox->isChecked() ||
		searchFontSizeCheckBox->isChecked() ||
		searchStrokeColorCheckBox->isChecked() ||
		searchStrokeShadeCheckBox->isChecked() ||
		searchStyleCheckBox->isChecked();
}

bool SearchReplace::anyReplaceChecked() const
{
	return
		replaceAlignmentCheckBox->isChecked() ||
		replaceFillColorCheckBox->isChecked() ||
		replaceFillShadeCheckBox->isChecked() ||
		replaceFontCheckBox->isChecked() ||
		replaceFontEffectsCheckBox->isChecked() ||
		replaceFontSizeCheckBox->isChecked() ||
		replaceStrokeColorCheckBox->isChecked() ||
		replaceStrokeShadeCheckBox->isChecked() ||
		replaceStyleCheckBox->isChecked();
}

SearchReplace::Options SearchReplace::getSearchOptions()
{
	Options options;

	options.alignmentEnabled = searchAlignmentCheckBox->isChecked();
	options.fillColorEnabled = searchFillColorCheckBox->isChecked();
	options.fillShadeEnabled = searchFillShadeCheckBox->isChecked();
	options.fontEffectsEnabled = searchFontEffectsCheckBox->isChecked();
	options.fontEnabled = searchFontCheckBox->isChecked();
	options.fontSizeEnabled = searchFontSizeCheckBox->isChecked();
	options.ignoreCase = ignoreCaseCheckBox->isChecked();
	options.paragraphStyleEnabled = searchStyleCheckBox->isChecked();
	options.regularExpression = regexCheckBox->isChecked() && !searchTextValue->text().isEmpty();
	options.strokeColorEnabled = searchStrokeColorCheckBox->isChecked();
	options.strokeShadeEnabled = searchStrokeShadeCheckBox->isChecked();
	options.textEnabled = !searchTextValue->text().isEmpty();
	options.wholeWords = wholeWordsCheckBox->isChecked();

	if (options.textEnabled)
	{
		options.text = searchTextValue->text();
		if (options.regularExpression)
			options.compiledRegex = RegexSearch::compile(options.text, options.ignoreCase, options.wholeWords);
	}
	if (options.paragraphStyleEnabled)
		options.paragraphStyle = searchStyleComboBox->currentIndex();
	if (options.alignmentEnabled)
		options.alignment = searchAlignmentComboBox->currentIndex();
	if (options.fontEnabled)
		options.font = searchFontComboBox->currentText();
	if (options.fontSizeEnabled)
		options.fontSize = qRound(searchFontSizeSpinBox->value() * 10);
	if (options.fontEffectsEnabled)
		options.fontEffects = searchFontEffects->getStyle();
	if (options.fillColorEnabled)
		options.fillColor = searchFillColorComboBox->currentText();
	if (options.fillShadeEnabled)
		options.fillShade = searchFillShadeToolButton->getValue();
	if (options.strokeColorEnabled)
	{
		options.strokeColor = searchStrokeColorComboBox->currentText();
	}
	if (options.strokeShadeEnabled)
		options.strokeShade = searchStrokeShadeToolButton->getValue();

	return options;
}

SearchReplace::Options SearchReplace::getReplaceOptions()
{
	Options options;

	// replace if there is a text or if no format is selected
	options.alignmentEnabled = replaceAlignmentCheckBox->isChecked();
	options.fillColorEnabled = replaceFillColorCheckBox->isChecked();
	options.fillShadeEnabled = replaceFillShadeCheckBox->isChecked();
	options.fontEffectsEnabled = replaceFontEffectsCheckBox->isChecked();
	options.fontEnabled = replaceFontCheckBox->isChecked();
	options.fontSizeEnabled = replaceFontSizeCheckBox->isChecked();
	options.paragraphStyleEnabled = replaceStyleCheckBox->isChecked();
	options.strokeColorEnabled = replaceStrokeColorCheckBox->isChecked();
	options.strokeShadeEnabled = replaceStrokeShadeCheckBox->isChecked();
	options.textEnabled = !replaceTextValue->text().isEmpty() || !anyReplaceChecked();

	if (options.textEnabled)
		options.text = replaceTextValue->text();
	if (options.paragraphStyleEnabled)
		options.paragraphStyle = replaceStyleComboBox->currentIndex();
	if (options.alignmentEnabled)
		options.alignment = replaceAlignmentComboBox->currentIndex();
	if (replaceFontCheckBox->isChecked())
		options.font = replaceFontComboBox->currentText();
	if (options.fontSizeEnabled)
		options.fontSize = qRound(replaceFontSizeSpinBox->value() * 10);
	if (options.fontEffectsEnabled)
		options.fontEffects = replaceFontEffects->getStyle();
	if (options.fillColorEnabled)
	{
		options.fillColor = replaceFillColorComboBox->currentText();
		options.fillColorId = searchFillColorComboBox->currentIndex();
	}
	if (options.fillShadeEnabled)
		options.fillShade = replaceFillShadeToolButton->getValue();
	if (options.strokeColorEnabled)
	{
		options.strokeColor = replaceStrokeColorComboBox->currentText();
		options.strokeColorId = searchStrokeColorComboBox->currentIndex();
	}
	if (options.strokeShadeEnabled)
		options.strokeShade = replaceStrokeShadeToolButton->getValue();


	return options;
}

void SearchReplace::updateButtonState()
{
	replaceAllButton->setVisible(!m_searching);
	replaceButton->setVisible(!m_searching);
	searchButton->setVisible(!m_searching);
	stopButton->setVisible(m_searching);
	previousButton->setVisible(!m_searching);

	bool enabled = anySearchChecked();
	const bool regexMode = regexCheckBox->isChecked() && !searchTextValue->text().isEmpty();
	const bool showRegexStyle = regexCheckBox->isChecked() && !m_storyEditorMode;
	regexStyleLabel->setVisible(showRegexStyle);
	regexStyleComboBox->setVisible(showRegexStyle);
	applyRegexStyleButton->setVisible(showRegexStyle && !m_searching);
	if (regexMode)
	{
		const auto expression = RegexSearch::compile(searchTextValue->text(), ignoreCaseCheckBox->isChecked(), wholeWordsCheckBox->isChecked());
		if (!expression.isValid())
		{
			showMessage(tr("Invalid regular expression: %1").arg(expression.errorString()));
			enabled = false;
		}
	}

	replaceAllButton->setEnabled(enabled);
	replaceButton->setEnabled(enabled);
	searchButton->setEnabled(enabled);
	previousButton->setEnabled(enabled);
	applyRegexStyleButton->setEnabled(enabled && regexMode && regexStyleComboBox->count() > 0);
}

void SearchReplace::hideMessage()
{
	messageLabel->setVisible(false);
}

void SearchReplace::showMessage(const QString& message)
{
	messageLabel->setText(message);
	messageLabel->setVisible(true);
}

void SearchReplace::readPrefs()
{
	m_stateCollapsed = m_prefs->getBool("collapsed", false);

	// QRect screen = QApplication::primaryScreen()->availableGeometry();
	// int left = m_prefs->getInt("left", screen.width() > width() ? (screen.width() - width()) / 2 : 0);
	// int top = m_prefs->getInt("top", screen.height() > height() ? (screen.height() - height()) / 2 : 0);
	// ensure that it's in the screen
	// if (left + width() > screen.width())
	// 	left = screen.width() - left;
	// if (left < 0)
	// 	left = 0;
	// move(left, top);

	replaceAlignmentCheckBox->setChecked(m_prefs->getBool("replaceAlignment", false));
	replaceFillColorCheckBox->setChecked(m_prefs->getBool("replaceFillColor", false));
	replaceFillShadeCheckBox->setChecked(m_prefs->getBool("replaceFillColorShade", false));
	replaceFontCheckBox->setChecked(m_prefs->getBool("replaceFont", false));
	replaceFontEffectsCheckBox->setChecked(m_prefs->getBool("replaceFontEffects", false));
	replaceFontSizeCheckBox->setChecked(m_prefs->getBool("replaceFontSize", false));
	replaceFontSizeSpinBox->setValue(m_prefs->getDouble("replaceFontSizeValue", m_doc->currentStyle.charStyle().fontSize() / 10.0));
	replaceStrokeColorCheckBox->setChecked(m_prefs->getBool("replaceStrokeColor", false));
	replaceStrokeShadeCheckBox->setChecked(m_prefs->getBool("replaceStrokeColorShade", false));
	replaceStyleCheckBox->setChecked(m_prefs->getBool("replaceParagraphStyle", false));
	replaceTextValue->setText(m_prefs->get("replaceTextValue", ""));
	searchAlignmentCheckBox->setChecked(m_prefs->getBool("searchAlignment", false));
	searchFillColorCheckBox->setChecked(m_prefs->getBool("searchFillColor", false));
	searchFillShadeCheckBox->setChecked(m_prefs->getBool("searchFillColorShade", false));
	searchFontCheckBox->setChecked(m_prefs->getBool("searchFont", false));
	searchFontEffectsCheckBox->setChecked(m_prefs->getBool("searchFontEffects", false));
	searchFontSizeCheckBox->setChecked(m_prefs->getBool("searchFontSize", false));
	searchFontSizeSpinBox->setValue(m_prefs->getDouble("searchFontSizeValue", m_doc->currentStyle.charStyle().fontSize() / 10.0));
	searchStrokeColorCheckBox->setChecked(m_prefs->getBool("searchStrokeColor", false));
	searchStrokeShadeCheckBox->setChecked(m_prefs->getBool("searchStrokeColorShade", false));
	searchStyleCheckBox->setChecked(m_prefs->getBool("searchParagraphStyle", false));
	searchTextValue->setText(m_prefs->get("searchTextValue", ""));

	int tmp = m_prefs->getInt("replaceStyleValue", findParagraphStyle(m_doc, m_doc->currentStyle));
	if (tmp < 0 || tmp >= replaceStyleComboBox->count())
		tmp = 0;
	replaceStyleComboBox->setCurrentIndex(tmp);

	tmp = m_prefs->getInt("replaceAlignmentValue", m_doc->currentStyle.alignment());
	if (tmp < 0 || tmp >= replaceAlignmentComboBox->count())
		tmp = 0;
	replaceAlignmentComboBox->setCurrentIndex(tmp);

	tmp = m_prefs->getInt("searchParagraphStyleValue", findParagraphStyle(m_doc, m_doc->currentStyle));
	if (tmp < 0 || tmp >= searchStyleComboBox->count())
		tmp = 0;
	searchStyleComboBox->setCurrentIndex(tmp);

	tmp = m_prefs->getInt("searchAlignmentValue", m_doc->currentStyle.alignment());
	if (tmp < 0 || tmp >= searchAlignmentComboBox->count())
		tmp = 0;
	searchAlignmentComboBox->setCurrentIndex(tmp);

	setCurrentComboItem(replaceFillColorComboBox, m_prefs->get("replaceFillColorValue", m_doc->currentStyle.charStyle().fillColor()));
	setCurrentComboItem(replaceFontComboBox, m_prefs->get("replaceFontValue", m_doc->currentStyle.charStyle().font().scName()));
	setCurrentComboItem(replaceStrokeColorComboBox, m_prefs->get("replaceStrokeColorValue", m_doc->currentStyle.charStyle().strokeColor()));
	setCurrentComboItem(searchFillColorComboBox, m_prefs->get("searchFillColorValue", m_doc->currentStyle.charStyle().fillColor()));
	setCurrentComboItem(searchFontComboBox, m_prefs->get("searchFontValue", m_doc->currentStyle.charStyle().font().scName()));
	setCurrentComboItem(searchStrokeColorComboBox, m_prefs->get("searchStrokeColorValue", m_doc->currentStyle.charStyle().strokeColor()));

	ignoreCaseCheckBox->setChecked(m_prefs->getBool("ignoreCase", false));
	wholeWordsCheckBox->setChecked(m_prefs->getBool("wholeword", false));
	regexCheckBox->setChecked(m_prefs->getBool("regularExpression", false));

	enableAlignmentReplace();
	enableAlignmentSearch();
	enableFillColorReplace();
	enableFillColorSearch();
	enableFillShadeReplace();
	enableFillShadeSearch();
	enableFontEffectsReplace();
	enableFontEffectsSearch();
	enableFontReplace();
	enableFontSearch();
	enableFontSizeReplace();
	enableFontSizeSearch();
	enableStrokeColorReplace();
	enableStrokeColorSearch();
	enableStrokeShadeReplace();
	enableStrokeShadeSearch();
	enableStyleReplace();
	enableStyleSearch();
	enableTextSearch();

	updateButtonState();

	searchTextValue->setFocus();
}

void SearchReplace::accept()
{
	writePrefs();
	QDialog::accept();
}

void SearchReplace::writePrefs()
{
	m_prefs->set("collapsed", m_stateCollapsed);
	m_prefs->set("left", pos().x());
	m_prefs->set("top", pos().y());
	m_prefs->set("replaceAlignment", replaceAlignmentCheckBox->isChecked());
	m_prefs->set("replaceAlignmentValue", replaceAlignmentComboBox->currentIndex());
	m_prefs->set("replaceFillColor", replaceFillColorCheckBox->isChecked());
	m_prefs->set("replaceFillColorShade", replaceFillShadeCheckBox->isChecked());
	m_prefs->set("replaceFillColorValue", replaceFillColorComboBox->currentText());
	m_prefs->set("replaceFont", replaceFontCheckBox->isChecked());
	m_prefs->set("replaceFontEffects", replaceFontEffectsCheckBox->isChecked());
	m_prefs->set("replaceFontSize", replaceFontSizeCheckBox->isChecked());
	m_prefs->set("replaceFontSizeValue", replaceFontSizeSpinBox->value());
	m_prefs->set("replaceFontValue", replaceFontComboBox->currentText());
	m_prefs->set("replaceParagraphStyle", replaceStyleCheckBox->isChecked());
	m_prefs->set("replaceStrokeColor", replaceStrokeColorCheckBox->isChecked());
	m_prefs->set("replaceStrokeColorValue", replaceStrokeColorComboBox->currentText());
	m_prefs->set("replaceStrokeColorShade", replaceStrokeShadeCheckBox->isChecked());
	m_prefs->set("replaceStyleValue", replaceStyleComboBox->currentIndex());
	m_prefs->set("replaceTextValue", replaceTextValue->text());
	m_prefs->set("searchAlignment", searchAlignmentCheckBox->isChecked());
	m_prefs->set("searchAlignmentValue", searchAlignmentComboBox->currentIndex());
	m_prefs->set("searchFillColor", searchFillColorCheckBox->isChecked());
	m_prefs->set("searchFillColorShade", searchFillShadeCheckBox->isChecked());
	m_prefs->set("searchFillColorValue", searchFillColorComboBox->currentText());
	m_prefs->set("searchFont", searchFontCheckBox->isChecked());
	m_prefs->set("searchFontEffects", searchFontEffectsCheckBox->isChecked());
	m_prefs->set("searchFontSize", searchFontSizeCheckBox->isChecked());
	m_prefs->set("searchFontSizeValue", searchFontSizeSpinBox->value());
	m_prefs->set("searchFontValue", searchFontComboBox->currentText());
	m_prefs->set("searchParagraphStyle", searchStyleCheckBox->isChecked());
	m_prefs->set("searchParagraphStyleValue", searchStyleComboBox->currentIndex());
	m_prefs->set("searchStrokeColor", searchStrokeColorCheckBox->isChecked());
	m_prefs->set("searchStrokeColorValue", searchStrokeColorComboBox->currentText());
	m_prefs->set("searchStrokeColorShade", searchStrokeShadeCheckBox->isChecked());
	m_prefs->set("searchTextValue", searchTextValue->text());
	m_prefs->set("ignoreCase", ignoreCaseCheckBox->isChecked());
	m_prefs->set("wholeword", wholeWordsCheckBox->isChecked());
	m_prefs->set("regularExpression", regexCheckBox->isChecked());
}
