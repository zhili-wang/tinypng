#pragma once
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include "MyTableView.h"
#include "Console.h"
#include "Dispatcher.h"
#include "Scanner.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    MyTableView* table;
    Console* console;
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    Dispatcher* dispatcher;
    Scanner* scanner;
    bool importing = false;
    int scanAccepted = 0;
    int scanSkipped = 0;
    QString firstSkipped;
    QVBoxLayout* layout;
    QPushButton* addbtn;
    QPushButton* folderbtn;
    QPushButton* startbtn;
    QPushButton* pausebtn;
    QPushButton* retrybtn;
    QPushButton* settingsbtn;
    QLabel* countLabel;
    QLabel* successLabel;
    QLabel* failedLabel;
    QLabel* savedLabel;
    QLabel* stateLabel;
    QLabel* progressLabel;
    QLabel* sizeRangeLabel;
    QLabel* importNotice;
    QProgressBar* progress;
    void buildInterface();
    void refreshSummary();
    void refreshActions();
    void refreshSizeHint();
    void showImportNotice(const QString& message);
private slots:
    void onClickEditBtn();
    void onClickFolderBtn();
    void onClickPauseBtn();
    void onClickStartBtn();
    void onClickRetryBtn();
    void onOpenSettingWindow();
    void createScannner(const QStringList& files);
};
