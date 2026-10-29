#pragma once
#include <QTableView>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QPainter>
#include <QStyle>
#include "Theme.h"

namespace QueueAppearance {
class StatusDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem background(option);
        initStyleOption(&background, index);
        background.text.clear();
        const QWidget* widget = option.widget;
        (widget ? widget->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &background, painter, widget);
        const int status = index.data(Qt::UserRole + 1).toInt();
        const QColor foreground = status == -1 ? Theme::instance()->color("#b55b48", "#efb09b") : status == 2 ? Theme::instance()->color("#26845a", "#a0dbb9") : Theme::instance()->color("#718278", "#b3c8ba");
        const QColor fill = status == -1 ? Theme::instance()->color("#fff0e9", "#4a3028") : status == 2 ? Theme::instance()->color("#e7f4ec", "#2d4a38") : Theme::instance()->color("#eef2ef", "#33463a");
        QRect badge(option.rect.center().x() - 35, option.rect.center().y() - 13, 70, 26);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(badge, 13, 13);
        painter->setPen(foreground);
        painter->drawText(badge, Qt::AlignCenter, index.data().toString());
        painter->restore();
    }
};

inline void applyTheme(QTableView* view) {
    view->setStyleSheet(Theme::instance()->css(R"(
QTableView { background: white; alternate-background-color: #f8fbf9; border: 0; color: #3a5145; selection-background-color: #e5f2e9; selection-color: #203b30; }
QTableView::item { border: 0; border-bottom: 1px solid #f0f4f1; padding: 8px; }
QTableView::item:selected { background: #e5f2e9; color: #203b30; }
QHeaderView::section { background: #f3f7f4; color: #73877a; border: 0; padding: 10px; font-weight: 500; }
QTableView { outline: 0; }
)"));
}

inline void configure(QTableView* view) {
    applyTheme(view);
    view->setShowGrid(false);
    view->setAlternatingRowColors(true);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setAcceptDrops(true);
    view->setDragDropOverwriteMode(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->verticalHeader()->hide();
    view->verticalHeader()->setDefaultSectionSize(48);
    view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    QHeaderView* header = new QHeaderView(Qt::Horizontal, view);
    view->setHorizontalHeader(header);
    header->setHighlightSections(false);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < view->model()->columnCount(); ++column) {
        header->setSectionResizeMode(column, QHeaderView::Fixed);
        view->setColumnWidth(column, 110);
    }
    view->setColumnHidden(0, false);
    view->setItemDelegateForColumn(4, new StatusDelegate(view));
}

}
