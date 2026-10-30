#pragma once
#include <QObject>
#include <QVariant>
#include <QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include "Config.h"

struct CompressResult {
    int errcode = 0;
    bool retryable = true;
    QString errmsg;
    int size = 0;
    int row = -1;
    QString path;
};
Q_DECLARE_METATYPE(CompressResult)

class Compress : public QObject {
    Q_OBJECT
public:
    Compress(const configItem& config, const QString& outputRoot,
        const QUrl& webBase = QUrl("https://tinypng.com/backend/opt/"),
        const QUrl& apiBase = QUrl("https://api.tinify.com/"));
public slots:
    void process(int row, const QString& root, const QString& path);
    void cancel();
signals:
    void finished(QVariant result);
private:
    configItem config;
    QString outputRoot;
    QUrl webBase, apiBase;
    bool usingWeb;
    QString path;
    int row;
    bool cancelled;
    QNetworkAccessManager* mgr;
    QNetworkReply* activeReply;
    bool waitForReply(QNetworkReply* reply, int timeoutSeconds);
    void processWeb(const QByteArray& binary, const QString& type);
    void download(const QByteArray& response, const QUrl& location = QUrl());
    void saveOutput(const QByteArray& data);
    void emitError(const QString& message, bool retryable = true);
    bool isCancelled() const;
};
