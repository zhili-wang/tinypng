#include "Dispatcher.h"
#include "Scanner.h"
#include "MainWindow.h"
#include "SettingWindow.h"
#include "MockService.h"
#include "Theme.h"
#include <QStyleHints>
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
    check(Config(nullptr).qset->fileName().startsWith(temp.path()), "test preferences stay in temporary directory instead of user application settings");
    check(!Config(nullptr).get().autoRetry, "automatic retry defaults to disabled for existing or new settings");
    configItem config{};
    config.outputMode = OutputMode::Replace;
    config.tinyReqMode = TinyReqMode::Web;
    config.maxsize = 5 * 1024 * 1024;
    config.autoRetry = true;
    Config(nullptr).set(config);
    TableModel model(nullptr);
    MyTableView table(nullptr, &model);
    Console console;
    MockService service;
    check(service.isListening(), "mock compression service listens locally");
    Dispatcher dispatcher(&console, &table, nullptr, [&](const configItem& cfg, const QString& output) {
        return new Compress(cfg, output, service.base(), service.base());
    });
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
        timer.start(30000);
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
    check(service.webTypesCorrect && service.noWebAuth, "SVG uses store/process with correct MIME, size and no API credentials");
    model.removeAll();
    service.reset();
    service.batching = true;
    for (int i = 0; i < 25; ++i) {
        const QString path = temp.filePath(QString("batch-%1.svg").arg(i));
        QByteArray fixture = valid;
        fixture.replace("viewBox=", "data-test-id=\"" + QByteArray::number(i) + "\" viewBox=");
        writeFile(path, fixture);
        model.addRow(rowFor(path, 0));
    }
    int updates = 0;
    bool terminalProgress = true;
    const auto progressCheck = QObject::connect(&dispatcher, &Dispatcher::progressChanged, &app, [&](int done, int, int failedCount) {
        if (done) { ++updates; terminalProgress &= done == updates && failedCount <= 1; }
        if (service.attempts.value(0) < 3 && model.getRow(0).status == -1) terminalProgress = false;
    });
    runBatch(false);
    QObject::disconnect(progressCheck);
    check(service.maximumActive == 10, "maximum concurrent images is ten");
    check(service.barriersHeld, "next batch waits for every previous image including retries");
    check(service.attempts.value(0) == 3 && model.getRow(0).status == 2, "transient failure succeeds on second automatic retry");
    check(service.attempts.value(1) == 3 && model.getRow(1).status == -1, "persistent failure stops after exactly three total attempts");
    check(updates == 25 && terminalProgress, "automatic retries do not inflate progress or failure counts");
    for (int i = 2; i < 25; ++i) check(service.attempts.value(i) == 1 && model.getRow(i).status == 2, "successful image uploads only once");
    model.removeAll(); service.reset(); service.failAll = true;
    const QString pausePath = temp.filePath("retry-pause.svg");
    QByteArray pauseFixture = valid; pauseFixture.replace("viewBox=", "data-test-id=\"0\" viewBox=");
    writeFile(pausePath, pauseFixture); model.addRow(rowFor(pausePath, 0));
    QEventLoop retryLoop;
    QObject::connect(&console, &QTextEdit::textChanged, &retryLoop, [&]() {
        if (service.attempts.value(0) == 1 && console.toPlainText().contains("自动重试 1/2")) retryLoop.quit();
    });
    dispatcher.run();
    QTimer::singleShot(5000, &retryLoop, &QEventLoop::quit); retryLoop.exec();
    check(dispatcher.isRunning() && model.getRow(0).status == 1, "waiting automatic retry remains processing");
    dispatcher.quit();
    QEventLoop pausedLoop; QTimer::singleShot(1200, &pausedLoop, &QEventLoop::quit); pausedLoop.exec();
    check(service.attempts.value(0) == 1 && model.getRow(0).status == 0, "pause cancels delayed retries without resurrecting workers");
    service.failAll = false; service.batching = false;
    runBatch(false);
    check(model.getRow(0).status == 2, "paused retry can resume in a fresh run");
    model.removeAll();
    config.tinyReqMode = TinyReqMode::Key; config.key = "test-key"; config.mail.clear(); Config(nullptr).set(config);
    const QString apiPath = temp.filePath("api.jpg"); writeFile(apiPath, "original-jpg"); model.addRow(rowFor(apiPath, 0));
    runBatch(false);
    check(model.getRow(0).status == 2, "API Location response completes without an account");
    check(service.apiUploadAuth, "API upload authenticates with api:KEY");
    check(service.apiDownloadAuth, "API Location download authenticates with api:KEY");
    check(readFile(apiPath) == "compressed-jpg", "API Location download is saved atomically");
    model.removeAll();
    const QString keySvg = temp.filePath("key-svg.svg"); writeFile(keySvg, valid); model.addRow(rowFor(keySvg, 0));
    runBatch(false);
    check(model.getRow(0).status == 2 && service.noWebAuth, "SVG uses website interface even when API KEY mode is selected");
    for (const QString& suffix : {QString("AVIF"), QString("JxL")}) {
        model.removeAll(); service.reset();
        config.tinyReqMode = TinyReqMode::Web; Config(nullptr).set(config);
        const QString raster = temp.filePath("new-format." + suffix);
        const QByteArray format = suffix.toLower().toLatin1();
        writeFile(raster, "fixture-" + format + "-source"); model.addRow(rowFor(raster, 0));
        runBatch(false);
        check(model.getRow(0).status == 2 && readFile(raster) == "compressed-" + format, "new raster format completes website upload process and download");
        check(service.webMimeTypes == QStringList({"image/" + suffix.toLower()}) && service.noWebAuth, "new formats send their own MIME without API credentials");
        model.removeAll(); service.reset();
        config.tinyReqMode = TinyReqMode::Key; Config(nullptr).set(config);
        writeFile(raster, "fixture-" + format + "-source"); model.addRow(rowFor(raster, 0));
        runBatch(false);
        if (suffix == "AVIF") {
            check(model.getRow(0).status == 2 && service.webMimeTypes.isEmpty() && service.apiUploadAuth && service.apiDownloadAuth, "AVIF uses authenticated developer API in KEY mode");
        } else {
            check(model.getRow(0).status == 2 && service.webMimeTypes == QStringList({"image/jxl"}) && service.noWebAuth, "JXL keeps using website in KEY mode");
        }
    }
    model.removeAll(); service.reset(); console.clear();
    config.tinyReqMode = TinyReqMode::Key; Config(nullptr).set(config);
    const QString enlarged = temp.filePath("enlarged-after-import.JXL");
    writeFile(enlarged, "fixture-jxl-source"); model.addRow(rowFor(enlarged, 0));
    QFile enlargedFile(enlarged);
    check(enlargedFile.open(QIODevice::ReadWrite) && enlargedFile.resize(Utility::webMaxFileSize() + 1), "queued JXL grows beyond website limit after import");
    enlargedFile.close(); runBatch(false);
    check(model.getRow(0).status == -1 && service.requests == 0, "upload guard rejects enlarged website image before any network call");
    check(QFileInfo(enlarged).size() == Utility::webMaxFileSize() + 1 && !console.toPlainText().contains("自动重试 1/2"), "permanent size rejection preserves original and avoids automatic retries");
    model.removeAll(); service.reset(); console.clear();
    config.tinyReqMode = TinyReqMode::Web; Config(nullptr).set(config);
    const QString boundary = temp.filePath("exact-five-mb.JXL");
    writeFile(boundary, "fixture-jxl-source");
    QFile boundaryFile(boundary);
    check(boundaryFile.open(QIODevice::ReadWrite) && boundaryFile.resize(Utility::webMaxFileSize()), "exact website size boundary created");
    boundaryFile.close(); model.addRow(rowFor(boundary, 0)); runBatch(false);
    check(model.getRow(0).status == 2 && service.uploaded == 1, "website upload accepts exactly five MB");
    model.removeAll(); service.reset(); console.clear(); service.outputVariant = "too-large";
    writeFile(boundary, "fixture-jxl-source"); model.addRow(rowFor(boundary, 0)); runBatch(false);
    check(model.getRow(0).status == -1 && service.requests == 1 && !console.toPlainText().contains("自动重试 1/2"), "HTTP 413 is terminal without repeated uploads");
    check(readFile(boundary) == "fixture-jxl-source", "HTTP 413 preserves original");
    service.outputVariant.clear();
    model.removeAll(); service.reset(); console.clear(); service.batching = true;
    config.autoRetry = false; Config(nullptr).set(config);
    const QString noRetry = temp.filePath("no-retry.svg");
    QByteArray noRetryFixture = valid; noRetryFixture.replace("viewBox=", "data-test-id=\"0\" viewBox=");
    writeFile(noRetry, noRetryFixture); model.addRow(rowFor(noRetry, 0)); runBatch(false);
    check(model.getRow(0).status == -1 && service.attempts.value(0) == 1 && !console.toPlainText().contains("自动重试 1/2"), "disabled automatic retry makes exactly one attempt");
    config.autoRetry = true; Config(nullptr).set(config); service.reset(); runBatch(true);
    check(model.getRow(0).status == 2 && service.attempts.value(0) == 3, "enabling retry takes effect in next batch and succeeds after two retries");
    service.batching = false;
    model.removeAll(); service.reset(); console.clear(); service.transientAll = true;
    for (int i = 0; i < 3; ++i) {
        const QString path = temp.filePath(QString("throttle-%1.svg").arg(i));
        QByteArray fixture = valid; fixture.replace("viewBox=", "data-test-id=\"" + QByteArray::number(i) + "\" viewBox=");
        writeFile(path, fixture); model.addRow(rowFor(path, 0));
    }
    runBatch(false);
    check(console.toPlainText().contains("已降低访问频率") && service.uploadTimes.size() == 6, "three distinct failed images enable adaptive throttling and each retries once");
    for (int i = 0; i < 3; ++i) check(model.getRow(i).status == 2 && service.attempts.value(i) == 2, "throttled retry recovers without duplicate requests");
    check(service.uploadTimes.at(3) >= 1900 && service.uploadTimes.at(4) - service.uploadTimes.at(3) >= 1900
        && service.uploadTimes.at(5) - service.uploadTimes.at(4) >= 1900, "throttled retries are spread at least two seconds apart");
    service.reset(); console.clear();
    for (int i = 0; i < 3; ++i) {
        const QString path = temp.filePath(QString("throttle-%1.svg").arg(i));
        QByteArray fixture = valid; fixture.replace("viewBox=", "data-test-id=\"" + QByteArray::number(i) + "\" viewBox=");
        writeFile(path, fixture); model.replaceRow(i, rowFor(path, 0));
    }
    QEventLoop throttleLoop;
    const auto throttleSignal = QObject::connect(&console, &QTextEdit::textChanged, &throttleLoop, [&]() {
        if (console.toPlainText().contains("已降低访问频率")) throttleLoop.quit();
    });
    dispatcher.run(); QTimer::singleShot(5000, &throttleLoop, &QEventLoop::quit); throttleLoop.exec();
    QObject::disconnect(throttleSignal);
    check(dispatcher.isRunning() && console.toPlainText().contains("已降低访问频率"), "pause fixture reaches throttled retry queue");
    dispatcher.quit(); const int requestsBeforePause = service.requests;
    QEventLoop slowPause; QTimer::singleShot(5200, &slowPause, &QEventLoop::quit); slowPause.exec();
    check(service.requests == requestsBeforePause, "pause cancels every queued throttled retry beyond the longest delay");
    for (int i = 0; i < 3; ++i) check(model.getRow(i).status == 0, "paused throttled rows return to pending");
    model.removeAll(); service.reset(); console.clear(); config.autoRetry = false; Config(nullptr).set(config);
    for (int i = 0; i < 13; ++i) {
        const QString path = temp.filePath(QString("throttle-batch-%1.svg").arg(i));
        QByteArray fixture = valid; fixture.replace("viewBox=", "data-test-id=\"" + QByteArray::number(i) + "\" viewBox=");
        writeFile(path, fixture); model.addRow(rowFor(path, 0));
    }
    runBatch(false);
    check(service.uploadTimes.size() == 13 && service.uploadTimes.at(10) - service.uploadTimes.at(9) >= 1900,
        "many failures delay the next batch even when automatic retry is disabled");
    check(console.toPlainText().contains("第 2 批 · 3 张并发"), "subsequent throttled batch reduces concurrency from ten to three");
    for (int i = 0; i < 13; ++i) check(service.attempts.value(i) == 1, "throttling respects disabled automatic retry");
    service.transientAll = false; config.autoRetry = true;
    config.tinyReqMode = TinyReqMode::Web; config.key.clear(); Config(nullptr).set(config);
    for (const QString& variant : {QString("invalid-url"), QString("truncated")}) {
        model.removeAll(); service.outputVariant = variant;
        const QString unsafe = temp.filePath(variant + ".svg"); writeFile(unsafe, valid); model.addRow(rowFor(unsafe, 0));
        runBatch(false);
        check(model.getRow(0).status == -1 && readFile(unsafe) == valid, "invalid output URL or truncated download preserves original after retries");
    }
    service.outputVariant.clear();
    writeFile(temp.filePath("UPPER.SVG"), valid);
    writeFile(temp.filePath("supported.AVIF"), "fixture-avif-source");
    writeFile(temp.filePath("supported.JXL"), "fixture-jxl-source");
    writeFile(temp.filePath("unsupported.gif"), "fixture");
    Scanner scanner;
    QList<TableModelRow> scanned;
    QEventLoop scanLoop;
    QObject::connect(&scanner, &Scanner::rowFound, &scanLoop, [&](const TableModelRow& row) { scanned.append(row); }, Qt::QueuedConnection);
    QObject::connect(&scanner, &QThread::finished, &scanLoop, &QEventLoop::quit);
    QTimer::singleShot(10000, &scanLoop, &QEventLoop::quit);
    scanner.start(QStringList() << temp.path(), 0, config.maxsize);
    scanLoop.exec();
    scanner.stop();
    bool uppercase = false, avif = false, jxl = false, unsupported = false;
    for (const auto& row : scanned) {
        uppercase |= row.path.endsWith("UPPER.SVG");
        avif |= row.path.endsWith("supported.AVIF");
        jxl |= row.path.endsWith("supported.JXL");
        unsupported |= row.path.endsWith("unsupported.gif");
        check(row.path.startsWith('/'), "folder paths join correctly with source and output roots");
    }
    check(uppercase && avif && jxl && !unsupported, "folder scanning accepts SVG AVIF and JXL and excludes GIF");

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
    auto scanPaths = [&](const QStringList& paths, int minBytes, int maxBytes, TinyReqMode mode = TinyReqMode::Web) {
        ScanResult result;
        Scanner reader;
        QEventLoop loop;
        bool completed = false;
        QObject::connect(&reader, &Scanner::rowFound, &loop, [&](const TableModelRow& row) { result.rows.append(row); }, Qt::QueuedConnection);
        QObject::connect(&reader, &Scanner::fileSkipped, &loop, [&](const QString& path, const QString& reason) { result.skipped.insert(path, reason); }, Qt::QueuedConnection);
        QObject::connect(&reader, &QThread::finished, &loop, [&]() { completed = true; loop.quit(); });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        reader.start(paths, minBytes, maxBytes, mode);
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
    const QString oversizedSvg = importDir + "/oversized.SVG", oversizedJxl = importDir + "/oversized.JXL", oversizedAvif = importDir + "/oversized.AVIF";
    for (const QString& path : {oversizedSvg, oversizedJxl, oversizedAvif}) {
        QFile file(path);
        check(file.open(QIODevice::WriteOnly) && file.resize(Utility::webMaxFileSize() + 1), "new format oversized fixture created");
    }
    const QStringList oversizedPaths = {hugeJpg, oversizedSvg, oversizedJxl, oversizedAvif};
    const auto webLimit = scanPaths(oversizedPaths, 0, 6 * 1024 * 1024);
    check(webLimit.rows.isEmpty() && webLimit.skipped.size() == 4, "website rejects oversized images of all new and existing formats even with a larger local limit");
    const auto apiLimit = scanPaths(oversizedPaths, 0, 6 * 1024 * 1024, TinyReqMode::Key);
    check(apiLimit.rows.size() == 2 && apiLimit.skipped.contains(oversizedSvg) && apiLimit.skipped.contains(oversizedJxl), "KEY mode keeps website cap for SVG JXL while accepting larger JPG AVIF for API");
    const QString exactLimit = importDir + "/exact-limit.JXL";
    QFile exactFile(exactLimit);
    check(exactFile.open(QIODevice::WriteOnly) && exactFile.resize(Utility::webMaxFileSize()), "import boundary fixture created");
    exactFile.close();
    check(scanPaths({exactLimit}, 0, 6 * 1024 * 1024).rows.size() == 1, "import includes exactly five MB");
    config.autoRetry = false; Config(nullptr).set(config);
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
        && notice->text().contains("官网免费模式") && !notice->text().contains("可在“设置"), "oversized JPG explains fixed website limit without suggesting settings can bypass it");
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
    auto* retrySwitch = settings->findChild<QCheckBox*>("autoRetry");
    check(retrySwitch && !retrySwitch->isChecked(), "settings exposes automatic retry switch off by default");
    retrySwitch->setChecked(true);
    for (QSpinBox* spin : settings->findChildren<QSpinBox*>()) {
        if (spin->value() == 5120) spin->setValue(6144);
    }
    check(QMetaObject::invokeMethod(settings, "onClickSaveBtn", Qt::DirectConnection), "size range setting saves");
    check(Config(nullptr).get().maxsize == 6 * 1024 * 1024, "settings update the maximum import size");
    check(Config(nullptr).get().autoRetry, "retry switch persists when enabled");
    QLabel* sizeHint = window.findChild<QLabel*>("sizeRangeLabel");
    check(sizeHint && sizeHint->text().contains("5.00 MB") && sizeHint->text().contains("单张最多 5 MB"), "website hint stays at service limit when local range is increased");
    dropUrls({QUrl::fromLocalFile(hugeJpg)});
    waitForImport();
    check(window.table->model()->rowCount() == 1 && notice->isVisible() && notice->text().contains("官网免费模式"), "increasing local limit cannot bypass website limit or erase the queue");
    auto* noticeBox = window.findChild<QFrame*>("importNoticeBox");
    auto* closeNotice = window.findChild<QPushButton*>("closeNotice");
    check(noticeBox && closeNotice, "import notice has a dismissible container");
    static_cast<TableModel*>(window.table->model())->removeAll();
    window.resize(840, 740);
    dropUrls({QUrl::fromLocalFile(text)}); waitForImport(); app.processEvents();
    const int beforeClose = window.table->viewport()->height();
    check(window.table->geometry().bottom() < window.table->parentWidget()->height(), "warning cannot clip the queue inside its card");
    check(beforeClose >= 110, "empty queue retains legible space under import warning");
    closeNotice->click(); app.processEvents();
    check(!noticeBox->isVisible() && window.table->viewport()->height() > beforeClose, "closing notice releases queue space");
    const QString log = window.console->toPlainText();
    Theme::instance()->setMode(ThemeMode::Dark); app.processEvents();
    check(QApplication::palette().color(QPalette::Base).lightness() < 128, "dark mode updates native input palette");
    check(window.styleSheet().contains("#141c19") && window.table->styleSheet().contains("#1d2822"), "dark theme covers workspace and queue");
    check(window.console->toPlainText() == log, "theme switch preserves activity logs");
    Theme::instance()->setMode(ThemeMode::Light); app.processEvents();
    check(QApplication::palette().color(QPalette::Base).lightness() > 128, "light mode overrides system appearance");
    check(Config(nullptr).get().mail.isEmpty(), "no account is populated by default");
    SettingWindow* themeSettings = new SettingWindow(&window); themeSettings->show();
    check(themeSettings->findChild<QCheckBox*>("autoRetry")->isChecked(), "reopened settings remembers enabled retry");
    themeSettings->findChild<QCheckBox*>("autoRetry")->setChecked(false);
    auto* themes = themeSettings->findChild<QButtonGroup*>("themeMode");
    check(themes && themes->buttons().size() == 3, "settings exposes light dark and system modes");
    const ThemeMode savedMode = Config(nullptr).get().themeMode;
    themes->button(int(ThemeMode::Dark))->click(); app.processEvents();
    check(Theme::instance()->isDark() && window.styleSheet().contains("#141c19"), "clicking dark previews the main window before saving");
    check(themeSettings->styleSheet().contains("#141c19") && window.table->styleSheet().contains("#1d2822"), "theme preview updates settings and queue together");
    check(Config(nullptr).get().themeMode == savedMode, "preview does not persist settings before save");
    for (QSpinBox* spin : themeSettings->findChildren<QSpinBox*>()) check(spin->findChild<QLineEdit*>()->width() >= 60, "live theme preview keeps input digits visible");
    themes->button(int(ThemeMode::Light))->click(); app.processEvents();
    check(!Theme::instance()->isDark() && window.styleSheet().contains("#f4f7f5"), "clicking light previews immediately");
    themes->button(int(ThemeMode::System))->click(); app.processEvents();
    check(Theme::instance()->mode() == ThemeMode::System, "clicking follow system applies immediately");
    themes->button(int(ThemeMode::Dark))->click();
    QMetaObject::invokeMethod(themeSettings, "onClickSaveBtn", Qt::DirectConnection); app.processEvents();
    check(Config(nullptr).get().themeMode == ThemeMode::Dark && Theme::instance()->isDark(), "saving theme persists preference and applies immediately");
    check(!Config(nullptr).get().autoRetry, "retry switch persists when disabled again");
    SettingWindow* apiSettings = new SettingWindow(&window); apiSettings->show();
    for (auto* radio : apiSettings->findChildren<QRadioButton*>()) if (radio->text() == "API KEY") radio->setChecked(true);
    int credentialFields = 0;
    for (auto* field : apiSettings->findChildren<QLineEdit*>()) if (!qobject_cast<QSpinBox*>(field->parentWidget())) ++credentialFields;
    check(credentialFields == 2, "API settings only asks for KEY and optional proxy, without an account field");
    app.processEvents();
    for (QSpinBox* spin : apiSettings->findChildren<QSpinBox*>()) {
        check(spin->buttonSymbols() == QAbstractSpinBox::NoButtons, "size inputs replace native arrow controls");
        QLineEdit* edit = spin->findChild<QLineEdit*>();
        check(edit->width() >= 60 && edit->height() >= edit->fontMetrics().height(), "size input digits and suffix have a complete text rectangle in dark mode");
        edit->setText(spin->objectName() == "maxSize" ? "8192 KB" : "1 KB"); spin->interpretText();
        check(spin->value() == (spin->objectName() == "maxSize" ? 8192 : 1), "size input keeps manual numeric editing");
    }
    Theme::instance()->setMode(ThemeMode::Light); app.processEvents();
    for (QSpinBox* spin : apiSettings->findChildren<QSpinBox*>()) check(spin->findChild<QLineEdit*>()->width() >= 60, "theme change preserves size input text geometry");
    apiSettings->findChild<QLineEdit*>("apiKeyInput")->setText("test-key");
    QMetaObject::invokeMethod(apiSettings, "onClickSaveBtn", Qt::DirectConnection);
    check(Config(nullptr).get().tinyReqMode == TinyReqMode::Key && Config(nullptr).get().mail.isEmpty(), "API settings saves a personal KEY without auto-filling an account");
    for (int action = 0; action < 3; ++action) {
        SettingWindow* preview = new SettingWindow(&window); preview->show();
        preview->findChild<QRadioButton*>("themeLight")->click(); app.processEvents();
        check(!Theme::instance()->isDark(), "unsaved light preview is visible");
        if (action == 0) preview->reject();
        if (action == 1) preview->close();
        if (action == 2) {
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(preview, &escape);
        }
        app.processEvents();
        check(Theme::instance()->isDark() && Config(nullptr).get().themeMode == ThemeMode::Dark, "cancel close and Escape restore the saved theme");
    }
    QMetaObject::invokeMethod(&window, "onOpenSettingWindow", Qt::DirectConnection);
    QMetaObject::invokeMethod(&window, "onOpenSettingWindow", Qt::DirectConnection);
    int visibleSettings = 0;
    for (SettingWindow* current : window.findChildren<SettingWindow*>()) if (current->isVisible()) ++visibleSettings;
    check(visibleSettings == 1, "only one settings preview can be open at a time");
    for (SettingWindow* current : window.findChildren<SettingWindow*>()) if (current->isVisible()) current->reject();
    Theme::instance()->setMode(ThemeMode::System);
    check(Theme::instance()->mode() == ThemeMode::System, "follow system mode can be restored");
    std::cout << "Passed " << checks << " integration checks\n";
}
