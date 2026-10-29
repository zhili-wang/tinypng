#include "MyTableView.h"
#include <QHeaderView>
#include <QDebug>
#include <QMimeData>
#include <QDir>
#include <QMenu>
#include <QDesktopServices>
#include <QApplication>
#include <QPainter>
#include <QFileInfo>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include "Utility.h"
#include "QueueAppearance.h"
#include "Config.h"

MyTableView::MyTableView(QWidget* parent, QAbstractItemModel* model) :QTableView(parent)
{

	init();
	this->setModel(model);
	this->setGridHeaderview();
	overlay = new MyOverlay(this);




}

void MyTableView::init() {
    processingLocked = false;
    connect(this, &QWidget::customContextMenuRequested, this, &MyTableView::customContextMenuResposne);
    connect(this, &QTableView::doubleClicked, this, &MyTableView::onDoubleClickRow);
}

void MyTableView::setProcessingLocked(bool locked) { processingLocked = locked; }

void MyTableView::setModel(QAbstractItemModel* m) {
	QTableView::setModel(m);
}


void MyTableView::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls() || event->mimeData()->urls().isEmpty()) {
        event->ignore();
        return;
    }
    overlay->show();
    event->setDropAction(Qt::CopyAction);
    event->accept();
}


void MyTableView::dragMoveEvent(QDragMoveEvent* event) {
    if (!event->mimeData()->hasUrls() || event->mimeData()->urls().isEmpty()) {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

void MyTableView::dragLeaveEvent(QDragLeaveEvent* event) {
	overlay->hide();
    event->accept();
}

void MyTableView::dropEvent(QDropEvent* event) {
    overlay->hide();
    if (!event->mimeData()->hasUrls() || event->mimeData()->urls().isEmpty()) {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    QStringList files;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
            emit importRejected("请拖入本地图片或文件夹；网页链接需要先下载到电脑。");
            return;
        }
        // Let the scanner report rejected formats and sizes instead of hiding them.
        files.append(url.toLocalFile());
    }
    emit readResource(files);
}


void MyTableView::keyPressEvent(QKeyEvent* event) {
	if (event->key() == Qt::Key_Delete) {
		this->clickDelete();
	}
	else if (event->matches(QKeySequence::SelectAll)) {
		this->selectAll();
	}
    else QTableView::keyPressEvent(event);
}

void MyTableView::openSelectedRowFolder(int flag) {
	int rowIdx = this->selectionModel()->selectedRows()[0].row();
	TableModel* model = (TableModel*)this->model();
	TableModelRow row = model->getRow(rowIdx);

	QString dir = "";
	if (flag == 1) { //输出目录
		dir = QFileInfo(row.root + "_tiny" + row.path).absoluteDir().absolutePath();
	}
	else { //源目录
		dir = QFileInfo(row.root + row.path).absoluteDir().absolutePath();
	}

	QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MyTableView::setGridHeaderview() { QueueAppearance::configure(this); }

void MyTableView::paintEvent(QPaintEvent* event) {
    QTableView::paintEvent(event);
    if (model()->rowCount() != 0 || !overlay->isHidden()) return;
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect area = viewport()->rect();
    const bool showIcon = area.height() >= 150;
    const bool showSubtitle = area.height() >= 90;
    const int blockHeight = (showIcon ? 68 : 0) + 30 + (showSubtitle ? 31 : 0);
    const int top = qMax(0, (area.height() - blockHeight) / 2);
    if (showIcon) {
        const QRect icon(area.center().x() - 28, top, 56, 56);
        painter.setBrush(Theme::instance()->color("#edf6f0", "#2b4534"));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(icon, 16, 16);
        painter.setPen(QPen(Theme::instance()->color("#5b9f76", "#9dd8b1"), 2));
        painter.drawRoundedRect(icon.adjusted(16, 12, -16, -12), 3, 3);
        painter.drawLine(icon.left() + 21, icon.top() + 21, icon.left() + 35, icon.top() + 21);
        painter.drawLine(icon.left() + 21, icon.top() + 28, icon.left() + 32, icon.top() + 28);
    }
    QFont title = font();
    title.setPixelSize(17);
    painter.setFont(title);
    painter.setPen(Theme::instance()->color("#4c6a58", "#c8dfd0"));
    const int textTop = top + (showIcon ? 68 : 0);
    painter.drawText(QRect(0, textTop, area.width(), 30), Qt::AlignCenter, "将图片或文件夹拖到这里");
    if (showSubtitle) {
        title.setPixelSize(12);
        painter.setFont(title);
        painter.setPen(Theme::instance()->color("#8a9c90", "#a5bfaf"));
        painter.drawText(QRect(0, textTop + 36, area.width(), 25), Qt::AlignCenter, "支持 PNG、JPG/JPEG、WebP、SVG · 或点击上方添加图片");
    }
}
void MyTableView::resizeEvent(QResizeEvent* event) {
	QTableView::resizeEvent(event);

	QPoint pos = this->mapTo(this, QPoint(0, 0));
	overlay->setGeometry(pos.x(), pos.y(), this->width(), this->height());
}

void MyTableView::onDoubleClickRow(const QModelIndex& index) {
	TableModel* model = (TableModel*)this->selectionModel()->model();
	TableModelRow row = model->getRow(index.row());
	QString path = row.root + row.path;
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MyTableView::customContextMenuResposne(const QPoint& pos) {
	int num = this->selectionModel()->selectedRows().length();
	if (num == 0) {
		return;
	}

	int rowIdx = this->selectionModel()->selectedRows()[0].row();
	TableModel* model = (TableModel*)this->model();
	TableModelRow row = model->getRow(rowIdx);

	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);
	QAction* action = new QAction("移除所选文件", menu);
    action->setEnabled(!processingLocked);
	menu->addAction(action);
	menu->addAction("打开源目录", [&]() {
		this->openSelectedRowFolder(0);
		});
	menu->addAction("打开输出目录", [&]() {
		this->openSelectedRowFolder(1);
		});
	if (num > 1) {
		menu->actions().at(1)->setDisabled(true);
		menu->actions().at(2)->setDisabled(true);
	}
	else if (row.root.isEmpty() || Config(this).get().outputMode == OutputMode::Replace || row.status != 2) {
		menu->actions().at(2)->setDisabled(true);
	}

	menu->popup(this->viewport()->mapToGlobal(pos), NULL);
	connect(action, SIGNAL(triggered()), this, SLOT(clickDelete()));

}




void MyTableView::clickDelete() {
    if (processingLocked) return;
	QModelIndexList selectedRows = this->selectionModel()->selectedRows();
	if (selectedRows.length() == 0) {
		return;
	}
	TableModel* model = (TableModel*)this->model();

	if (selectedRows.length() == this->model()->rowCount()) {
		model->removeAll();
		return;
	}
	else if (selectedRows.first().row() + selectedRows.length() == (selectedRows.last().row() + 1)) {
		model->removeRange(selectedRows.first().row(), selectedRows.last().row());
	}
	else {
		QList<QPersistentModelIndex> indexs;
		for (int i = 0; i < selectedRows.length(); i++) {
			indexs << QPersistentModelIndex(selectedRows.at(i));
		}
		QListIterator<QPersistentModelIndex> i(indexs);
		while (i.hasNext()) {
			QPersistentModelIndex item = i.next();
			model->removeRow(item.row());
		}

		indexs.clear();
	}


}



MyTableView::~MyTableView()
{

}
