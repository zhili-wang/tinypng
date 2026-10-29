#if defined(_MSC_VER) && _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif
#include "MainWindow.h"
#include "SettingWindow.h"
#include "Utility.h"
#include "WorkspaceUi.h"
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFrame>
#include <QSplitter>
#include <QDesktopServices>
#include <QStatusBar>
#include <QFontDatabase>


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), dispatcher(nullptr), scanner(nullptr) {
    QApplication::setStyle("Fusion");
    QFont font = QApplication::font();
    font.setPointSize(10);
    QApplication::setFont(font);
    setStyleSheet(WorkspaceAppearance::theme());
    setWindowTitle("TinyPNG · 图片压缩");
    setWindowIcon(QIcon(":/logo.ico"));
    setMinimumSize(840, 740);
    resize(1080, 800);
    buildInterface();
    dispatcher = new Dispatcher(console, table, this);
    connect(table, &MyTableView::readResource, this, &MainWindow::createScannner);
    connect(dispatcher, &Dispatcher::started, this, [this]() {
        stateLabel->setText("正在压缩");
        refreshActions();
    });
    connect(dispatcher, &Dispatcher::completed, this, [this]() {
        stateLabel->setText("就绪");
        refreshSummary();
        refreshActions();
    });
    connect(dispatcher, &Dispatcher::progressChanged, this, [this](int done, int total, int failures) {
        progress->setRange(0, total);
        progress->setValue(done);
        progressLabel->setText(QString("%1 / %2 已处理 · %3 个失败").arg(done).arg(total).arg(failures));
        refreshSummary();
    });
    connect(table->model(), &QAbstractItemModel::rowsInserted, this, [this]() { refreshSummary(); });
    connect(table->model(), &QAbstractItemModel::rowsRemoved, this, [this]() { refreshSummary(); refreshActions(); });
    refreshSummary();
    refreshActions();
}
MainWindow::~MainWindow() {
    if (scanner) scanner->stop();
    dispatcher->quit();
}

void MainWindow::buildInterface() {
    table = new MyTableView(this, new TableModel(this));
    console = new Console(this);
    WorkspaceUi ui;
    ui.setup(this, table, console);
    layout = ui.layout;
    addbtn = ui.addbtn;
    folderbtn = ui.folderbtn;
    startbtn = ui.startbtn;
    pausebtn = ui.pausebtn;
    retrybtn = ui.retrybtn;
    settingsbtn = ui.settingsbtn;
    countLabel = ui.countLabel;
    successLabel = ui.successLabel;
    failedLabel = ui.failedLabel;
    savedLabel = ui.savedLabel;
    stateLabel = ui.stateLabel;
    progressLabel = ui.progressLabel;
    progress = ui.progress;
    connect(addbtn, &QPushButton::clicked, this, &MainWindow::onClickEditBtn);
    connect(folderbtn, &QPushButton::clicked, this, &MainWindow::onClickFolderBtn);
    connect(startbtn, &QPushButton::clicked, this, &MainWindow::onClickStartBtn);
    connect(pausebtn, &QPushButton::clicked, this, &MainWindow::onClickPauseBtn);
    connect(retrybtn, &QPushButton::clicked, this, &MainWindow::onClickRetryBtn);
    connect(settingsbtn, &QPushButton::clicked, this, &MainWindow::onOpenSettingWindow);
}

void MainWindow::refreshSummary() {
    TableModel* model = static_cast<TableModel*>(table->model());
    int success = 0, failed = 0;
    qint64 saved = 0;
    for (int i = 0; i < model->rowCount(); ++i) {
        const TableModelRow row = model->getRow(i);
        if (row.status == 2) { ++success; saved += row.size - row.thin_size; }
        if (row.status == -1) ++failed;
    }
    countLabel->setText(QString::number(model->rowCount()));
    successLabel->setText(QString::number(success));
    failedLabel->setText(QString::number(failed));
    savedLabel->setText(QLocale().formattedDataSize(saved, 1));
    retrybtn->setText(failed ? QString("重试失败项 (%1)").arg(failed) : "重试失败项");
}
void MainWindow::refreshActions() {
    const bool scanning = scanner && scanner->isRunning();
    const bool running = dispatcher && dispatcher->isRunning();
    bool canStart = false, canRetry = false;
    TableModel* model = static_cast<TableModel*>(table->model());
    for (int i = 0; i < model->rowCount(); ++i) {
        canStart |= model->getRow(i).needsCompression();
        canRetry |= model->getRow(i).needsCompression(true);
    }
    addbtn->setEnabled(!scanning && !running);
    folderbtn->setEnabled(!scanning && !running);
    settingsbtn->setEnabled(!scanning && !running);
    startbtn->setVisible(!running);
    startbtn->setEnabled(!scanning && canStart);
    pausebtn->setVisible(running);
    retrybtn->setEnabled(!scanning && !running && canRetry);
}
void MainWindow::onClickEditBtn() {
    createScannner(QFileDialog::getOpenFileNames(this, "选择图片", "", "图片 (" + Utility::getFormatList().join(" ") + ")"));
}
void MainWindow::onClickFolderBtn() {
    const QString folder = QFileDialog::getExistingDirectory(this, "选择图片文件夹");
    if (!folder.isEmpty()) createScannner(QStringList() << folder);
}
void MainWindow::onClickStartBtn() { if (!scanner || !scanner->isRunning()) dispatcher->run(); }
void MainWindow::onClickRetryBtn() { if (!scanner || !scanner->isRunning()) dispatcher->run(true); }
void MainWindow::onClickPauseBtn() {
    if (!dispatcher->isRunning()) return;
    dispatcher->quit();
    console->tip("已暂停，未完成的文件可继续压缩");
    progressLabel->setText("已暂停 · 点击开始压缩继续");
}
void MainWindow::onOpenSettingWindow() { (new SettingWindow(this))->show(); }
void MainWindow::createScannner(const QStringList& files) {
    if (files.isEmpty() || (scanner && scanner->isRunning())) return;
    onClickPauseBtn();
    if (scanner) { delete scanner; scanner = nullptr; }
    TableModel* model = static_cast<TableModel*>(table->model());
    model->removeAll();
    scanner = new Scanner(this);
    connect(scanner, &Scanner::rowFound, model, [model](const TableModelRow& row) { model->addRow(row); }, Qt::QueuedConnection);
    connect(scanner, &QThread::finished, this, [this]() {
        table->setProcessingLocked(false);
        stateLabel->setText("就绪");
        progress->setRange(0, qMax(1, table->model()->rowCount()));
        progress->setValue(0);
        progressLabel->setText(QString("%1 个文件待压缩").arg(table->model()->rowCount()));
        refreshSummary();
        refreshActions();
        if (table->model()->rowCount() == 0) console->tip("没有找到符合格式和大小设置的图片");
        else if (Config(this).get().autoStart) onClickStartBtn();
    });
    const configItem config = Config(this).get();
    table->setProcessingLocked(true);
    stateLabel->setText("正在读取图片");
    progressLabel->setText("扫描文件中…");
    progress->setValue(0);
    scanner->start(files, config.minsize, config.maxsize);
    refreshActions();
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (scanner) scanner->stop();
    dispatcher->quit();
    event->accept();
}
