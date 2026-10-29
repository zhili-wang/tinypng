#include "Compress.h"
#include "Utility.h"
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

Compress::Compress(const configItem& cfg, const QString& output)
    : config(cfg), outputRoot(output), row(-1), cancelled(false), mgr(nullptr), activeReply(nullptr) {}

bool Compress::isCancelled() const {
    return cancelled || QThread::currentThread()->isInterruptionRequested();
}

void Compress::cancel() {
    cancelled = true;
    if (activeReply) activeReply->abort();
}

bool Compress::waitForReply(QNetworkReply* reply, int timeoutSeconds) {
    activeReply = reply;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutSeconds * 1000);
    if (!reply->isFinished() && !isCancelled()) loop.exec();
    const bool timedOut = !timer.isActive();
    timer.stop();
    activeReply = nullptr;
    if (isCancelled()) {
        reply->abort();
        return false;
    }
    if (timedOut || !reply->isFinished()) {
        reply->abort();
        emitError(QString("请求超时（%1 秒），可点击“重试失败项”重试").arg(timeoutSeconds));
        return false;
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 413) {
            emitError("图片超过压缩服务允许的大小（HTTP 413）。请先减小图片；调整本地“大小范围”不会提高服务限制。");
        } else {
            emitError(QString("请求失败：%1；%2").arg(reply->errorString(), QString::fromUtf8(reply->readAll())));
        }
        return false;
    }
    return true;
}

void Compress::process(int taskRow, const QString& root, const QString& taskPath) {
    row = taskRow;
    path = taskPath;
    if (isCancelled()) return;
    QFile file(root + path);
    if (!file.open(QIODevice::ReadOnly)) {
        emitError("打开文件失败：" + file.errorString());
        return;
    }
    const QByteArray binary = file.readAll();
    if (file.error() != QFile::NoError) {
        emitError("读取文件失败：" + file.errorString());
        return;
    }
    file.close();
    if (QFileInfo(path).suffix().compare("svg", Qt::CaseInsensitive) == 0) {
        QByteArray optimized;
        QString error;
        if (!Utility::optimizeSvg(binary, optimized, error)) emitError(error);
        else saveOutput(optimized);
        return;
    }
    // Create all network objects in the worker thread.
    if (!mgr) {
        mgr = new QNetworkAccessManager(this);
        if (!config.proxy.isEmpty()) {
            const QUrl proxyUrl(config.proxy);
            mgr->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyUrl.port(8080)));
        }
    }
    const bool webMode = config.tinyReqMode == TinyReqMode::Web;
    QNetworkRequest request(QUrl(webMode ? "https://tinypng.com/backend/opt/shrink" : "https://api.tinify.com/shrink"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    if (webMode) {
        request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    } else {
        request.setRawHeader("Authorization", "Basic " + (config.mail + ":" + config.key).toUtf8().toBase64());
    }
    QNetworkReply* reply = mgr->post(request, binary);
    if (waitForReply(reply, 180)) {
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code >= 200 && code < 300) download(reply->readAll());
        else emitError(QString("压缩接口返回 HTTP %1").arg(code));
    }
    reply->deleteLater();
}

void Compress::download(const QByteArray& response) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    const QJsonObject object = document.object();
    const QJsonObject output = object.value("output").toObject();
    const QUrl url(output.value("url").toString());
    if (parseError.error != QJsonParseError::NoError || !url.isValid() || url.scheme() != "https" || url.host().isEmpty()) {
        emitError("压缩接口返回无效结果：" + QString::fromUtf8(response));
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = mgr->get(request);
    if (waitForReply(reply, 120)) {
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        const qint64 expectedSize = output.value("size").toVariant().toLongLong();
        if (code != 200 || data.isEmpty() || (expectedSize > 0 && data.size() != expectedSize)) {
            emitError(QString("下载失败或文件不完整（HTTP %1）").arg(code));
        } else saveOutput(data);
    }
    reply->deleteLater();
}

void Compress::saveOutput(const QByteArray& data) {
    if (isCancelled()) return;
    const QString outputPath = outputRoot + path;
    const QDir directory = QFileInfo(outputPath).absoluteDir();
    if (!directory.exists() && !directory.mkpath(".")) {
        emitError("创建目录失败：" + directory.path());
        return;
    }
    // Commit atomically: failed writes must leave the original available for retry.
    QSaveFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
        emitError("保存文件失败：" + file.errorString());
        return;
    }
    if (isCancelled()) {
        file.cancelWriting();
        return;
    }
    if (!file.commit()) {
        emitError("保存文件失败：" + file.errorString());
        return;
    }
    CompressResult result;
    result.row = row;
    result.size = data.size();
    result.path = outputPath;
    emit finished(QVariant::fromValue(result));
}

void Compress::emitError(const QString& message) {
    if (isCancelled()) return;
    CompressResult result;
    result.row = row;
    result.errcode = 1;
    result.errmsg = message;
    emit finished(QVariant::fromValue(result));
}
