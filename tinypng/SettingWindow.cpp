#include "SettingWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

SettingWindow::SettingWindow(QWidget* parent) : QDialog(parent) {
    setWindowTitle("TinyPNG · 压缩设置");
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(470);
    setStyleSheet(R"(
QDialog { background: #f4f7f5; }
QGroupBox { background: white; border: 1px solid #e1e9e4; border-radius: 10px; margin-top: 22px; padding: 16px; font-weight: 500; }
QGroupBox::title { subcontrol-origin: margin; left: 4px; color: #42624e; }
QLineEdit, QSpinBox { background: #f8fbf9; border: 1px solid #dce6df; border-radius: 6px; padding: 7px; min-height: 20px; }
QLineEdit:focus, QSpinBox:focus { border-color: #64a87b; }
QRadioButton, QCheckBox { spacing: 8px; font-weight: normal; }
QPushButton#save { background: #278558; color: white; border-color: #278558; }
)");
    QVBoxLayout* column = new QVBoxLayout(this);
    column->setContentsMargins(24, 22, 24, 22);
    column->setSpacing(14);
    QLabel* title = new QLabel("按你的方式压缩");
    title->setStyleSheet("font-size: 22px; font-weight: 600;");
    column->addWidget(title);
    QLabel* description = new QLabel("调整压缩服务、文件范围和保存方式。");
    description->setStyleSheet("color: #7b8d83;");
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
    mailInput = new QLineEdit;
    keyInput = new QLineEdit;
    keyInput->setEchoMode(QLineEdit::Password);
    credentialForm->addRow("账号", mailInput);
    credentialForm->addRow("API KEY", keyInput);
    serviceLayout->addWidget(credentials);
    connect(keyMode, &QRadioButton::toggled, credentials, &QWidget::setVisible);
    QLabel* svg = new QLabel("SVG 在本地精简，无需网络或 API KEY。");
    svg->setStyleSheet("color: #7b8d83; font-size: 12px;");
    serviceLayout->addWidget(svg);
    column->addWidget(service);
    QGroupBox* files = new QGroupBox("文件与输出");
    QVBoxLayout* filesLayout = new QVBoxLayout(files);
    QFormLayout* form = new QFormLayout;
    QWidget* sizeRange = new QWidget;
    QHBoxLayout* sizes = new QHBoxLayout(sizeRange);
    sizes->setContentsMargins(0, 0, 0, 0);
    minSize = new QSpinBox;
    maxSize = new QSpinBox;
    minSize->setRange(0, 200000);
    maxSize->setRange(1, 200000);
    minSize->setSuffix(" KB");
    maxSize->setSuffix(" KB");
    sizes->addWidget(minSize);
    sizes->addWidget(new QLabel("至"));
    sizes->addWidget(maxSize);
    form->addRow("大小范围", sizeRange);
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
    column->addWidget(files);
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
    mailInput->setText(config.mail);
    keyInput->setText(config.key);
    proxyInput->setText(config.proxy);
    minSize->setValue(config.minsize / 1024);
    maxSize->setValue(config.maxsize / 1024);
    newDirectory->setChecked(config.outputMode == OutputMode::NewDir);
    replaceOriginal->setChecked(config.outputMode == OutputMode::Replace);
    autoStart->setChecked(config.autoStart);
}
void SettingWindow::onClickSaveBtn() {
    if (minSize->value() > maxSize->value()) {
        QMessageBox::warning(this, "大小范围无效", "最小文件大小不能超过最大文件大小。");
        return;
    }
    if (keyMode->isChecked() && (mailInput->text().trimmed().isEmpty() || keyInput->text().trimmed().isEmpty())) {
        QMessageBox::warning(this, "缺少 API 配置", "请填写账号和 API KEY。");
        return;
    }
    configItem config;
    config.mail = mailInput->text().trimmed();
    config.key = keyInput->text().trimmed();
    config.proxy = proxyInput->text().trimmed();
    config.minsize = minSize->value() * 1024;
    config.maxsize = maxSize->value() * 1024;
    config.outputMode = newDirectory->isChecked() ? OutputMode::NewDir : OutputMode::Replace;
    config.tinyReqMode = keyMode->isChecked() ? TinyReqMode::Key : TinyReqMode::Web;
    config.autoStart = autoStart->isChecked();
    Config(this).set(config);
    accept();
}
