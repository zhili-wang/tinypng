#include "WorkspaceUi.h"
#include "QueueAppearance.h"
#include <QApplication>
#include <QStandardItemModel>
#include <QDir>
#include <iostream>
int main(int argc, char** argv) {
    QApplication app(argc,argv);
    app.setStyle("Fusion");
    QFont font("PingFang SC");
    font.setPointSize(10);
    app.setFont(font);
    QMainWindow window;
    window.setStyleSheet(WorkspaceAppearance::theme());
    window.resize(1080,800);
    const QDir output(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::tempPath());
    QStandardItemModel model;
    model.setHorizontalHeaderLabels({"文件路径", "原始大小", "压缩后", "节省比例", "状态"});
    QStringList paths = {"images/hero-banner.png", "icons/brand-logo.svg", "photos/product-cover.webp", "images/navigation-icons.svg", "photos/lifestyle.jpg"};
    QStringList before = {"1.8 MB", "24.6 KB", "862.4 KB", "38.2 KB", "2.1 MB"};
    QStringList after = {"542.3 KB", "16.2 KB", "—", "—", "—"};
    QStringList ratios = {"70.6%", "34.1%", "—", "—", "—"};
    QStringList states = {"成功", "成功", "失败", "待压缩", "待压缩"};
    for(int row=0;row<5;++row) {
        QList<QStandardItem*> items;
        for(const QString& value : {paths[row],before[row],after[row],ratios[row],states[row]}) items.append(new QStandardItem(value));
        for(int col=1;col<5;++col) items[col]->setTextAlignment(Qt::AlignCenter);
        items[4]->setData(row<2?2:row==2?-1:0,Qt::UserRole+1);
        model.appendRow(items);
    }
    QTableView* table = new QTableView;
    table->setModel(&model);
    QueueAppearance::configure(table);
    QTextBrowser* console = new QTextBrowser;
    console->setStyleSheet("QTextBrowser { background: white; color: #788b7f; border: 0; font-size: 12px; }");
    console->append("已就绪 · PNG、JPEG、WebP 使用 TinyPNG 压缩，SVG 在本地精简。");
    console->append("<span style='color:#2c8b5b'>1/5 · images/hero-banner.png · 已完成</span>");
    console->append("<span style='color:#2c8b5b'>2/5 · icons/brand-logo.svg · 已完成</span>");
    console->append("<span style='color:#b85e4d'>3/5 · photos/product-cover.webp · 请求超时，可点击“重试失败项”重试</span>");
    WorkspaceUi ui;
    ui.setup(&window,table,console);
    ui.countLabel->setText("5");
    ui.successLabel->setText("2");
    ui.failedLabel->setText("1");
    ui.savedLabel->setText("1.3 MB");
    ui.retrybtn->setText("重试失败项 (1)");
    ui.pausebtn->hide();
    ui.progress->setRange(0,5);
    ui.progress->setValue(3);
    ui.progressLabel->setText("3 / 5 已处理 · 1 个失败");
    window.show();
    app.processEvents();
    if (!window.grab().save(output.filePath("tinypng-ui-preview.png"))) return 1;
    window.resize(840,740);
    app.processEvents();
    if (!window.grab().save(output.filePath("tinypng-ui-preview-compact.png"))) return 1;
    for(QPushButton* btn : {ui.addbtn,ui.folderbtn,ui.retrybtn,ui.startbtn,ui.settingsbtn}) {
      if(btn->width()<btn->minimumSizeHint().width()) { std::cerr<<"Clipped button: "<<btn->text().toStdString()<<'\n'; return 1; }
    }
    std::cout<<"Native Qt UI layouts rendered at 1080x800 and 840x740\n";
}
