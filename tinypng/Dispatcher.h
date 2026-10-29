#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QThread>
#include "Compress.h"
#include "Console.h"
#include "MyTableView.h"

class Dispatcher : public QObject {
    Q_OBJECT
public:
    Dispatcher(Console* console, MyTableView* table, QObject* parent = nullptr);
    ~Dispatcher();
    bool isRunning() const;
    void run(bool failedOnly = false);
    void quit();
signals:
    void started();
    void completed();
    void progressChanged(int finished, int total, int failures);
private:
    Console* console;
    MyTableView* tableview;
    TableModel* model;
    QHash<Compress*, QThread*> workers;
    QList<int> taskRows;
    int offset;
    int activeWorkers;
    int finishNum;
    int failedNum;
    qint64 totalSize;
    qint64 savedSize;
    quint64 generation;
    bool running;
    QElapsedTimer elapsedTimer;
    void nextTask(Compress* worker);
    void doneTask(Compress* worker, const QVariant& result);
    void stopWorkers();
};
