#pragma once
#include "Utility.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <memory>

// Local HTTP service exercises the real upload/process/download worker without external requests.
class MockService : public QTcpServer {
public:
    QHash<QString, QByteArray> stored, outputs;
    QHash<int, int> attempts;
    QSet<int> terminal;
    int active = 0, maximumActive = 0, uploaded = 0;
    bool barriersHeld = true, webTypesCorrect = true, noWebAuth = true;
    bool apiUploadAuth = false, apiDownloadAuth = false;
    bool batching = false;
    bool failAll = false;
    QString outputVariant;
    MockService() {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (hasPendingConnections()) {
                QTcpSocket* socket = nextPendingConnection();
                auto data = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, data]() {
                    data->append(socket->readAll());
                    const int split = data->indexOf("\r\n\r\n");
                    if (split < 0) return;
                    const QByteArray header = data->left(split);
                    const auto length = QRegularExpression("content-length: (\\d+)", QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(header));
                    const int bytes = length.hasMatch() ? length.captured(1).toInt() : 0;
                    if (data->size() < split + 4 + bytes) return;
                    disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
                    handle(socket, header, data->mid(split + 4, bytes));
                });
            }
        });
    }
    QUrl base() const { return QUrl(QString("http://127.0.0.1:%1/").arg(serverPort())); }
    void reset() {
        attempts.clear(); terminal.clear(); stored.clear(); outputs.clear();
        active = maximumActive = uploaded = 0;
        barriersHeld = true;
    }
private:
    void respond(QTcpSocket* socket, int code, const QByteArray& body, const QByteArray& extra = {}) {
        if (socket->state() == QAbstractSocket::UnconnectedState) return;
        socket->write("HTTP/1.1 " + QByteArray::number(code) + " Response\r\nContent-Length: " + QByteArray::number(body.size())
            + "\r\nConnection: close\r\n" + extra + "\r\n" + body);
        socket->disconnectFromHost();
    }
    int id(const QByteArray& bytes) const {
        const auto match = QRegularExpression("data-test-id=\"(\\d+)\"").match(QString::fromUtf8(bytes));
        return match.hasMatch() ? match.captured(1).toInt() : -1;
    }
    void handle(QTcpSocket* socket, const QByteArray& header, const QByteArray& body) {
        const QByteArray path = header.split(' ').value(1);
        const QByteArray auth = "Authorization: Basic " + QByteArray("api:test-key").toBase64();
        if (path == "/store") {
            noWebAuth &= !header.toLower().contains("authorization:");
            const int task = id(body);
            if (batching && task >= 0) {
                for (int earlier = 0; earlier < (task / 10) * 10; ++earlier) barriersHeld &= terminal.contains(earlier);
                ++attempts[task];
            }
            ++active; maximumActive = qMax(maximumActive, active);
            const QString key = QString::number(++uploaded);
            stored.insert(key, body);
            respond(socket, 201, QJsonDocument(QJsonObject{{"key",key},{"size",body.size()}}).toJson());
        } else if (path == "/process") {
            noWebAuth &= !header.toLower().contains("authorization:");
            const QJsonObject request = QJsonDocument::fromJson(body).object();
            const QByteArray original = stored.value(request.value("key").toString());
            webTypesCorrect &= request.value("originalType").toString() == "image/svg+xml"
                && request.value("originalSize").toInt() == original.size();
            const int task = id(original);
            QByteArray optimized; QString error;
            const bool valid = Utility::optimizeSvg(original, optimized, error);
            if (failAll || (batching && (task == 1 || (task == 0 && attempts.value(task) <= 2))) || !valid) {
                --active;
                if (batching && attempts.value(task) == 3) terminal.insert(task);
                respond(socket, 503, "{\"message\":\"simulated failure\"}");
            } else {
                const QString key = request.value("key").toString();
                outputs.insert(key, optimized);
                QJsonObject result{{"url",base().resolved(QUrl("download/" + key)).toString()},{"size",optimized.size()}};
                if (outputVariant == "invalid-url") result.insert("url", "https://other.example/image.svg");
                if (outputVariant == "truncated") result.insert("size", optimized.size() + 1);
                respond(socket, 201, QJsonDocument(result).toJson());
            }
        } else if (path.startsWith("/download/")) {
            const QString key = QString::fromLatin1(path.mid(10));
            const int task = id(stored.value(key));
            --active;
            if (batching) terminal.insert(task);
            respond(socket, 200, outputs.value(key));
        } else if (path == "/shrink") {
            apiUploadAuth = header.contains(auth);
            respond(socket, 201, "{\"input\":{\"size\":10}}", "Location: " + base().resolved(QUrl("api-output")).toEncoded() + "\r\n");
        } else if (path == "/api-output") {
            apiDownloadAuth = header.contains(auth);
            respond(socket, 200, "compressed-jpg");
        } else respond(socket, 404, "missing");
    }
};
