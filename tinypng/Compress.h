#pragma once
#include <QObject>
#include <QVariant>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include "Config.h"

struct CompressResult {
    int errcode = 0;
    QString errmsg;
    int size = 0;
    int row = -1;
    QString path;
};
Q_DECLARE_METATYPE(CompressResult)

class Compress : public QObject {
    Q_OBJECT
public:
    Compress(const configItem& config, const QString& outputRoot);
public slots:
    void process(int row, const QString& root, const QString& path);
    void cancel();
signals:
    void finished(QVariant result);
private:
    configItem config;
    QString outputRoot;
    QString path;
    int row;
    bool cancelled;
    QNetworkAccessManager* mgr;
    QNetworkReply* activeReply;
    bool waitForReply(QNetworkReply* reply, int timeoutSeconds);
    void download(const QByteArray& response);
    void saveOutput(const QByteArray& data);
    void emitError(const QString& message);
    bool isCancelled() const;
};
