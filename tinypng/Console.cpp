#include "Console.h"
#include <QScrollBar>
#include <QDesktopServices>
#include <QMenu>
#include <QContextMenuEvent>
#include <QTextDocument>

Console::Console(QWidget* parent) : QTextBrowser(parent), atBottom(true) {
    setMinimumHeight(70);
    setStyleSheet("QTextBrowser { background: white; color: #768a7d; border: 0; font-size: 12px; }");
    document()->setMaximumBlockCount(1000);
    setOpenLinks(false);
    connect(this, &QTextEdit::textChanged, this, &Console::textChangedSlot);
    connect(this, &Console::infoSignal, this, &Console::info);
    connect(this, &Console::tipSignal, this, &Console::tip);
    connect(this, &Console::errorSignal, this, &Console::error);
    connect(this, &QTextBrowser::anchorClicked, this, &Console::clickLink);
    tip("已就绪 · PNG、JPG/JPEG、WebP 使用 TinyPNG 压缩，SVG 在本地精简。");
}
void Console::append(const QString& text) {
    atBottom = verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 4;
    QTextBrowser::append(text);
}
void Console::textChangedSlot() {
    if (atBottom) verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
void Console::contextMenuEvent(QContextMenuEvent* event) {
    QMenu* menu = createStandardContextMenu();
    menu->addSeparator();
    menu->addAction("清空记录", this, &QTextEdit::clear);
    menu->exec(event->globalPos());
    delete menu;
}
void Console::info(const QString& text) { append(QString("<span style=\"color:#2c8b5b\">%1</span>").arg(text)); }
void Console::tip(const QString& text) { append(QString("<span style=\"color:#788b7f\">%1</span>").arg(text)); }
void Console::error(QString text) { append(QString("<span style=\"color:#b85e4d\">%1</span>").arg(text)); }
void Console::clickLink(const QUrl& link) { QDesktopServices::openUrl(link); }
Console::~Console() {}
