#include "Dispatcher.h"
#include "Scanner.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <iostream>
#include <cstdlib>

int checks = 0;
void check(bool passed, const char* message) {
    ++checks;
    if (!passed) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "fixture opens");
    check(file.write(bytes) == bytes.size(), "fixture saved");
}
QByteArray readFile(const QString& path) {
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "result opens");
    return file.readAll();
}
TableModelRow rowFor(const QString& path, int status) {
    TableModelRow row;
    row.path = path;
    row.size = QFileInfo(path).size();
    row.status = status;
    row.thin_size = status == 2 ? row.size : -1;
    return row;
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir temp;
    check(temp.isValid(), "temporary directory available");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    configItem config{};
    config.outputMode = OutputMode::Replace;
    config.tinyReqMode = TinyReqMode::Web;
    config.maxsize = 5 * 1024 * 1024;
    Config(nullptr).set(config);
    TableModel model(nullptr);
    MyTableView table(nullptr, &model);
    Console console;
    Dispatcher dispatcher(&console, &table);
    const QByteArray valid = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
        <!-- redundant annotation -->
        <circle cx="50" cy="50" r="30" fill="green" />
    </svg>)SVG";
    const QString success = temp.filePath("success.svg");
    const QString failed = temp.filePath("failed.svg");
    const QString pending = temp.filePath("pending.svg");
    writeFile(success, "successful sentinel");
    writeFile(failed, "<svg><circle></svg>");
    writeFile(pending, valid);
    model.addRow(rowFor(success, 2));
    TableModelRow failure = rowFor(failed, -1);
    failure.thin_size = 100; // Retry must use status, even with stale output size.
    model.addRow(failure);
    model.addRow(rowFor(pending, 0));
    auto runBatch = [&](bool failedOnly) {
        QEventLoop loop;
        bool completed = false;
        const auto connection = QObject::connect(&dispatcher, &Dispatcher::completed, &loop, [&]() { completed = true; loop.quit(); });
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        dispatcher.run(failedOnly);
        timer.start(10000);
        if (!completed) loop.exec();
        QObject::disconnect(connection);
        check(completed && !dispatcher.isRunning(), "batch finishes and workers stop");
    };
    runBatch(true);
    check(model.getRow(1).status == -1, "invalid SVG remains failed");
    check(readFile(failed) == "<svg><circle></svg>", "failed SVG preserves original bytes");
    check(model.getRow(2).status == 0 && readFile(pending) == valid, "retry does not process pending files");
    check(readFile(success) == "successful sentinel", "retry does not process successful files");
    writeFile(failed, valid);
    failure = rowFor(failed, -1);
    model.replaceRow(1, failure);
    runBatch(true);
    check(model.getRow(1).status == 2, "repaired failed SVG succeeds on retry");
    check(readFile(failed).size() < valid.size(), "SVG output is compacted");
    check(model.getRow(2).status == 0, "retry still leaves pending files alone");
    runBatch(false);
    check(model.getRow(2).status == 2, "start processes pending file");
    check(readFile(success) == "successful sentinel", "successful row is never recompressed");
    const QString resumed = temp.filePath("resumed.svg");
    writeFile(resumed, valid);
    model.addRow(rowFor(resumed, 0));
    dispatcher.run();
    dispatcher.quit();
    check(!dispatcher.isRunning() && model.getRow(3).status == 0, "pause restores pending state without deadlock");
    runBatch(false);
    check(model.getRow(3).status == 2, "fresh batch resumes after pause");
    writeFile(temp.filePath("UPPER.SVG"), valid);
    writeFile(temp.filePath("unsupported.avif"), "fixture");
    Scanner scanner;
    QList<TableModelRow> scanned;
    QEventLoop scanLoop;
    QObject::connect(&scanner, &Scanner::rowFound, &scanLoop, [&](const TableModelRow& row) { scanned.append(row); }, Qt::QueuedConnection);
    QObject::connect(&scanner, &QThread::finished, &scanLoop, &QEventLoop::quit);
    QTimer::singleShot(10000, &scanLoop, &QEventLoop::quit);
    scanner.start(QStringList() << temp.path(), 0, config.maxsize);
    scanLoop.exec();
    scanner.stop();
    bool uppercase = false, unsupported = false;
    for (const auto& row : scanned) {
        uppercase |= row.path.endsWith("UPPER.SVG");
        unsupported |= row.path.endsWith("unsupported.avif");
        check(row.path.startsWith('/'), "folder paths join correctly with source and output roots");
    }
    check(uppercase && !unsupported, "folder scanning accepts uppercase SVG and excludes AVIF");
    std::cout << "Passed " << checks << " integration checks\n";
}
