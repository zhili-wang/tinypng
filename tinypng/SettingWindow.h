#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QButtonGroup>
#include <QKeyEvent>
#include "Config.h"

class SettingWindow : public QDialog {
    Q_OBJECT
public:
    explicit SettingWindow(QWidget* parent = nullptr);
private slots:
    void onClickSaveBtn();
private:
    QRadioButton* webMode;
    QRadioButton* keyMode;
    QButtonGroup* themeMode;
    QLineEdit* keyInput;
    QLineEdit* proxyInput;
    QSpinBox* minSize;
    QSpinBox* maxSize;
    QRadioButton* newDirectory;
    QRadioButton* replaceOriginal;
    QCheckBox* autoStart;
};
