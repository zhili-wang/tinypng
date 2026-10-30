#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QThread>
#include <QTimer>
#include <QQueue>
#include <QSet>
#include <functional>
#include "Compress.h"
#include "Console.h"
#include "MyTableView.h"

class Dispatcher : public QObject {
    Q_OBJECT
public:
    using WorkerFactory = std::function<Compress*(const configItem&, const QString&)>;
    Dispatcher(Console* console, MyTableView* table, QObject* parent = nullptr, WorkerFactory factory = {});
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
    int batchRemaining;
    QHash<int, int> retries;
    QHash<Compress*, int> assignments;
    WorkerFactory workerFactory;
    int finishNum;
    int failedNum;
    qint64 totalSize;
    qint64 savedSize;
    quint64 generation;
    bool running;
    bool automaticRetry = false;
    bool throttled = false;
    int batchNumber = 0;
    QSet<int> failedRowsInBatch;
    QHash<Compress*, QTimer*> retryTimers;
    QQueue<Compress*> pendingRetries;
    QTimer* slowRetryTimer;
    QElapsedTimer elapsedTimer;
    void startBatch();
    void startTask(Compress* worker, int row);
    void doneTask(Compress* worker, const QVariant& result);
    void stopWorkers();
    void enableThrottling();
    void queueRetry(Compress* worker, int row, int attempt);
    void scheduleSlowRetry();
    void nextBatch();
};
