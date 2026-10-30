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

Compress::Compress(const configItem& cfg, const QString& output, const QUrl& web, const QUrl& api)
    : config(cfg), outputRoot(output), webBase(web), apiBase(api), usingWeb(true), row(-1), cancelled(false), mgr(nullptr), activeReply(nullptr) {}

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
            emitError("图片超过压缩服务允许的大小（HTTP 413）。请先减小图片；调整本地“大小范围”不会提高服务限制。", false);
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
    usingWeb = Utility::usesWebsite(path, config.tinyReqMode);
    QFile file(root + path);
    if (!file.open(QIODevice::ReadOnly)) {
        emitError("打开文件失败：" + file.errorString());
        return;
    }
    if (file.size() > Utility::maxFileSize()) {
        emitError(Utility::sizeLimitError(file.size()), false);
        return;
    }
    const QByteArray binary = file.readAll();
    if (file.error() != QFile::NoError) {
        emitError("读取文件失败：" + file.errorString());
        return;
    }
    file.close();
    if (binary.size() > Utility::maxFileSize()) {
        emitError(Utility::sizeLimitError(binary.size()), false);
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
    // SVG and JPEG XL use the website even when API KEY mode is selected.
    if (usingWeb) {
        processWeb(binary, Utility::imageMimeType(path));
        return;
    }
    QNetworkRequest request(apiBase.resolved(QUrl("shrink")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setRawHeader("Authorization", "Basic " + ("api:" + config.key).toUtf8().toBase64());
    QNetworkReply* reply = mgr->post(request, binary);
    if (waitForReply(reply, 180)) download(reply->readAll(), QUrl(QString::fromUtf8(reply->rawHeader("Location"))));
    reply->deleteLater();
}

void Compress::processWeb(const QByteArray& binary, const QString& type) {
    QNetworkRequest upload(webBase.resolved(QUrl("store")));
    upload.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    upload.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    QNetworkReply* stored = mgr->post(upload, binary);
    if (!waitForReply(stored, 180)) { stored->deleteLater(); return; }
    QJsonParseError error;
    const QJsonObject input = QJsonDocument::fromJson(stored->readAll(), &error).object();
    stored->deleteLater();
    const QString key = input.value("key").toString();
    if (error.error != QJsonParseError::NoError || key.isEmpty()) {
        emitError("上传接口未返回有效文件标识");
        return;
    }
    QJsonObject parameters;
    parameters.insert("key", key);
    parameters.insert("originalType", type);
    parameters.insert("originalSize", binary.size());
    QNetworkRequest request(webBase.resolved(QUrl("process")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    QNetworkReply* processed = mgr->post(request, QJsonDocument(parameters).toJson(QJsonDocument::Compact));
    if (waitForReply(processed, 180)) download(processed->readAll());
    processed->deleteLater();
}

void Compress::download(const QByteArray& response, const QUrl& location) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    const QJsonObject object = document.object();
    const QJsonObject output = usingWeb ? object : object.value("output").toObject();
    const QUrl url = location.isEmpty() ? QUrl(output.value("url").toString()) : location;
    const QUrl base = usingWeb ? webBase : apiBase;
    if (parseError.error != QJsonParseError::NoError || !url.isValid() || url.scheme() != base.scheme() || url.host() != base.host() || url.port() != base.port()) {
        emitError("压缩接口返回无效结果：" + QString::fromUtf8(response));
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    if (!usingWeb) request.setRawHeader("Authorization", "Basic " + ("api:" + config.key).toUtf8().toBase64());
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

void Compress::emitError(const QString& message, bool retryable) {
    if (isCancelled()) return;
    CompressResult result;
    result.row = row;
    result.errcode = 1;
    result.retryable = retryable;
    result.errmsg = message;
    emit finished(QVariant::fromValue(result));
}
