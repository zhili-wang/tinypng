#include "MainWindow.h"
#include "SettingWindow.h"
#include "Theme.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QRadioButton>
#include <QTimer>
#include <QEventLoop>
#include <iostream>
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir preferences;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    configItem config{};
    config.autoStart = false; config.maxsize = 5 * 1024 * 1024;
    config.outputMode = OutputMode::Replace; config.tinyReqMode = TinyReqMode::Web;
    config.themeMode = ThemeMode::Light; Config(nullptr).set(config);
    QFont font("PingFang SC"); font.setPointSize(10); app.setFont(font);
    QDir output(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::tempPath());
    output.mkpath(".");
    MainWindow window;
    auto* model = static_cast<TableModel*>(window.table->model());
    const QStringList paths = {"images/hero-banner.png", "icons/brand-logo.svg", "photos/product-cover.webp", "images/navigation-icons.svg", "photos/lifestyle.jpg"};
    for (int i = 0; i < paths.size(); ++i) {
        TableModelRow row{}; row.path = paths.at(i); row.size = i == 1 ? 24800 : 1800000;
        row.status = i < 2 ? 2 : i == 2 ? -1 : 0;
        row.thin_size = row.status == 2 ? row.size / 3 : -1;
        model->addRow(row);
    }
    window.console->info("1/5 · images/hero-banner.png · 已完成");
    window.console->tip("photos/product-cover.webp · 请求失败 · 1 秒后自动重试 1/2");
    window.console->error("3/5 · photos/product-cover.webp · 重试 2 次后仍失败");
    auto capture = [&](QWidget* widget, const QString& name) {
        QEventLoop settle; QTimer::singleShot(120, &settle, &QEventLoop::quit); settle.exec();
        app.processEvents();
        if (!widget->grab().save(output.filePath(name))) std::exit(1);
    };
    window.show();
    for (ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
        Theme::instance()->setMode(mode);
        config.themeMode = mode; Config(nullptr).set(config);
        const QString suffix = mode == ThemeMode::Dark ? "dark" : "light";
        window.resize(1080, 800); capture(&window, "tinypng-queue-" + suffix + ".png");
        window.resize(840, 740); capture(&window, "tinypng-queue-compact-" + suffix + ".png");
    }
    model->removeAll();
    const QString warning = "已添加 0 张图片，1 个文件未添加。\nsample-jpg-files-sample_5184x3456.jpg：文件大小 5.30 MB（5554547 字节），超过当前上限 5.00 MB（5242880 字节）。可在“设置＞大小范围”中调整。";
    for (ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
        Theme::instance()->setMode(mode);
        config.themeMode = mode; Config(nullptr).set(config);
        const QString suffix = mode == ThemeMode::Dark ? "dark" : "light";
        window.table->importRejected(warning);
        capture(&window, "tinypng-empty-warning-" + suffix + ".png");
        if (window.table->viewport()->height() < 110 || window.table->geometry().bottom() >= window.table->parentWidget()->height()) return 2;
        const int withNotice = window.table->viewport()->height();
        window.findChild<QPushButton*>("closeNotice")->click();
        capture(&window, "tinypng-empty-dismissed-" + suffix + ".png");
        if (window.table->viewport()->height() <= withNotice) return 3;
        auto* settings = new SettingWindow(&window);
        settings->show();
        for (auto* radio : settings->findChildren<QRadioButton*>()) if (radio->text() == "API KEY") radio->setChecked(true);
        capture(settings, "tinypng-settings-" + suffix + ".png");
        for (auto* spin : settings->findChildren<QSpinBox*>()) {
            auto* edit = spin->findChild<QLineEdit*>();
            if (edit->width() < 60 || edit->height() < edit->fontMetrics().height()) {
                std::cerr << "Size input text is clipped\n"; return 4;
            }
        }
        settings->close();
    }
    Theme::instance()->setMode(ThemeMode::Light);
    auto* preview = new SettingWindow(&window); preview->show();
    preview->findChild<QRadioButton*>("themeDark")->click();
    capture(&window, "tinypng-live-preview-dark-workspace.png");
    capture(preview, "tinypng-live-preview-dark-settings.png");
    if (!Theme::instance()->isDark()) return 5;
    preview->findChild<QRadioButton*>("themeLight")->click();
    capture(&window, "tinypng-live-preview-light-workspace.png");
    if (Theme::instance()->isDark()) return 6;
    preview->findChild<QRadioButton*>("themeSystem")->click();
    if (Theme::instance()->mode() != ThemeMode::System) return 7;
    preview->close();
    if (Theme::instance()->mode() != ThemeMode::Light) return 8;
    std::cout << "Rendered actual Qt workspace, empty warnings, dismissal and settings in both themes\n";
}
