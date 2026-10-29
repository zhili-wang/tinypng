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
	QTableView::dragEnterEvent(event);

	if (!checkMimeIsDir(event->mimeData())) {
		return;
	}
	overlay->show();
	event->acceptProposedAction();
}


void MyTableView::dragMoveEvent(QDragMoveEvent* event) {
	QTableView::dragMoveEvent(event);

	if (!checkMimeIsDir(event->mimeData())) {
		return;
	}
	event->acceptProposedAction();
}

void MyTableView::dragLeaveEvent(QDragLeaveEvent* event) {
	overlay->hide();
}

void MyTableView::dropEvent(QDropEvent* event) {
	QTableView::dropEvent(event);
	const QMimeData* mimedata = event->mimeData();
	if (!mimedata->hasUrls()) {
		return;
	}
	QList<QUrl> urls = mimedata->urls();
	QStringList files = QStringList();
	for (int i = 0; i < urls.count(); i++) {
		QString path = urls.at(i).toLocalFile();
		QFileInfo file(path);
		if (file.isFile() && !Utility::checkFileFormat(path)) {
			continue;
		}
		files << path;
	}

	overlay->hide();
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
    const QPoint center = viewport()->rect().center();
    const QRect icon(center.x() - 28, center.y() - 68, 56, 56);
    painter.setBrush(QColor("#edf6f0"));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(icon, 16, 16);
    painter.setPen(QPen(QColor("#5b9f76"), 2));
    painter.drawRoundedRect(icon.adjusted(16, 12, -16, -12), 3, 3);
    painter.drawLine(center.x() - 7, center.y() - 47, center.x() + 7, center.y() - 47);
    painter.drawLine(center.x() - 7, center.y() - 40, center.x() + 4, center.y() - 40);
    QFont title = font();
    title.setPixelSize(17);
    painter.setFont(title);
    painter.setPen(QColor("#4c6a58"));
    painter.drawText(QRect(0, center.y() + 1, viewport()->width(), 30), Qt::AlignCenter, "将图片或文件夹拖到这里");
    title.setPixelSize(12);
    painter.setFont(title);
    painter.setPen(QColor("#8a9c90"));
    painter.drawText(QRect(0, center.y() + 38, viewport()->width(), 25), Qt::AlignCenter, "支持 PNG、JPEG、WebP、SVG · 或点击上方添加图片");
}

bool MyTableView::checkMimeIsDir(const QMimeData* mimedata) {

	if (mimedata->hasUrls() == false) {
		return false;
	}

	if (mimedata->urls().length() == 0) {
		return false;
	}
	int dirNum = 0;
	int fileNum = 0;
	for (int i = 0; i < mimedata->urls().length(); i++) {
		QString path = mimedata->urls().at(i).toLocalFile();

		if (QDir(path).exists()) {
			dirNum += 1;
		}
		else if (Utility::checkFileFormat(path)) {
			fileNum += 1;
		}
	}
	//不能是多个文件夹 不能文件夹和文件混合  
	if (dirNum > 1 || (dirNum > 0 && fileNum > 0) || (fileNum + dirNum) == 0) {
		return false;
	}
	return true;
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
