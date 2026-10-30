#pragma once
#include <QMainWindow>
#include <QTableView>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSplitter>
#include <QProgressBar>
#include <QDesktopServices>
#include <QScrollArea>
#include <QEvent>
#include "Theme.h"

namespace WorkspaceAppearance {
inline QString theme() {
    return Theme::instance()->css(QStringLiteral(R"(
QMainWindow, QWidget#workspace { background: #f4f7f5; }
QWidget { color: #203b30; font-size: 13px; }
QFrame#card, QFrame#stat { background: white; border: 1px solid #e1e9e4; border-radius: 12px; }
QLabel#brand { font-size: 28px; font-weight: 700; color: #183e2b; }
QLabel#subtitle, QLabel#muted, QLabel#sizeRangeLabel { color: #7b8d83; }
QLabel#eyebrow { color: #42986b; font-size: 11px; font-weight: 600; }
QLabel#formats { background: #e5f1e9; color: #397756; border-radius: 13px; padding: 7px 12px; font-size: 11px; }
QLabel#sectionTitle { font-size: 15px; font-weight: 600; }
QFrame#importNoticeBox { background: #fff6ed; border: 1px solid #f0dcc7; border-radius: 8px; }
QLabel#importNotice { color: #985539; background: transparent; padding: 0; }
QScrollArea#importNoticeScroll, QScrollArea#importNoticeScroll QWidget { background: transparent; border: 0; }
QPushButton#closeNotice { background: transparent; color: #985539; border: 0; padding: 0; font-size: 20px; }
QLabel#statValue { font-size: 25px; font-weight: 600; }
QLabel#failedValue { font-size: 25px; font-weight: 600; color: #bf6353; }
QLabel#savedValue { font-size: 25px; font-weight: 600; color: #2d8f5e; }
QPushButton { background: white; color: #365a46; border: 1px solid #dce6df; border-radius: 8px; padding: 10px 16px; font-weight: 500; }
QPushButton:hover { background: #eef6f0; border-color: #a5c9b2; }
QPushButton:pressed { background: #deece2; }
QPushButton:focus { border: 1px solid #58a778; }
QPushButton#primary { background: #278558; border-color: #278558; color: white; }
QPushButton#primary:hover { background: #20724a; }
QPushButton#primary:pressed { background: #195e3c; }
QPushButton#retry { color: #a2593e; background: #fff6ed; border-color: #f0dcc7; }
QPushButton#retry:hover { background: #ffecd9; }
QPushButton:disabled, QPushButton#primary:disabled, QPushButton#retry:disabled { background: #edf1ee; color: #9daaa2; border-color: #e1e7e3; }
QPushButton#link { border: 0; background: transparent; color: #6e897a; padding: 4px 8px; }
QProgressBar { border: 0; background: #e9f0eb; border-radius: 4px; min-height: 7px; max-height: 7px; }
QProgressBar::chunk { background: #38a26b; border-radius: 4px; }
QSplitter::handle { background: transparent; height: 9px; }
QScrollBar:vertical { background: #f6f8f6; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: #d2dfd6; border-radius: 4px; min-height: 30px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QMenu { background: white; border: 1px solid #dce6df; padding: 6px; }
QMenu::item { padding: 8px 22px; }
QMenu::item:selected { background: #eaf4ed; color: #237c51; }
QToolTip { background: #122c1e; color: white; border: 0; padding: 7px; }
)"));
}
inline QLabel* label(const QString& text, const QString& name) {
    QLabel* result = new QLabel(text);
    result->setObjectName(name);
    return result;
}
inline QPushButton* button(const QString& text, const QString& name = "") {
    QPushButton* result = new QPushButton(text);
    result->setObjectName(name);
    result->setCursor(Qt::PointingHandCursor);
    result->setMinimumHeight(40);
    return result;
}
inline QFrame* statCard(const QString& title, QLabel*& value, const QString& name = "statValue") {
    QFrame* frame = new QFrame;
    frame->setObjectName("stat");
    QVBoxLayout* column = new QVBoxLayout(frame);
    column->setContentsMargins(18, 13, 18, 14);
    column->setSpacing(5);
    column->addWidget(label(title, "muted"));
    value = label("0", name);
    column->addWidget(value);
    return frame;
}
}

class ImportNoticeFrame : public QFrame {
protected:
    bool event(QEvent* event) override {
        const bool visibilityChanged = event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent;
        const bool handled = QFrame::event(event);
        if (visibilityChanged && parentWidget() && parentWidget()->layout()) {
            // The splitter must reserve the complete queue layout, including an open notice.
            QSplitter* splitter = qobject_cast<QSplitter*>(parentWidget()->parentWidget());
            const QList<int> sizes = splitter ? splitter->sizes() : QList<int>();
            QLayout* layout = parentWidget()->layout();
            layout->invalidate();
            parentWidget()->setMinimumHeight(layout->minimumSize().height());
            parentWidget()->updateGeometry();
            if (auto* main = qobject_cast<QMainWindow*>(window())) {
                main->centralWidget()->layout()->invalidate();
                main->setMinimumHeight(qMax(740, main->centralWidget()->layout()->minimumSize().height()));
            }
            if (splitter && event->type() == QEvent::HideToParent) splitter->setSizes(sizes);
        }
        return handled;
    }
};

struct WorkspaceUi {
    QVBoxLayout* layout;
    QPushButton *addbtn, *folderbtn, *startbtn, *pausebtn, *retrybtn, *settingsbtn;
    QLabel *countLabel, *successLabel, *failedLabel, *savedLabel, *stateLabel, *progressLabel, *sizeRangeLabel, *importNotice;
    QProgressBar* progress;
    QFrame* importNoticeBox;
void setup(QMainWindow* window, QTableView* table, QTextBrowser* console) {
    using namespace WorkspaceAppearance;
    QWidget* workspace = new QWidget;
    workspace->setObjectName("workspace");
    window->setCentralWidget(workspace);
    layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(28, 24, 28, 20);
    layout->setSpacing(18);
    QHBoxLayout* header = new QHBoxLayout;
    QVBoxLayout* title = new QVBoxLayout;
    title->setSpacing(3);
    title->addWidget(label("LESS SIZE. SAME BEAUTY.", "eyebrow"));
    title->addWidget(label("TinyPNG", "brand"));
    title->addWidget(label("让每一张图片，更轻一点。", "subtitle"));
    header->addLayout(title);
    header->addStretch();
    header->addWidget(label("PNG / JPG / JPEG / WebP / SVG / AVIF / JXL", "formats"), 0, Qt::AlignVCenter);
    layout->addLayout(header);

    QHBoxLayout* stats = new QHBoxLayout;
    stats->setSpacing(12);
    stats->addWidget(statCard("文件总数", countLabel));
    stats->addWidget(statCard("压缩成功", successLabel));
    stats->addWidget(statCard("需要重试", failedLabel, "failedValue"));
    stats->addWidget(statCard("已节省空间", savedLabel, "savedValue"));
    layout->addLayout(stats);

    QHBoxLayout* actions = new QHBoxLayout;
    actions->setSpacing(9);
    addbtn = button("＋ 添加图片");
    folderbtn = button("添加文件夹");
    startbtn = button("开始压缩", "primary");
    pausebtn = button("暂停压缩");
    retrybtn = button("重试失败项", "retry");
    retrybtn->setToolTip("只重试失败的文件（Ctrl+R）");
    settingsbtn = button("设置");
    addbtn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    startbtn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    pausebtn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    retrybtn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    actions->addWidget(addbtn);
    actions->addWidget(folderbtn);
    actions->addStretch();
    actions->addWidget(retrybtn);
    actions->addWidget(startbtn);
    actions->addWidget(pausebtn);
    actions->addWidget(settingsbtn);
    layout->addLayout(actions);

    QSplitter* splitter = new QSplitter(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    QFrame* files = new QFrame;
    files->setObjectName("card");
    QVBoxLayout* filesLayout = new QVBoxLayout(files);
    filesLayout->setContentsMargins(16, 14, 16, 12);
    filesLayout->setSpacing(12);
    QHBoxLayout* filesHeader = new QHBoxLayout;
    filesHeader->addWidget(label("压缩队列", "sectionTitle"));
    filesHeader->addStretch();
    filesHeader->addWidget(label("双击预览 · 右键管理", "muted"));
    filesLayout->addLayout(filesHeader);
    sizeRangeLabel = label("当前大小范围：0 B – 5 MB · 可在设置中调整", "sizeRangeLabel");
    sizeRangeLabel->setTextFormat(Qt::PlainText);
    sizeRangeLabel->setWordWrap(true);
    filesLayout->addWidget(sizeRangeLabel);
    importNoticeBox = new ImportNoticeFrame;
    importNoticeBox->setObjectName("importNoticeBox");
    QHBoxLayout* noticeLayout = new QHBoxLayout(importNoticeBox);
    noticeLayout->setContentsMargins(12, 10, 10, 10);
    QScrollArea* noticeScroll = new QScrollArea;
    noticeScroll->setObjectName("importNoticeScroll");
    noticeScroll->setWidgetResizable(true);
    noticeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    importNotice = label("", "importNotice");
    importNotice->setTextFormat(Qt::PlainText);
    importNotice->setWordWrap(true);
    importNotice->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    importNotice->setTextInteractionFlags(Qt::TextSelectableByMouse);
    importNotice->setMinimumWidth(0);
    noticeScroll->setWidget(importNotice);
    noticeLayout->addWidget(noticeScroll, 1);
    QPushButton* closeNotice = new QPushButton("×");
    closeNotice->setObjectName("closeNotice");
    closeNotice->setFixedSize(28, 28);
    closeNotice->setToolTip("关闭提示（详细原因保留在活动记录中）");
    closeNotice->setAccessibleName("关闭导入提示");
    noticeLayout->addWidget(closeNotice, 0, Qt::AlignTop);
    QObject::connect(closeNotice, &QPushButton::clicked, importNoticeBox, &QWidget::hide);
    importNoticeBox->setFixedHeight(94);
    importNoticeBox->hide();
    filesLayout->addWidget(importNoticeBox);
    table->setMinimumHeight(168);
    filesLayout->addWidget(table);
    splitter->addWidget(files);

    QFrame* log = new QFrame;
    log->setObjectName("card");
    QVBoxLayout* logLayout = new QVBoxLayout(log);
    logLayout->setContentsMargins(16, 12, 16, 12);
    logLayout->setSpacing(8);
    QHBoxLayout* logHeader = new QHBoxLayout;
    logHeader->addWidget(label("活动记录", "sectionTitle"));
    logHeader->addStretch();
    QPushButton* clear = button("清空记录", "link");
    clear->setMinimumHeight(22);
    logHeader->addWidget(clear);
    logLayout->addLayout(logHeader);
    logLayout->addWidget(console);
    log->setMinimumHeight(115);
    QObject::connect(clear, &QPushButton::clicked, console, &QTextEdit::clear);
    splitter->addWidget(log);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 340 << 150);
    layout->addWidget(splitter, 1);

    QVBoxLayout* footer = new QVBoxLayout;
    footer->setSpacing(8);
    progress = new QProgressBar;
    progress->setTextVisible(false);
    progress->setRange(0, 1);
    progress->setValue(0);
    footer->addWidget(progress);
    QHBoxLayout* status = new QHBoxLayout;
    stateLabel = label("就绪", "muted");
    progressLabel = label("添加图片或拖入文件夹开始", "muted");
    status->addWidget(stateLabel);
    status->addWidget(progressLabel);
    status->addStretch();
    QPushButton* github = button("GitHub ↗", "link");
    github->setMinimumHeight(22);
    QObject::connect(github, &QPushButton::clicked, window, []() { QDesktopServices::openUrl(QUrl("https://github.com/zhili-wang/tinypng")); });
    status->addWidget(github);
    footer->addLayout(status);
    layout->addLayout(footer);
}

};
