#if defined(_MSC_VER) && _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif
#include "MainWindow.h"
#include "SettingWindow.h"
#include "Utility.h"
#include "WorkspaceUi.h"
#include "QueueAppearance.h"
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFrame>
#include <QSplitter>
#include <QDesktopServices>
#include <QStatusBar>
#include <QFontDatabase>
#include <QFileInfo>
#include <QLocale>


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
    connect(Theme::instance(), &Theme::changed, this, &MainWindow::refreshTheme);
    refreshTheme();
    dispatcher = new Dispatcher(console, table, this);
    connect(table, &MyTableView::readResource, this, &MainWindow::createScannner);
    connect(table, &MyTableView::importRejected, this, &MainWindow::showImportNotice);
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
    refreshSizeHint();
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
    sizeRangeLabel = ui.sizeRangeLabel;
    importNotice = ui.importNotice;
    importNoticeBox = ui.importNoticeBox;
    progress = ui.progress;
    connect(addbtn, &QPushButton::clicked, this, &MainWindow::onClickEditBtn);
    connect(folderbtn, &QPushButton::clicked, this, &MainWindow::onClickFolderBtn);
    connect(startbtn, &QPushButton::clicked, this, &MainWindow::onClickStartBtn);
    connect(pausebtn, &QPushButton::clicked, this, &MainWindow::onClickPauseBtn);
    connect(retrybtn, &QPushButton::clicked, this, &MainWindow::onClickRetryBtn);
    connect(settingsbtn, &QPushButton::clicked, this, &MainWindow::onOpenSettingWindow);
}

void MainWindow::refreshTheme() {
    setStyleSheet(WorkspaceAppearance::theme());
    QueueAppearance::applyTheme(table);
    table->viewport()->update();
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
    const bool scanning = importing;
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
void MainWindow::refreshSizeHint() {
    const configItem config = Config(this).get();
    const QLocale locale(QLocale::English);
    const int maximum = config.tinyReqMode == TinyReqMode::Web
        ? qMin(config.maxsize, int(Utility::webMaxFileSize())) : config.maxsize;
    sizeRangeLabel->setText(QString("当前大小范围：%1 – %2 · %3")
        .arg(locale.formattedDataSize(config.minsize, 2, QLocale::DataSizeTraditionalFormat),
             locale.formattedDataSize(maximum, 2, QLocale::DataSizeTraditionalFormat),
             config.tinyReqMode == TinyReqMode::Web ? "官网免费模式：单张最多 5 MB" : "API KEY 模式 · SVG / JXL 单张最多 5 MB"));
    sizeRangeLabel->setToolTip(QString("本地筛选范围：%1 至 %2 字节。官网免费模式（含 SVG / JXL）的单张上限为 5 MB，调大本地范围不会提高官网上限。")
        .arg(config.minsize).arg(config.maxsize));
}
void MainWindow::showImportNotice(const QString& message) {
    importNotice->setText(message);
    importNoticeBox->show();
    console->error(message.toHtmlEscaped().replace('\n', "<br>"));
}
void MainWindow::onClickEditBtn() {
    createScannner(QFileDialog::getOpenFileNames(this, "选择图片", "", Utility::getFileDialogFilter()));
}
void MainWindow::onClickFolderBtn() {
    const QString folder = QFileDialog::getExistingDirectory(this, "选择图片文件夹");
    if (!folder.isEmpty()) createScannner(QStringList() << folder);
}
void MainWindow::onClickStartBtn() { if (!importing) dispatcher->run(); }
void MainWindow::onClickRetryBtn() { if (!importing) dispatcher->run(true); }
void MainWindow::onClickPauseBtn() {
    if (!dispatcher->isRunning()) return;
    dispatcher->quit();
    console->tip("已暂停，未完成的文件可继续压缩");
    progressLabel->setText("已暂停 · 点击开始压缩继续");
}
void MainWindow::onOpenSettingWindow() {
    for (SettingWindow* settings : findChildren<SettingWindow*>()) {
        if (settings->isVisible()) {
            settings->raise();
            settings->activateWindow();
            return;
        }
    }
    SettingWindow* settings = new SettingWindow(this);
    connect(settings, &QDialog::accepted, this, &MainWindow::refreshSizeHint);
    settings->show();
}
void MainWindow::createScannner(const QStringList& files) {
    if (files.isEmpty()) return;
    if (importing) {
        showImportNotice("正在读取图片，请等待本次读取完成后再添加。");
        return;
    }
    if (dispatcher->isRunning()) {
        showImportNotice("正在压缩图片，请先暂停压缩，再添加新的图片或文件夹。");
        return;
    }
    int directories = 0;
    for (const QString& file : files) directories += QFileInfo(file).isDir() ? 1 : 0;
    if (directories > 1 || (directories == 1 && files.size() > 1)) {
        showImportNotice("一次只能添加一个文件夹，不能与单独的图片混合拖入。请分批添加，或选择这些图片的共同文件夹。");
        return;
    }
    if (scanner) { delete scanner; scanner = nullptr; }
    TableModel* model = static_cast<TableModel*>(table->model());
    scanAccepted = 0;
    scanSkipped = 0;
    firstSkipped.clear();
    importNoticeBox->hide();
    refreshSizeHint();
    scanner = new Scanner(this);
    importing = true;
    connect(scanner, &Scanner::rowFound, this, [this, model](const TableModelRow& row) {
        // An invalid import must not erase the user's existing queue.
        if (scanAccepted == 0) model->removeAll();
        ++scanAccepted;
        model->addRow(row);
    }, Qt::QueuedConnection);
    connect(scanner, &Scanner::fileSkipped, this, [this](const QString& path, const QString& reason) {
        ++scanSkipped;
        if (firstSkipped.isEmpty()) firstSkipped = QFileInfo(path).fileName() + "：" + reason;
        console->error((path + "：" + reason).toHtmlEscaped());
    }, Qt::QueuedConnection);
    connect(scanner, &QThread::finished, this, [this]() {
        importing = false;
        table->setProcessingLocked(false);
        stateLabel->setText("就绪");
        if (scanAccepted > 0) {
            progress->setRange(0, scanAccepted);
            progress->setValue(0);
            progressLabel->setText(QString("%1 个文件待压缩").arg(scanAccepted));
        } else {
            progressLabel->setText(table->model()->rowCount() > 0 ? "未添加新图片 · 原队列已保留" : "未添加图片 · 请查看上方提示");
        }
        refreshSummary();
        refreshActions();
        if (scanSkipped > 0) {
            QString message = QString("已添加 %1 张图片，%2 个文件未添加。\n%3").arg(scanAccepted).arg(scanSkipped).arg(firstSkipped);
            if (scanSkipped > 1) message += "\n全部跳过原因可在下方活动记录中查看。";
            showImportNotice(message);
        } else if (scanAccepted == 0) {
            showImportNotice("没有找到 PNG、JPG/JPEG、WebP、SVG、AVIF 或 JXL 图片。请检查文件夹内容和当前大小范围。");
        }
        if (scanAccepted > 0 && Config(this).get().autoStart) onClickStartBtn();
    });
    const configItem config = Config(this).get();
    table->setProcessingLocked(true);
    stateLabel->setText("正在读取图片");
    progressLabel->setText("扫描文件中…");
    progress->setValue(0);
    scanner->start(files, config.minsize, config.maxsize, config.tinyReqMode);
    refreshActions();
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (scanner) scanner->stop();
    dispatcher->quit();
    event->accept();
}
