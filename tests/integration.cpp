#include "Dispatcher.h"
#include "Scanner.h"
#include "MainWindow.h"
#include "SettingWindow.h"
#include <QDir>
#include <QImage>
#include <QMap>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
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

    const QString importDir = temp.filePath("import-fixtures");
    check(QDir().mkpath(importDir), "import fixture directory created");
    const QString minimum = importDir + "/minimum.jpg";
    const QString maximum = importDir + "/maximum.JPG";
    const QString tooSmall = importDir + "/small.jpeg";
    const QString tooLarge = importDir + "/large.jPg";
    const QString empty = importDir + "/empty.jpg";
    const QString text = importDir + "/notes.txt";
    const QString missing = importDir + "/missing.jpg";
    writeFile(minimum, QByteArray(1024, 'a'));
    writeFile(maximum, QByteArray(2048, 'b'));
    writeFile(tooSmall, QByteArray(1023, 'c'));
    writeFile(tooLarge, QByteArray(2049, 'd'));
    writeFile(empty, QByteArray());
    writeFile(text, "not an image");
    struct ScanResult { QList<TableModelRow> rows; QMap<QString, QString> skipped; };
    auto scanPaths = [&](const QStringList& paths, int minBytes, int maxBytes) {
        ScanResult result;
        Scanner reader;
        QEventLoop loop;
        bool completed = false;
        QObject::connect(&reader, &Scanner::rowFound, &loop, [&](const TableModelRow& row) { result.rows.append(row); }, Qt::QueuedConnection);
        QObject::connect(&reader, &Scanner::fileSkipped, &loop, [&](const QString& path, const QString& reason) { result.skipped.insert(path, reason); }, Qt::QueuedConnection);
        QObject::connect(&reader, &QThread::finished, &loop, [&]() { completed = true; loop.quit(); });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        reader.start(paths, minBytes, maxBytes);
        loop.exec();
        reader.stop();
        check(completed, "import scan completes without timeout");
        return result;
    };
    const ScanResult limits = scanPaths({minimum, maximum, tooSmall, tooLarge, empty, text, missing}, 1024, 2048);
    check(limits.rows.size() == 2, "JPG files exactly at both size boundaries are accepted");
    check(limits.skipped.size() == 5, "every rejected selected file receives an explanation");
    check(limits.skipped.value(tooLarge).contains("超过当前上限") && limits.skipped.value(tooLarge).contains("2049")
        && limits.skipped.value(tooLarge).contains("2048"), "oversized warning includes actual and maximum byte sizes");
    check(limits.skipped.value(tooSmall).contains("低于当前下限") && limits.skipped.value(tooSmall).contains("1024"), "minimum-size rejection explains the limit");
    check(limits.skipped.value(empty).contains("文件为空"), "empty image has a clear explanation");
    check(limits.skipped.value(text).contains("不支持此格式") && limits.skipped.value(text).contains("JPG/JPEG"), "unsupported format lists accepted image formats");
    check(limits.skipped.value(missing).contains("文件不存在"), "missing file has a clear explanation");
    const ScanResult folderLimits = scanPaths({importDir}, 1024, 2048);
    check(folderLimits.rows.size() == 2 && folderLimits.skipped.contains(tooLarge), "folder imports use identical JPG and size rules");
    check(!folderLimits.skipped.contains(text), "ordinary non-image files in a folder do not create noisy warnings");

    const QString realJpg = importDir + "/real-photo.JPG";
    QImage photo(32, 32, QImage::Format_RGB32);
    photo.fill(Qt::green);
    check(photo.save(realJpg, "JPEG"), "real uppercase JPG fixture can be encoded");
    check(readFile(realJpg).startsWith(QByteArray::fromHex("ffd8")), "JPG fixture contains JPEG bytes");
    const QString hugeJpg = importDir + "/over-five-megabytes.JPG";
    QFile huge(hugeJpg);
    check(huge.open(QIODevice::WriteOnly) && huge.resize(qint64(config.maxsize) + 1), "oversized JPG fixture created");
    huge.close();
    MainWindow window;
    window.show();
    app.processEvents();
    auto waitForImport = [&]() {
        Scanner* reader = window.findChild<Scanner*>();
        check(reader != nullptr, "window starts an import scanner");
        if (reader->isRunning()) {
            QEventLoop loop;
            QObject::connect(reader, &QThread::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(10000, &loop, &QEventLoop::quit);
            loop.exec();
        }
        check(!reader->isRunning(), "window import finishes");
        app.processEvents();
    };
    auto dropUrls = [&](const QList<QUrl>& urls) {
        QMimeData mime;
        mime.setUrls(urls);
        QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window.table->viewport(), &enter);
        check(enter.isAccepted(), "URL drag reaches the drop handler even when a file will be rejected");
        QDropEvent drop(QPointF(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window.table->viewport(), &drop);
        check(drop.isAccepted() && drop.dropAction() == Qt::CopyAction, "drop processes files without moving originals");
    };
    QLabel* notice = window.findChild<QLabel*>("importNotice");
    check(notice != nullptr, "window has a persistent import notice");
    dropUrls({QUrl::fromLocalFile(realJpg), QUrl::fromLocalFile(hugeJpg)});
    dropUrls({QUrl::fromLocalFile(text)});
    check(notice->text().contains("正在读取图片"), "another drop during import explains why it cannot be added");
    waitForImport();
    check(window.table->model()->rowCount() == 1, "mixed drop adds the valid JPG and skips the oversized JPG");
    check(notice && notice->isVisible() && notice->text().contains("over-five-megabytes.JPG")
        && notice->text().contains("超过当前上限"), "oversized JPG creates a visible filename and size warning");
    check(notice->textFormat() == Qt::PlainText, "import notices render filenames as plain text");
    check(window.console->toPlainText().contains("5242881") && window.console->toPlainText().contains("5242880"), "activity log records precise actual and configured size");
    dropUrls({QUrl::fromLocalFile(hugeJpg)});
    waitForImport();
    check(window.table->model()->rowCount() == 1 && notice->text().contains("已添加 0"), "fully rejected drop keeps the previous queue and shows rejection");
    dropUrls({QUrl::fromLocalFile(text)});
    waitForImport();
    check(window.table->model()->rowCount() == 1 && notice->text().contains("不支持此格式"), "unsupported-only drop is explained and preserves existing images");
    dropUrls({QUrl("https://example.com/photo.jpg")});
    check(notice->text().contains("本地图片") && window.table->model()->rowCount() == 1, "remote URL drop receives a visible explanation");
    dropUrls({QUrl::fromLocalFile(importDir), QUrl::fromLocalFile(realJpg)});
    check(notice->text().contains("不能与单独的图片混合") && window.table->model()->rowCount() == 1, "mixed folder and file drop is explained before altering the queue");
    check(QMetaObject::invokeMethod(&window, "onOpenSettingWindow", Qt::DirectConnection), "settings can be opened");
    SettingWindow* settings = window.findChild<SettingWindow*>();
    check(settings != nullptr, "settings window exists");
    for (QSpinBox* spin : settings->findChildren<QSpinBox*>()) {
        if (spin->value() == 5120) spin->setValue(6144);
    }
    check(QMetaObject::invokeMethod(settings, "onClickSaveBtn", Qt::DirectConnection), "size range setting saves");
    check(Config(nullptr).get().maxsize == 6 * 1024 * 1024, "settings update the maximum import size");
    QLabel* sizeHint = window.findChild<QLabel*>("sizeRangeLabel");
    check(sizeHint && sizeHint->text().contains("6.00 MB"), "current size hint updates immediately after saving settings");
    dropUrls({QUrl::fromLocalFile(hugeJpg)});
    waitForImport();
    check(window.table->model()->rowCount() == 1 && !notice->isVisible(), "previously oversized JPG imports after increasing the local limit");
    std::cout << "Passed " << checks << " integration checks\n";
}
