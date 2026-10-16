#include "stacked_container.h"
#include <QStackedLayout>
#include <QEvent>


StackedContainer::StackedContainer(QWidget *parent)
	: QStackedWidget( parent)
{
	// Only the current page contributes to this container's size. Otherwise
	// QStackedLayout can impose the largest hidden inspector page's minimum.
	layout()->setSizeConstraint(QLayout::SetNoConstraint);
	connect(this, &QStackedWidget::currentChanged, this, &StackedContainer::updateSize);
}

QSize StackedContainer::minimumSizeHint() const
{
	const QWidget* page = currentWidget();
	return page ? page->minimumSizeHint().expandedTo(page->minimumSize()) : QSize(0, 0);
}

QSize StackedContainer::sizeHint() const
{
	const QWidget* page = currentWidget();
	return page ? page->sizeHint().expandedTo(minimumSizeHint()) : QSize(0, 0);
}

void StackedContainer::updateSize()
{
	// Ask the containing dock/scroll area to relayout; do not resize it out
	// from under a user who deliberately chose a compact inspector.
	updateGeometry();
}

bool StackedContainer::event(QEvent* event)
{
	const bool handled = QStackedWidget::event(event);
	if (event->type() == QEvent::LayoutRequest)
		updateGeometry();
	return handled;
}

