#pragma once
#include <QThread>
#include "TableModel.h"
#include "Config.h"

class Scanner : public QThread {
    Q_OBJECT
public:
    explicit Scanner(QObject* parent = nullptr);
    ~Scanner();
    void start(const QStringList& files, int minsize, int maxsize, TinyReqMode mode = TinyReqMode::Web);
    void stop();
signals:
    void rowFound(TableModelRow row);
    void fileSkipped(QString path, QString reason);
protected:
    void run() override;
private:
    QStringList files;
    int minsize;
    int maxsize;
    TinyReqMode mode;
    void addFile(const QString& root, const QString& path);
};
