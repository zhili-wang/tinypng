#include "Dispatcher.h"

Dispatcher::Dispatcher(Console* output, MyTableView* table, QObject* parent)
    : QObject(parent), console(output), tableview(table), model(static_cast<TableModel*>(table->model())),
      offset(0), activeWorkers(0), finishNum(0), failedNum(0), totalSize(0), savedSize(0), generation(0), running(false) {
    qRegisterMetaType<CompressResult>();
}
Dispatcher::~Dispatcher() { quit(); }
bool Dispatcher::isRunning() const { return running; }

void Dispatcher::run(bool failedOnly) {
    if (running) return;
    taskRows.clear();
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->getRow(row).needsCompression(failedOnly)) taskRows.append(row);
    }
    if (taskRows.isEmpty()) {
        console->tip(failedOnly ? "没有失败项需要重试" : "没有可压缩的文件");
        return;
    }
    offset = finishNum = failedNum = 0;
    totalSize = savedSize = 0;
    running = true;
    const quint64 batch = ++generation;
    const configItem config = Config(this).get();
    QString outputRoot = model->getRow(taskRows.first()).root;
    if (config.outputMode == OutputMode::NewDir && !outputRoot.isEmpty()) outputRoot += "_tiny";
    console->tip(failedOnly ? QString("重试 %1 个失败项").arg(taskRows.size()) : QString("开始压缩 %1 个文件").arg(taskRows.size()));
    elapsedTimer.start();
    activeWorkers = qMin(taskRows.size(), qMax(1, qMin(QThread::idealThreadCount() * 2, 12)));
    tableview->setProcessingLocked(true);
    emit started();
    emit progressChanged(0, taskRows.size(), 0);
    const int count = activeWorkers;
    for (int i = 0; i < count; ++i) {
        QThread* thread = new QThread(this);
        Compress* worker = new Compress(config, outputRoot);
        worker->moveToThread(thread);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &Compress::finished, this, [this, worker, batch](const QVariant& result) {
            // Discard results queued by a batch that was paused or replaced.
            if (running && generation == batch) doneTask(worker, result);
        });
        workers.insert(worker, thread);
        thread->start();
        nextTask(worker);
    }
}

void Dispatcher::nextTask(Compress* worker) {
    if (!running) return;
    if (offset >= taskRows.size()) {
        if (--activeWorkers == 0) {
            const QString seconds = QString::number(elapsedTimer.elapsed() / 1000.0, 'f', 2);
            const QString rate = QString::number(totalSize > 0 ? savedSize * 100.0 / totalSize : 0, 'f', 1);
            console->info(QString("压缩完成：%1 个成功，%2 个失败 · %3 秒 · 节省 %4%")
                .arg(finishNum - failedNum).arg(failedNum).arg(seconds, rate));
            quit();
        }
        return;
    }
    const int row = taskRows.at(offset++);
    TableModelRow item = model->getRow(row);
    item.status = 1;
    item.thin_size = -1;
    model->replaceRow(row, item);
    QMetaObject::invokeMethod(worker, "process", Qt::QueuedConnection,
        Q_ARG(int, row), Q_ARG(QString, item.root), Q_ARG(QString, item.path));
}

void Dispatcher::doneTask(Compress* worker, const QVariant& variant) {
    const CompressResult result = variant.value<CompressResult>();
    if (result.row < 0 || result.row >= model->rowCount()) return;
    TableModelRow item = model->getRow(result.row);
    ++finishNum;
    const QString displayPath = item.path.toHtmlEscaped();
    if (result.errcode == 0) {
        item.status = 2;
        item.thin_size = result.size;
        totalSize += item.size;
        savedSize += item.size - item.thin_size;
        console->info(QString("%1/%2 · %3 · 已完成").arg(finishNum).arg(taskRows.size()).arg(displayPath));
    } else {
        item.status = -1;
        item.thin_size = -1;
        ++failedNum;
        console->error(QString("%1/%2 · %3 · %4").arg(finishNum).arg(taskRows.size()).arg(displayPath, result.errmsg.toHtmlEscaped()));
    }
    model->replaceRow(result.row, item);
    emit progressChanged(finishNum, taskRows.size(), failedNum);
    nextTask(worker);
}

void Dispatcher::stopWorkers() {
    for (auto it = workers.begin(); it != workers.end(); ++it) {
        disconnect(it.key(), nullptr, this, nullptr);
        it.value()->requestInterruption();
        QMetaObject::invokeMethod(it.key(), "cancel", Qt::QueuedConnection);
        it.value()->quit();
    }
    for (QThread* thread : workers) {
        thread->wait();
        delete thread;
    }
    workers.clear();
}

void Dispatcher::quit() {
    if (!running) return;
    running = false;
    ++generation;
    stopWorkers();
    for (int row : taskRows) {
        TableModelRow item = model->getRow(row);
        if (item.status == 1) {
            item.status = 0;
            model->replaceRow(row, item);
        }
    }
    tableview->setProcessingLocked(false);
    emit completed();
}
