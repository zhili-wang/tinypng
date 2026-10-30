#include "Console.h"
#include "Theme.h"
#include <QScrollBar>
#include <QDesktopServices>
#include <QMenu>
#include <QContextMenuEvent>
#include <QTextDocument>

Console::Console(QWidget* parent) : QTextBrowser(parent), atBottom(true) {
    setMinimumHeight(70);
    const auto applyTheme = [this]() {
        setStyleSheet(Theme::instance()->css("QTextBrowser { background: white; color: #768a7d; border: 0; font-size: 12px; }"));
        QString html = document()->toHtml();
        const bool dark = Theme::instance()->isDark();
        html.replace(dark ? "#2c8b5b" : "#88d5a5", dark ? "#88d5a5" : "#2c8b5b");
        html.replace(dark ? "#788b7f" : "#a4bcad", dark ? "#a4bcad" : "#788b7f");
        html.replace(dark ? "#b85e4d" : "#efa891", dark ? "#efa891" : "#b85e4d");
        const int scroll = verticalScrollBar()->value();
        setHtml(html);
        verticalScrollBar()->setValue(scroll);
    };
    connect(Theme::instance(), &Theme::changed, this, applyTheme);
    applyTheme();
    document()->setMaximumBlockCount(1000);
    setOpenLinks(false);
    connect(this, &QTextEdit::textChanged, this, &Console::textChangedSlot);
    connect(this, &Console::infoSignal, this, &Console::info);
    connect(this, &Console::tipSignal, this, &Console::tip);
    connect(this, &Console::errorSignal, this, &Console::error);
    connect(this, &QTextBrowser::anchorClicked, this, &Console::clickLink);
    tip("已就绪 · PNG、JPG/JPEG、WebP、SVG、AVIF、JXL 使用 TinyPNG 压缩 · 每批 10 张；可在设置中开启失败自动重试。");
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
void Console::info(const QString& text) { append(QString("<span style=\"color:%2\">%1</span>").arg(text, Theme::instance()->color("#2c8b5b", "#88d5a5").name())); }
void Console::tip(const QString& text) { append(QString("<span style=\"color:%2\">%1</span>").arg(text, Theme::instance()->color("#788b7f", "#a4bcad").name())); }
void Console::error(QString text) { append(QString("<span style=\"color:%2\">%1</span>").arg(text, Theme::instance()->color("#b85e4d", "#efa891").name())); }
void Console::clickLink(const QUrl& link) { QDesktopServices::openUrl(link); }
Console::~Console() {}
