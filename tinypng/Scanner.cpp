#include "Scanner.h"
#include "Utility.h"
#include <QDirIterator>
#include <QFileInfo>

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
    if (!info.isFile() || !Utility::checkFileFormat(info.fileName()) || info.size() < minsize || info.size() > maxsize) return;
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
