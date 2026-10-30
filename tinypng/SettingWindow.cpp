#include "SettingWindow.h"
#include "Theme.h"
#include "WorkspaceUi.h"
#include "Utility.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

// Some Qt stylesheet versions reserve an invalid edit rectangle when spin buttons are hidden.
// Keep the editable text inside the styled field across resizes and theme changes.
class SizeSpinBox : public QSpinBox {
protected:
    bool event(QEvent* event) override {
        const bool handled = QSpinBox::event(event);
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
        case QEvent::FontChange:
        case QEvent::Polish:
            lineEdit()->setGeometry(rect().adjusted(8, 2, -8, -2));
            break;
        default: break;
        }
        return handled;
    }
};

SettingWindow::SettingWindow(QWidget* parent) : QDialog(parent), originalTheme(Theme::instance()->mode()) {
    setWindowTitle("TinyPNG · 压缩设置");
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(470);
    const auto applyTheme = [this]() {
        setStyleSheet(WorkspaceAppearance::theme() + Theme::instance()->css(R"(
QDialog { background: #f4f7f5; }
QGroupBox { background: white; border: 1px solid #e1e9e4; border-radius: 10px; margin-top: 22px; padding: 16px; font-weight: 500; }
QGroupBox::title { subcontrol-origin: margin; left: 4px; color: #42624e; }
QLineEdit, QSpinBox { background: #f8fbf9; border: 1px solid #dce6df; border-radius: 6px; padding: 7px; min-height: 20px; }
QSpinBox QLineEdit { border: 0; background: transparent; padding: 0; min-height: 0; }
QLineEdit:focus, QSpinBox:focus { border-color: #64a87b; }
QRadioButton, QCheckBox { spacing: 8px; font-weight: normal; }
QPushButton#step { padding: 0; font-size: 16px; }
QPushButton#save { background: #278558; color: white; border-color: #278558; }
)") + QString(R"(
QRadioButton::indicator { width: 18px; height: 18px; border: 0; background: transparent; image: url(:/radio-%1.png); }
QRadioButton::indicator:checked { image: url(:/radio-%1-checked.png); }
QCheckBox::indicator { width: 16px; height: 16px; border: 0; background: transparent; image: url(:/check-%1.png); }
QCheckBox::indicator:checked { image: url(:/check-%1-checked.png); }
)").arg(Theme::instance()->isDark() ? "dark" : "light"));
    };
    connect(Theme::instance(), &Theme::changed, this, applyTheme);
    applyTheme();
    QVBoxLayout* column = new QVBoxLayout(this);
    column->setContentsMargins(24, 22, 24, 22);
    column->setSpacing(14);
    QLabel* title = new QLabel("按你的方式压缩");
    title->setStyleSheet("font-size: 22px; font-weight: 600;");
    column->addWidget(title);
    QLabel* description = new QLabel("调整压缩服务、文件范围和保存方式。");
    description->setObjectName("muted");
    column->addWidget(description);
    QGroupBox* service = new QGroupBox("压缩服务");
    QVBoxLayout* serviceLayout = new QVBoxLayout(service);
    QHBoxLayout* modes = new QHBoxLayout;
    webMode = new QRadioButton("网页模式（免 KEY）", service);
    keyMode = new QRadioButton("API KEY", service);
    modes->addWidget(webMode);
    modes->addWidget(keyMode);
    serviceLayout->addLayout(modes);
    QWidget* credentials = new QWidget;
    QFormLayout* credentialForm = new QFormLayout(credentials);
    credentialForm->setContentsMargins(0, 8, 0, 0);
    keyInput = new QLineEdit;
    keyInput->setObjectName("apiKeyInput");
    keyInput->setPlaceholderText("填写你自己的 API KEY");
    keyInput->setEchoMode(QLineEdit::Password);
    credentialForm->addRow("API KEY", keyInput);
    serviceLayout->addWidget(credentials);
    connect(keyMode, &QRadioButton::toggled, credentials, &QWidget::setVisible);
    QLabel* svg = new QLabel("SVG / JXL 使用官网压缩接口，无需 API KEY，单张最多 5 MB。");
    svg->setObjectName("muted");
    svg->setWordWrap(true);
    serviceLayout->addWidget(svg);
    column->addWidget(service);
    QGroupBox* files = new QGroupBox("文件与输出");
    QVBoxLayout* filesLayout = new QVBoxLayout(files);
    QFormLayout* form = new QFormLayout;
    QWidget* sizeRange = new QWidget;
    QHBoxLayout* sizes = new QHBoxLayout(sizeRange);
    sizes->setContentsMargins(0, 0, 0, 0);
    minSize = new SizeSpinBox;
    maxSize = new SizeSpinBox;
    minSize->setRange(0, int(Utility::maxFileSize() / 1024));
    maxSize->setRange(1, int(Utility::maxFileSize() / 1024));
    minSize->setSuffix(" KB");
    maxSize->setSuffix(" KB");
    const auto spinControl = [](QSpinBox* spin, const QString& name) {
        spin->setObjectName(name);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        QWidget* container = new QWidget;
        QHBoxLayout* row = new QHBoxLayout(container);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        QPushButton* minus = new QPushButton("−");
        QPushButton* plus = new QPushButton("+");
        minus->setObjectName("step"); plus->setObjectName("step");
        minus->setFixedSize(28, 36); plus->setFixedSize(28, 36);
        minus->setAccessibleName(name + "减少"); plus->setAccessibleName(name + "增加");
        minus->setAutoDefault(false); plus->setAutoDefault(false);
        row->addWidget(minus); row->addWidget(spin, 1); row->addWidget(plus);
        QObject::connect(minus, &QPushButton::clicked, spin, &QAbstractSpinBox::stepDown);
        QObject::connect(plus, &QPushButton::clicked, spin, &QAbstractSpinBox::stepUp);
        const auto updateButtons = [spin, minus, plus]() {
            minus->setEnabled(spin->value() > spin->minimum());
            plus->setEnabled(spin->value() < spin->maximum());
        };
        QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), container, updateButtons);
        updateButtons();
        return container;
    };
    sizes->addWidget(spinControl(minSize, "minSize"));
    sizes->addWidget(new QLabel("至"));
    sizes->addWidget(spinControl(maxSize, "maxSize"));
    form->addRow("大小范围", sizeRange);
    QLabel* sizeHelp = new QLabel("所有模式、所有图片格式单张最多 5 MB（5120 KB）。\n大小范围不能超过此上限，超限文件会显示跳过原因。");
    sizeHelp->setWordWrap(true);
    sizeHelp->setMinimumHeight(sizeHelp->fontMetrics().lineSpacing() * 4);
    sizeHelp->setObjectName("muted");
    form->addRow(sizeHelp);
    proxyInput = new QLineEdit;
    proxyInput->setPlaceholderText("可选，例如 http://127.0.0.1:7890");
    form->addRow("HTTP 代理", proxyInput);
    filesLayout->addLayout(form);
    newDirectory = new QRadioButton("文件夹另存到同级 _tiny 目录", files);
    replaceOriginal = new QRadioButton("覆盖原文件", files);
    newDirectory->setToolTip("添加单独的图片时仍保存到原路径；添加文件夹时另存到 _tiny 目录。");
    filesLayout->addWidget(newDirectory);
    filesLayout->addWidget(replaceOriginal);
    autoStart = new QCheckBox("添加图片后自动开始压缩");
    filesLayout->addWidget(autoStart);
    autoRetry = new QCheckBox("失败时自动重试（最多 2 次）");
    autoRetry->setObjectName("autoRetry");
    autoRetry->setToolTip("默认关闭；开启后最多重试 2 次。本批至少 3 张请求失败时，按 2–5 秒间隔排队重试，后续每批最多 3 张。超过服务大小上限的文件不自动重试。");
    filesLayout->addWidget(autoRetry);
    column->addWidget(files);
    QFormLayout* appearance = new QFormLayout;
    QWidget* themeChoices = new QWidget;
    QHBoxLayout* themeButtons = new QHBoxLayout(themeChoices);
    themeButtons->setContentsMargins(0, 0, 0, 0);
    themeButtons->setSpacing(18);
    themeMode = new QButtonGroup(this);
    themeMode->setObjectName("themeMode");
    const QStringList labels = {"浅色", "深色", "跟随系统"};
    const QStringList names = {"themeLight", "themeDark", "themeSystem"};
    for (int i = 0; i < labels.size(); ++i) {
        QRadioButton* choice = new QRadioButton(labels.at(i), themeChoices);
        choice->setObjectName(names.at(i));
        themeMode->addButton(choice, i);
        themeButtons->addWidget(choice);
    }
    themeButtons->addStretch();
    appearance->addRow("界面主题", themeChoices);
    column->addLayout(appearance);
    QHBoxLayout* actions = new QHBoxLayout;
    actions->addStretch();
    QPushButton* cancel = new QPushButton("取消");
    QPushButton* save = new QPushButton("保存设置");
    save->setObjectName("save");
    save->setDefault(true);
    actions->addWidget(cancel);
    actions->addWidget(save);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &SettingWindow::onClickSaveBtn);
    column->addLayout(actions);
    const configItem config = Config(this).get();
    webMode->setChecked(config.tinyReqMode == TinyReqMode::Web);
    keyMode->setChecked(config.tinyReqMode == TinyReqMode::Key);
    credentials->setVisible(keyMode->isChecked());
    keyInput->setText(config.key);
    proxyInput->setText(config.proxy);
    minSize->setValue(config.minsize / 1024);
    maxSize->setValue(config.maxsize / 1024);
    newDirectory->setChecked(config.outputMode == OutputMode::NewDir);
    replaceOriginal->setChecked(config.outputMode == OutputMode::Replace);
    autoStart->setChecked(config.autoStart);
    autoRetry->setChecked(config.autoRetry);
    themeMode->button(int(originalTheme))->setChecked(true);
    for (QAbstractButton* choice : themeMode->buttons()) {
        const ThemeMode mode = ThemeMode(themeMode->id(choice));
        connect(choice, &QAbstractButton::toggled, this, [mode](bool checked) {
            if (checked) Theme::instance()->setMode(mode);
        });
    }
}
void SettingWindow::done(int result) {
    if (!previewFinished) {
        previewFinished = true;
        if (result != QDialog::Accepted) Theme::instance()->setMode(originalTheme);
    }
    QDialog::done(result);
}

void SettingWindow::onClickSaveBtn() {
    if (minSize->value() > maxSize->value()) {
        QMessageBox::warning(this, "大小范围无效", "最小文件大小不能超过最大文件大小。");
        return;
    }
    if (keyMode->isChecked() && keyInput->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "缺少 API 配置", "请填写你自己的 API KEY。");
        return;
    }
    configItem config = Config(this).get();
    config.key = keyInput->text().trimmed();
    config.proxy = proxyInput->text().trimmed();
    config.minsize = minSize->value() * 1024;
    config.maxsize = maxSize->value() * 1024;
    config.outputMode = newDirectory->isChecked() ? OutputMode::NewDir : OutputMode::Replace;
    config.tinyReqMode = keyMode->isChecked() ? TinyReqMode::Key : TinyReqMode::Web;
    config.autoStart = autoStart->isChecked();
    config.autoRetry = autoRetry->isChecked();
    config.themeMode = ThemeMode(themeMode->checkedId());
    Config(this).set(config);
    Theme::instance()->setMode(config.themeMode);
    accept();
}
