#include "Scanner.h"
#include "Utility.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>

namespace {
QString sizeDescription(qint64 bytes) {
    return QLocale(QLocale::English).formattedDataSize(bytes, 2, QLocale::DataSizeTraditionalFormat)
        + QString("（%1 字节）").arg(bytes);
}
}

Scanner::Scanner(QObject* parent) : QThread(parent), minsize(0), maxsize(0) {
    qRegisterMetaType<TableModelRow>();
}
Scanner::~Scanner() { stop(); }
void Scanner::start(const QStringList& input, int minimum, int maximum) {
    stop();
    files = input;
    minsize = minimum;
    maxsize = maximum;
    QThread::start();
}
void Scanner::stop() {
    requestInterruption();
    wait();
}
void Scanner::addFile(const QString& root, const QString& path) {
    const QFileInfo info(root + path);
    if (!info.isFile() || !info.isReadable()) {
        emit fileSkipped(info.absoluteFilePath(), "文件不存在或无法读取，请检查文件路径与权限。");
        return;
    }
    if (!Utility::checkFileFormat(info.fileName())) {
        // Non-image files in a folder are expected; report explicitly selected files.
        if (root.isEmpty()) emit fileSkipped(info.absoluteFilePath(), "不支持此格式；支持 PNG、JPG/JPEG、WebP 和 SVG。");
        return;
    }
    if (info.size() == 0) {
        emit fileSkipped(info.absoluteFilePath(), "文件为空，无法压缩。");
        return;
    }
    if (info.size() < minsize || info.size() > maxsize) {
        const QString reason = info.size() > maxsize
            ? QString("文件大小 %1，超过当前上限 %2。可在“设置 → 大小范围”中调整。").arg(sizeDescription(info.size()), sizeDescription(maxsize))
            : QString("文件大小 %1，低于当前下限 %2。可在“设置 → 大小范围”中调整。").arg(sizeDescription(info.size()), sizeDescription(minsize));
        emit fileSkipped(info.absoluteFilePath(), reason);
        return;
    }
    TableModelRow row;
    row.root = root;
    row.path = path;
    row.size = info.size();
    row.thin_size = -1;
    row.status = 0;
    emit rowFound(row);
}
void Scanner::run() {
    if (files.isEmpty()) return;
    if (files.size() == 1 && QFileInfo(files.first()).isDir()) {
        const QString root = QFileInfo(files.first()).absoluteFilePath();
        // Apply the same case-insensitive filter to folders, file dialogs and drops.
        QDirIterator it(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (!isInterruptionRequested() && it.hasNext()) {
            const QString path = it.next();
            addFile(root, "/" + QDir(root).relativeFilePath(path));
        }
    } else {
        for (const QString& path : files) {
            if (isInterruptionRequested()) break;
            addFile("", path);
        }
    }
}
